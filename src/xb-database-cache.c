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

#include <gio/gio.h>

enum {
  PROP_0,
  PROP_CACHE_DIR,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

typedef struct {
  gchar *cache_dir;
  GFile *cache_file;
  JsonObject *json_root;
} XbDatabaseCachePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XbDatabaseCache, xb_database_cache, G_TYPE_OBJECT)

static void
xb_database_cache_finalize (GObject *object)
{
  XbDatabaseCache *self = XB_DATABASE_CACHE (object);
  XbDatabaseCachePrivate *priv = xb_database_cache_get_instance_private (self);

  g_free (priv->cache_dir);
  g_clear_object (&priv->cache_file);
  g_clear_pointer (&priv->json_root, (GDestroyNotify) json_object_unref);

  G_OBJECT_CLASS (xb_database_cache_parent_class)->finalize (object);
}

static void
xb_database_cache_set_property (GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  XbDatabaseCache *self = XB_DATABASE_CACHE (object);
  XbDatabaseCachePrivate *priv = xb_database_cache_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CACHE_DIR:
      priv->cache_dir = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
xb_database_cache_get_property (GObject *object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  XbDatabaseCache *self = XB_DATABASE_CACHE (object);

  switch (prop_id)
    {
    case PROP_CACHE_DIR:
      g_value_set_string (value, xb_database_cache_get_cache_dir (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

/* Reads the cache file from disk, or initializes it to an empty state
 * in case that fails, or when the cache is not yet present.
 */
static void
xb_database_cache_ensure (XbDatabaseCache *self)
{
  XbDatabaseCachePrivate *priv = xb_database_cache_get_instance_private (self);
  gchar *contents;
  gsize contents_length;
  JsonParser *parser;
  GError *error = NULL;

  if (priv->json_root != NULL)
    return;

  if (!g_file_load_contents (priv->cache_file, NULL, &contents, &contents_length, NULL, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        g_message ("Cannot load databases.json cache: %s. Will start a new one.",
                   error->message);

      g_clear_error (&error);
    }
  else
    {
      parser = json_parser_new ();
      json_parser_load_from_data (parser, contents, contents_length, &error);

      if (error != NULL)
        {
          g_warning ("Cannot parse databases.json cache: %s. Will start a new one.",
                     error->message);
          g_clear_error (&error);
        }
      else
        {
          priv->json_root = json_object_ref (json_node_get_object (json_parser_get_root (parser)));
        }

      g_free (contents);
      g_object_unref (parser);
    }

  if (priv->json_root == NULL)
    priv->json_root = json_object_new ();
}

static void
xb_database_cache_constructed (GObject *object)
{
  XbDatabaseCache *self = XB_DATABASE_CACHE (object);
  XbDatabaseCachePrivate *priv = xb_database_cache_get_instance_private (self);
  gchar *path;

  G_OBJECT_CLASS (xb_database_cache_parent_class)->constructed (object);

  g_assert (priv->cache_dir != NULL);
  path = g_build_filename (priv->cache_dir, "xapian-glib", NULL);
  g_mkdir_with_parents (path, 0744);
  g_free (path);

  path = g_build_filename (priv->cache_dir, "xapian-glib", "databases.json", NULL);
  priv->cache_file = g_file_new_for_path (path);
  g_free (path);

  xb_database_cache_ensure (self);
}

static void
xb_database_cache_class_init (XbDatabaseCacheClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  const gchar *cache_dir_env;

  gobject_class->constructed = xb_database_cache_constructed;
  gobject_class->finalize = xb_database_cache_finalize;
  gobject_class->set_property = xb_database_cache_set_property;
  gobject_class->get_property = xb_database_cache_get_property;

  cache_dir_env = g_getenv ("XB_DATABASE_CACHE_DIR");

  properties[PROP_CACHE_DIR] =
    g_param_spec_string ("cache-dir",
                         "Cache directory",
                         "Directory in which the database cache should live",
                         (cache_dir_env != NULL) ? cache_dir_env : LOCALSTATEDIR "/cache",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
xb_database_cache_init (XbDatabaseCache *self)
{
}

/* Returns a JSON object whose keys are database names, and values
 * are JSON objects with the following members:
 *   - path: the filesystem path of the database
 *   - lang: the primary language of the database
 *
 * Does not return a reference.
 */
JsonObject *
xb_database_cache_get_entries (XbDatabaseCache *self)
{
  XbDatabaseCachePrivate *priv = xb_database_cache_get_instance_private (self);
  return priv->json_root;
}

/* Removes the entry object for the passed-in name, and writes to disk */
void
xb_database_cache_remove_entry (XbDatabaseCache *self,
                                const gchar *name)
{
  JsonObject *object;

  object = xb_database_cache_get_entries (self);
  json_object_remove_member (object, name);
  xb_database_cache_save (self);
}

/* Stores the path and lang for the passed-in name, and writes to disk */
void
xb_database_cache_set_entry (XbDatabaseCache *self,
                             const gchar *name,
                             const gchar *path,
                             const gchar *language)
{
  JsonObject *object;
  JsonObject *entry;

  object = xb_database_cache_get_entries (self);
  if (json_object_has_member (object, name))
    {
      entry = json_object_get_object_member (object, name);
    }
  else
    {
      entry = json_object_new ();
      json_object_set_object_member (object, name, entry);
    }

  json_object_set_string_member (entry, "path", path);
  if (language != NULL)
    json_object_set_string_member (entry, "lang", language);

  xb_database_cache_save (self);
}

/* Writes the entry set to disk, overwriting any existing data */
void
xb_database_cache_save (XbDatabaseCache *self)
{
  XbDatabaseCachePrivate *priv = xb_database_cache_get_instance_private (self);
  JsonGenerator *generator;
  gchar *contents;
  gsize contents_len;
  GError *error = NULL;
  JsonNode *node;

  generator = json_generator_new ();

  node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (node, priv->json_root);
  json_generator_set_root (generator, node);
  json_node_free (node);

  contents = json_generator_to_data (generator, &contents_len);

  g_file_replace_contents (priv->cache_file, contents, contents_len,
                           NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION,
                           NULL, NULL, &error);

  if (error != NULL)
    {
      g_warning ("Unable to save database.json cache: %s.",
                 error->message);
      g_clear_error (&error);
    }

  g_free (contents);
  g_object_unref (generator);
}

XbDatabaseCache *
xb_database_cache_new (void)
{
  return g_object_new (XB_TYPE_DATABASE_CACHE, NULL);
}

XbDatabaseCache *
xb_database_cache_new_with_cache_dir (const gchar *cache_dir)
{
  return g_object_new (XB_TYPE_DATABASE_CACHE,
                       "cache-dir", cache_dir,
                       NULL);
}

const gchar *
xb_database_cache_get_cache_dir (XbDatabaseCache *self)
{
    XbDatabaseCachePrivate *priv = xb_database_cache_get_instance_private (self);
    return priv->cache_dir;
}
