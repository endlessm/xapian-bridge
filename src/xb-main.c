/* Copyright 2014  Endless Mobile
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "xb-database-cache.h"
#include "xb-database-manager.h"
#include "xb-error.h"
#include "xb-router.h"
#include "xb-routed-server.h"

#include <glib-unix.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <stdlib.h>

#define DEFAULT_PORT 3004
#define META_DB_ALL "_all"
#define MIME_JSON "application/json"
#define SYSTEMD_LISTEN_FD 3

typedef struct {
  XbRoutedServer *server;
  XbDatabaseCache *cache;
  XbDatabaseManager *manager;
  GList *meta_database_names;
  GMainLoop *loop;
  guint sigterm_id;
} XapianBridge;

/* Sets up a SoupMessage to respond */
static void
server_send_response (SoupMessage *message,
                      SoupStatus status_code,
                      GHashTable *headers,
                      JsonObject *body)
{
  SoupMessageHeaders *response_headers;
  GHashTableIter iter;
  const gchar *key, *value;
  JsonNode *node;
  JsonGenerator *generator;
  gchar *body_str;
  gsize body_len;

  soup_message_set_status (message, status_code);

  if (headers != NULL)
    {
      g_object_get (message,
                    SOUP_MESSAGE_RESPONSE_HEADERS, &response_headers,
                    NULL);

      g_hash_table_iter_init (&iter, headers);
      while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &value))
        soup_message_headers_replace (response_headers, key, value);

      soup_message_headers_free (response_headers);
    }

  if (body != NULL)
    {
      generator = json_generator_new ();
      node = json_node_new (JSON_NODE_OBJECT);
      
      json_node_set_object (node, body);
      json_generator_set_root (generator, node);

      body_str = json_generator_to_data (generator, &body_len);
      soup_message_set_response (message, MIME_JSON, SOUP_MEMORY_TAKE,
                                 body_str, body_len);

      g_object_unref (generator);
      json_node_free (node);
    }
}

/* GET /:index_name - just returns OK if database exists, NOT_FOUND otherwise
 * Returns:
 *      200 - Database at index_name exists
 *      404 - No database was found at index_name
 */
static void
server_get_index_name_callback (GHashTable *params,
                                GHashTable *query,
                                SoupMessage *message,
                                gpointer user_data)
{
  XapianBridge *xb = user_data;
  const gchar *index_name;

  index_name = g_hash_table_lookup (params, "index_name");
  g_assert (index_name != NULL);

  if (xb_database_manager_has_db (xb->manager, index_name) ||
      g_list_find_custom (xb->meta_database_names, index_name, (GCompareFunc) g_strcmp0))
    server_send_response (message, SOUP_STATUS_OK, NULL, NULL);
  else
    server_send_response (message, SOUP_STATUS_NOT_FOUND, NULL, NULL);
}

/* GET /:index_name/query - query an index
 * Returns:
 *     200 - Query was successful
 *     400 - One of the required parameters wasn't specified (e.g. limit)
 *     404 - No database was found at index_name
 */
static void
server_get_index_name_query_callback (GHashTable *params,
                                      GHashTable *query,
                                      SoupMessage *message,
                                      gpointer user_data)
{
  XapianBridge *xb = user_data;
  JsonObject *result;
  const gchar *index_name, *lang;
  GError *error = NULL;

  index_name = g_hash_table_lookup (params, "index_name");
  g_assert (index_name != NULL);

  if (g_list_find_custom (xb->meta_database_names, index_name, (GCompareFunc) g_strcmp0))
    {
      if (g_strcmp0 (index_name, META_DB_ALL) == 0)
        {
          result = xb_database_manager_query_all (xb->manager, query, &error);
        }
      else
        {
          /* index_name = "_{lang}" */
          lang = index_name++;
          result = xb_database_manager_query_lang (xb->manager, lang, query, &error);
        }
    }
  else
    {
      result = xb_database_manager_query_db (xb->manager, index_name, query, &error);
    }

  if (result != NULL)
    {
      server_send_response (message, SOUP_STATUS_OK, NULL, result);
      json_object_unref (result);
    }
  else
    {
      if (g_error_matches (error, XB_ERROR, XB_ERROR_DATABASE_NOT_FOUND))
        server_send_response (message, SOUP_STATUS_NOT_FOUND, NULL, NULL);
      else if (g_error_matches (error, XB_ERROR, XB_ERROR_INVALID_PARAMS))
        server_send_response (message, SOUP_STATUS_BAD_REQUEST, NULL, NULL);
      else
        server_send_response (message, SOUP_STATUS_INTERNAL_SERVER_ERROR, NULL, NULL);

      g_critical ("Unable to query database: %s", error->message);
      g_clear_error (&error);
    }
}

