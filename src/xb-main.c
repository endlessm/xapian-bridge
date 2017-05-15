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

#include "xb-database-manager.h"
#include "xb-error.h"
#include "xb-router.h"
#include "xb-routed-server.h"

#include <glib-unix.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PORT 3004
#define META_DB_ALL "_all"
#define MIME_JSON "application/json; charset=utf-8"
#define SYSTEMD_LISTEN_FD 3

typedef struct {
  XbRoutedServer *server;
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

static gboolean
fill_xbdb_from_query (SoupMessage *message,
                      GHashTable  *query,
                      XbDatabase  *db)
{
  db->path = g_hash_table_lookup (query, "path");
  db->manifest_path = g_hash_table_lookup (query, "manifest_path");

  if (db->path == NULL && db->manifest_path == NULL)
    {
      server_send_response (message, SOUP_STATUS_BAD_REQUEST, NULL, NULL);
      return FALSE;
    }

  return TRUE;
}

/* GET /query - query an index
 * Returns:
 *     200 - Query was successful
 *     400 - One of the required parameters wasn't specified (e.g. limit)
 *     404 - No database was found at index_name
 */
static void
server_get_query_callback (GHashTable *params,
                           GHashTable *query,
                           SoupMessage *message,
                           gpointer user_data)
{
  XapianBridge *xb = user_data;
  JsonObject *result;
  GError *error = NULL;
  XbDatabase db;

  if (!fill_xbdb_from_query (message, query, &db))
    return;

  result = xb_database_manager_query_db (xb->manager, db, query, &error);

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

/* GET /fix - fix a user query
 * Returns:
 *     200 - Query was successfully fixed (though no changes may have occurred)
 *     400 - One of the required parameters wasn't specified
 *     404 - No database was found at index_name
 */
static void
server_get_fix_callback (GHashTable *params,
                         GHashTable *query,
                         SoupMessage *message,
                         gpointer user_data)
{
  XapianBridge *xb = user_data;
  JsonObject *result;
  GError *error = NULL;
  XbDatabase db;

  if (!fill_xbdb_from_query (message, query, &db))
    return;

  result = xb_database_manager_fix_query (xb->manager, db, query, &error);

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

      g_critical ("Unable to fix user query: %s", error->message);
      g_clear_error (&error);
    }
}

/* GET /test - test for existence of a particular feature
 * Returns:
 *     200 - Feature supported by this instance of xapian-bridge
 *     400 - The required "feature" parameter wasn't specified
 *     404 - Feature not supported by this instance of xapian-bridge
 *
 * Note: xapian-bridge versions predating the addition of /test return 500
 * (default response to an unknown URL) so you typically want to handle 500
 * in the same way as 404.
 */
static void
server_get_test_callback (GHashTable *params,
                          GHashTable *query,
                          SoupMessage *message,
                          gpointer user_data)
{
  XapianBridge *xb = user_data;
  const char *feature;

  feature = g_hash_table_lookup (query, "feature");
  if (feature == NULL)
    {
      server_send_response (message, SOUP_STATUS_BAD_REQUEST, NULL, NULL);
      return;
    }

  if (strcmp (feature, "query-param-defaultOp") == 0)
    {
      server_send_response (message, SOUP_STATUS_OK, NULL, NULL);
    }
  else
    {
      server_send_response (message, SOUP_STATUS_NOT_FOUND, NULL, NULL);
    }
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
  xb->manager = xb_database_manager_new ();
  xb->loop = g_main_loop_new (NULL, FALSE);
  xb->meta_database_names = g_list_prepend (xb->meta_database_names, g_strdup (META_DB_ALL));
  xb->sigterm_id = g_unix_signal_add (SIGTERM, sigterm_handler, xb);

  xb_routed_server_get (server, "/query",
                        server_get_query_callback, xb);
  xb_routed_server_get (server, "/fix",
                        server_get_fix_callback, xb);
  xb_routed_server_get (server, "/test",
                        server_get_test_callback, xb);

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
