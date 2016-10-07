#include "test-util.h"
#include "errno.h"

#include <glib/gstdio.h>

gchar *
test_get_sample_db_path (void)
{
  return g_test_build_filename (G_TEST_DIST,
                                "test",
                                "testdb",
                                NULL);
}

gchar *
test_get_manifest_db_path (void)
{
  return g_test_build_filename (G_TEST_DIST,
                                "test",
                                "manifest.json",
                                NULL);
}

gchar *
test_get_invalid_db_path (void)
{
  return g_test_build_filename (G_TEST_DIST,
                                "test",
                                "invalid",
                                NULL);
}

gchar *
test_get_sample_db_path_for_query (void)
{
  gchar *path, *escaped;

  path = test_get_sample_db_path ();
  escaped = g_uri_escape_string (path, NULL, FALSE);
  g_free (path);

  return escaped;
}

gchar *
test_get_invalid_db_path_for_query (void)
{
  gchar *path, *escaped;

  path = test_get_invalid_db_path ();
  escaped = g_uri_escape_string (path, NULL, FALSE);
  g_free (path);

  return escaped;
}

void
test_clear_dir (const gchar *path)
{
  GDir *dir;
  const gchar *name;
  gchar *filename;

  dir = g_dir_open (path, 0, NULL);
  if (dir == NULL)
    return;

  while ((name = g_dir_read_name (dir)) != NULL)
    {
      filename = g_build_filename (path, name, NULL);
      if (g_file_test (filename, G_FILE_TEST_IS_DIR))
        test_clear_dir (filename);

      g_remove (filename);
      g_free (filename);
    }

  g_remove (path);
  g_dir_close (dir);
}

gchar *
test_generate_json (JsonObject *object,
                    gboolean take_ownership)
{
  JsonGenerator *generator;
  JsonNode *node;
  gchar *generated;

  generator = json_generator_new ();
  node = json_node_new (JSON_NODE_OBJECT);
  if (take_ownership)
    json_node_take_object (node, object);
  else
    json_node_set_object (node, object);
  json_generator_set_root (generator, node);
  json_generator_set_pretty (generator, TRUE);

  generated = json_generator_to_data (generator, NULL);

  g_object_unref (generator);
  json_node_free (node);

  return generated;
}