/* PUT /:index_name - add/update the xapian database at index_name
 * Returns:
 *      200 - Database was successfully made
 *      400 - No path was specified or lang is unsupported
 *      403 - Database creation failed because path didn't exist
 *      405 - Attempt was made to create a database at reserved index
 */
static void
server_put_index_name_callback (GHashTable *params,
                                GHashTable *query,
                                SoupMessage *message,
                                gpointer user_data)
{
  XapianBridge *xb = user_data;
  const gchar *index_name;
  const gchar *path, *lang;
  GHashTable *headers;
  GError *error = NULL;
  gchar *lang_index_name;

  index_name = g_hash_table_lookup (params, "index_name");
  g_assert (index_name != NULL);

  if (query == NULL)
    {
      server_send_response (message, SOUP_STATUS_BAD_REQUEST, NULL, NULL);
      return;
    }

  path = g_hash_table_lookup (query, "path");
  if (path == NULL)
    {
      server_send_response (message, SOUP_STATUS_BAD_REQUEST, NULL, NULL);
      return;
    }

  if (g_list_find_custom (xb->meta_database_names, index_name, (GCompareFunc) g_strcmp0))
    {
      headers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
      g_hash_table_insert (headers, g_strdup ("Allow"), g_strdup ("GET"));

      server_send_response (message, SOUP_STATUS_METHOD_NOT_ALLOWED, headers, NULL);
      g_hash_table_unref (headers);
      return;
    }

  lang = g_hash_table_lookup (query, "lang");
  if (lang == NULL)
    lang = "none";

  /* Create the XapianDatabase and add it to the manager */
  xb_database_manager_create_db (xb->manager, index_name, path, lang, &error);
  if (error != NULL)
    {
      if (g_error_matches (error, XB_ERROR, XB_ERROR_INVALID_PATH) ||
          g_error_matches (error, XB_ERROR, XB_ERROR_UNSUPPORTED_LANG))
        {
          server_send_response (message, SOUP_STATUS_FORBIDDEN, NULL, NULL);
        }
      else
        {
          server_send_response (message, SOUP_STATUS_INTERNAL_SERVER_ERROR, NULL, NULL);
          g_critical ("Unable to create database: %s", error->message);
        }

      g_error_free (error);
      return;
    }

  /* Add index_name and path to the cache */
  xb_database_cache_set_entry (xb->cache, index_name, path, lang);

  /* Since a database now exists for lang, ensure a meta database name
   * for that language also exists.
   */
  lang_index_name = g_strconcat ("_", lang, NULL);
  if (!g_list_find_custom (xb->meta_database_names, lang_index_name, (GCompareFunc) g_strcmp0))
    xb->meta_database_names = g_list_prepend (xb->meta_database_names, lang_index_name);

  server_send_response (message, SOUP_STATUS_OK, NULL, NULL);
}

/* DELETE /:index_name - remove the xapian database at index_name
 * Returns:
 *      200 - Database was successfully deleted
 *      404 - No database existed at index_name
 *      405 - Attempt was made to delete a reserved index, like "_all"
 */
static void
server_delete_index_name_callback (GHashTable *params,
                                   GHashTable *query,
                                   SoupMessage *message,
                                   gpointer user_data)
{
  XapianBridge *xb = user_data;
  GHashTable *headers;
  const gchar *index_name;
  GError *error = NULL;

  index_name = g_hash_table_lookup (params, "index_name");
  g_assert (index_name != NULL);

  if (g_list_find_custom (xb->meta_database_names, index_name, (GCompareFunc) g_strcmp0))
    {
      headers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
      g_hash_table_insert (headers, g_strdup ("Allow"), g_strdup ("GET"));

      server_send_response (message, SOUP_STATUS_METHOD_NOT_ALLOWED, headers, NULL);
      g_hash_table_unref (headers);
      return;
    }

  /* Remove the database from the manager, so it can't be queried */
  if (!xb_database_manager_remove_db (xb->manager, index_name, &error))
    {
      g_warning ("Cannot remove database %s: %s", index_name, error->message);
      g_error_free (error);

      server_send_response (message, SOUP_STATUS_NOT_FOUND, NULL, NULL);
      return;
    }

  /* Remove the index_name and path from the cache */
  xb_database_cache_remove_entry (xb->cache, index_name);
  server_send_response (message, SOUP_STATUS_OK, NULL, NULL);
}

