#include "config.h"
#include "test-util.h"

#include "xb-database-cache.h"

typedef struct {
  XbDatabaseCache *database_cache;
  gchar *cache_dir;
} DatabaseCacheFixture;

static void
teardown (DatabaseCacheFixture *fixture,
          gconstpointer user_data)
{
  g_clear_object (&fixture->database_cache);

  test_clear_dir (fixture->cache_dir);
  g_free (fixture->cache_dir);
}

static void
setup (DatabaseCacheFixture *fixture,
       gconstpointer user_data)
{
  fixture->cache_dir = g_dir_make_tmp ("xb-db-cache-test-XXXXXX", NULL);
  fixture->database_cache = xb_database_cache_new_with_cache_dir (fixture->cache_dir);
}

static void
assert_same_entries (DatabaseCacheFixture *fixture,
                     const gchar *expected)
{
  JsonObject *object;
  gchar *generated;
  XbDatabaseCache *new_cache;

  object = xb_database_cache_get_entries (fixture->database_cache);
  generated = test_generate_json (object, FALSE);

  /* ensure the cache's in-memory entries are what we expect */
  g_assert_cmpstr (expected, ==, generated);
  g_free (generated);

  /* make a new cache which reads the now-written cache to init its entries */
  new_cache = xb_database_cache_new_with_cache_dir (fixture->cache_dir);

  /* ensure that the cached entries reflect the in-memory entries */
  object = xb_database_cache_get_entries (new_cache);
  generated = test_generate_json (object, FALSE);
  g_assert_cmpstr (expected, ==, generated);

  g_free (generated);
  g_object_unref (new_cache);
}

static void
test_supports_lang_parameter (DatabaseCacheFixture *fixture,
                              gconstpointer user_data)
{
  const gchar *expected;

  expected = "{\n" \
    "  \"foo\" : {\n" \
    "    \"path\" : \"/some/path\",\n" \
    "    \"lang\" : \"gallifreyan\"\n" \
    "  },\n" \
    "  \"bar\" : {\n" \
    "    \"path\" : \"/some/other/path\",\n" \
    "    \"lang\" : \"swaghili\"\n" \
    "  }\n" \
    "}";

  xb_database_cache_set_entry (fixture->database_cache, "foo", "/some/path", "gallifreyan");
  xb_database_cache_set_entry (fixture->database_cache, "bar", "/some/other/path", "swaghili");

  assert_same_entries (fixture, expected);
}

static void
test_removes (DatabaseCacheFixture *fixture,
              gconstpointer user_data)
{
  const gchar *expected;

  expected = "{\n" \
    "  \"bar\" : {\n" \
    "    \"path\" : \"/some/other/path\"\n" \
    "  }\n" \
    "}";

  xb_database_cache_set_entry (fixture->database_cache, "foo", "/some/path", NULL);
  xb_database_cache_set_entry (fixture->database_cache, "bar", "/some/other/path", NULL);
  xb_database_cache_remove_entry (fixture->database_cache, "foo");

  assert_same_entries (fixture, expected);
}

static void
test_reads_and_writes (DatabaseCacheFixture *fixture,
                       gconstpointer user_data)
{
  const gchar *expected;

  expected = "{\n" \
    "  \"foo\" : {\n" \
    "    \"path\" : \"/some/path\"\n" \
    "  },\n" \
    "  \"bar\" : {\n" \
    "    \"path\" : \"/some/other/path\"\n" \
    "  }\n" \
    "}";

  xb_database_cache_set_entry (fixture->database_cache, "foo", "/some/path", NULL);
  xb_database_cache_set_entry (fixture->database_cache, "bar", "/some/other/path", NULL);

  assert_same_entries (fixture, expected);
}

static void
test_creates_xapian_glib_dir (DatabaseCacheFixture *fixture,
                              gconstpointer user_data)
{
  gchar *path;

  path = g_build_filename (fixture->cache_dir, "xapian-glib", NULL);
  g_assert_true (g_file_test (path, G_FILE_TEST_IS_DIR));

  g_free (path);
}

static void
test_cache_dir_is_correctly_propagated (DatabaseCacheFixture *fixture,
                                        gconstpointer user_data)
{
  const gchar *cache_dir;

  cache_dir = xb_database_cache_get_cache_dir (fixture->database_cache);
  g_assert_cmpstr (cache_dir, ==, fixture->cache_dir);
}

static void
test_database_cache_new_succeeds (DatabaseCacheFixture *fixture,
                                  gconstpointer user_data)
{
  g_assert_nonnull (fixture->database_cache);
}

int
main (int argc,
      gchar **argv)
{
  g_test_init (&argc, &argv, NULL);

#define ADD_DBCACHE_TEST(path, func) \
  g_test_add ((path), DatabaseCacheFixture, NULL, setup, (func), teardown)

  ADD_DBCACHE_TEST ("/dbcache/new-succeeds",
                    test_database_cache_new_succeeds);
  ADD_DBCACHE_TEST ("/dbcache/cache-dir-is-correctly-propagated",
                    test_cache_dir_is_correctly_propagated);
  ADD_DBCACHE_TEST ("/dbcache/creates-xapian-glib-dir",
                    test_creates_xapian_glib_dir);
  ADD_DBCACHE_TEST ("/dbcache/reads-and-writes-entries",
                    test_reads_and_writes);
  ADD_DBCACHE_TEST ("/dbcache/removes-entries",
                    test_removes);
  ADD_DBCACHE_TEST ("/dbcache/supports-lang-parameter",
                    test_supports_lang_parameter);

#undef ADD_DBCACHE_TEST

  return g_test_run ();
}