static void
initialize_entries_from_cache (XbDatabaseCache *cache,
                               XbDatabaseManager *manager)
{
  const gchar *entry_name, *entry_path, *entry_lang;
  JsonObject *cache_entries, *entry;
  GList *entries, *l;
  GError *error = NULL;

  /* Load the cached database info and attempt to add the databases
   * found this way.
   */
  cache_entries = xb_database_cache_get_entries (cache);
  entries = json_object_get_members (cache_entries);
  for (l = entries; l != NULL; l = l->next)
    {
      entry_name = l->data;
      entry = json_object_get_object_member (cache_entries, entry_name);
      if (entry == NULL)
        continue;

      entry_path = json_object_get_string_member (entry, "path");
      entry_lang = json_object_get_string_member (entry, "lang");

      xb_database_manager_create_db (manager, entry_name,
                                     entry_path, entry_lang,
                                     &error);

      if (error != NULL)
        {
          g_warning ("Can't create database from cache: %s",
                     error->message);
          g_clear_error (&error);

          /* Error creating the entry in the cache, so delete it */
          xb_database_cache_remove_entry (cache, entry_name);
        }
    }

  g_list_free (entries);
}

static gboolean
sigterm_handler (gpointer user_data)
{
  XapianBridge *xb = user_data;
  xb->sigterm_id = 0;
  g_main_loop_quit (xb->loop);
  return FALSE;
}

static void
xapian_bridge_free (XapianBridge *xb)
{
  g_clear_object (&xb->cache);
  g_clear_object (&xb->manager);
  g_clear_object (&xb->server);
  g_clear_pointer (&xb->loop, g_main_loop_unref);
  g_list_free_full (xb->meta_database_names, g_free);

  if (xb->sigterm_id > 0)
    g_source_remove (xb->sigterm_id);

  g_slice_free (XapianBridge, xb);
}

static XapianBridge *
xapian_bridge_new (GError **error_out)
{
  XapianBridge *xb;
  XbRoutedServer *server;
  const gchar *pid_string, *fd_string;
  const gchar *port_string;
  guint port;
  GError *error = NULL;

  server = xb_routed_server_new ();

  /* If this service is launched by systemd, LISTEN_PID and LISTEN_FDS will
   * be set by systemd and we should set up the server to use the fd instead
   * of a port.
   * Somewhat counterintuitively, LISTEN_FDS does not specify the actual file
   * descriptor to listen on, but will contain the number of descriptors passed by
   * systemd this way, starting from number 3 (0, 1 and 2 are respectively stdin,
   * stdout and stderr).
   * We can safely assume that we only want the first one here though, so we ignore
   * the actual contents of the variable.
   *
   * See http://www.freedesktop.org/software/systemd/man/sd_listen_fds.html
   * for further information on how this works.
   */
  pid_string = g_getenv ("LISTEN_PID");
  fd_string = g_getenv ("LISTEN_FDS");
  if (pid_string != NULL && fd_string != NULL)
    {
      soup_server_listen_fd (SOUP_SERVER (server), SYSTEMD_LISTEN_FD,
                             0, &error);
    }
  else
    {
      port_string = g_getenv ("XB_PORT");
      if (port_string != NULL)
        port = (guint) g_ascii_strtod (port_string, NULL);
      else
        port = DEFAULT_PORT;

      soup_server_listen_local (SOUP_SERVER (server), port,
                                0, &error);
    }

  if (error != NULL)
    {
      g_propagate_error (error_out, error);
      g_object_unref (server);

      return NULL;
    }

  xb = g_slice_new0 (XapianBridge);
  xb->server = server;
  xb->cache = xb_database_cache_new ();
  xb->manager = xb_database_manager_new ();
  xb->loop = g_main_loop_new (NULL, FALSE);
  xb->meta_database_names = g_list_prepend (xb->meta_database_names, g_strdup (META_DB_ALL));
  xb->sigterm_id = g_unix_signal_add (SIGTERM, sigterm_handler, xb);

  initialize_entries_from_cache (xb->cache, xb->manager);

  xb_routed_server_get (server, "/:index_name",
                        server_get_index_name_callback, xb);
  xb_routed_server_get (server, "/:index_name/query",
                        server_get_index_name_query_callback, xb);
  xb_routed_server_put (server, "/:index_name",
                        server_put_index_name_callback, xb);
  xb_routed_server_delete (server, "/:index_name",
                           server_delete_index_name_callback, xb);

  return xb;
}

int
main (gint argc,
      char **argv)
{
  GError *error = NULL;
  XapianBridge *xb;

  xb = xapian_bridge_new (&error);
  if (error != NULL)
    {
      g_critical ("Can't start Xapian bridge server: %s", error->message);
      g_error_free (error);
      return EXIT_FAILURE;
    }

  g_main_loop_run (xb->loop);
  xapian_bridge_free (xb);

  return EXIT_SUCCESS;
}
