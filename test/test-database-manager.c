#include "config.h"

#include "xb-database-manager.h"
#include "xb-error.h"
#include "test-util.h"

typedef struct {
  XbDatabaseManager *manager;
} DatabaseManagerFixture;

static void
teardown (DatabaseManagerFixture *fixture,
          gconstpointer user_data)
{
  g_clear_object (&fixture->manager);
}

static void
setup (DatabaseManagerFixture *fixture,
       gconstpointer user_data)
{
  fixture->manager = xb_database_manager_new ();
}

static XbDatabase
get_sample_db (void)
{
  return ((XbDatabase) { .path = test_get_sample_db_path () });
}

static gboolean
create_sample_db (DatabaseManagerFixture *fixture,
                  XbDatabase *db_out,
                  GError **error)
{
  gboolean res;
  XbDatabase db = get_sample_db ();

  res = xb_database_manager_create_db (fixture->manager, db, error);

  if (db_out)
    *db_out = db;

  return res;
}

static void
assert_json_query_object (JsonObject *object,
                          gint num_results,
                          gint offset,
                          const gchar *str)
{
  gint query_num_results, query_offset;
  const gchar *query_str;

  query_num_results = json_object_get_int_member (object, "numResults");
  g_assert_cmpint (num_results, ==, query_num_results);

  query_offset = json_object_get_int_member (object, "offset");
  g_assert_cmpint (offset, ==, query_offset);

  if (str != NULL)
    {
      query_str = json_object_get_string_member (object, "query");
      g_assert_cmpstr (str, ==, query_str);
    }
}

static void
test_query_invalid_params_fails (DatabaseManagerFixture *fixture,
                                 gconstpointer user_data)
{
  GHashTable *query;
  JsonObject *object;
  gboolean res;
  XbDatabase db;
  GError *error = NULL;

  res = create_sample_db (fixture, &db, &error);

  g_assert_true (res);
  g_assert_no_error (error);
  g_assert_nonnull (db.path);

  /* create an empty query */
  query = g_hash_table_new (g_str_hash, g_str_equal);

  object = xb_database_manager_query_db (fixture->manager, db, query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);
  
  /* create a query without limit or offset */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");

  object = xb_database_manager_query_db (fixture->manager, db, query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);

  /* create a query without offset */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");
  g_hash_table_insert (query, "limit", "5");

  object = xb_database_manager_query_db (fixture->manager, db, query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);

  /* create a query with both query string and match all param */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "limit", "5");
  g_hash_table_insert (query, "offset", "0");
  g_hash_table_insert (query, "q", "a");
  g_hash_table_insert (query, "matchAll", "1");

  object = xb_database_manager_query_db (fixture->manager, db, query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);

  /* create a query without query string or match all parameter */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "limit", "5");
  g_hash_table_insert (query, "offset", "0");

  object = xb_database_manager_query_db (fixture->manager, db, query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);
  g_free ((char *) db.path);
}

static void
test_query_invalid_db_fails (DatabaseManagerFixture *fixture,
                             gconstpointer user_data)
{
  GHashTable *query;
  JsonObject *object;
  gboolean res;
  GError *error = NULL;
  XbDatabase db = { .path = "invalid" };

  res = create_sample_db (fixture, NULL, &error);

  g_assert_true (res);
  g_assert_no_error (error);

  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");
  g_hash_table_insert (query, "limit", "5");
  g_hash_table_insert (query, "offset", "0");

  object = xb_database_manager_query_db (fixture->manager, db, query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PATH);

  g_error_free (error);
  g_hash_table_unref (query);
}

static void
test_queries_db (DatabaseManagerFixture *fixture,
                 gconstpointer user_data)
{
  GHashTable *query;
  JsonObject *object;
  gboolean res;
  XbDatabase db;
  GError *error = NULL;

  res = create_sample_db (fixture, &db, &error);

  g_assert_true (res);
  g_assert_no_error (error);
  g_assert_nonnull (db.path);

  /* Test normal query */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");
  g_hash_table_insert (query, "limit", "5");
  g_hash_table_insert (query, "offset", "0");

  object = xb_database_manager_query_db (fixture->manager, db, query, &error);

  g_assert_nonnull (object);
  g_assert_no_error (error);
  assert_json_query_object (object, 5, 0, "a");

  json_object_unref (object);
  g_hash_table_unref (query);

  /* Test match all query */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "matchAll", "1");
  g_hash_table_insert (query, "limit", "5");
  g_hash_table_insert (query, "offset", "0");

  object = xb_database_manager_query_db (fixture->manager, db, query, &error);

  g_assert_nonnull (object);
  g_assert_no_error (error);
  assert_json_query_object (object, 5, 0, NULL);

  json_object_unref (object);
  g_hash_table_unref (query);
  g_free ((char *) db.path);
}

static void
test_query_invalid_lang_succeeds (DatabaseManagerFixture *fixture,
                                  gconstpointer user_data)
{
  gboolean res;
  GError *error = NULL;
  GHashTable *query;
  XbDatabase db;
  JsonObject *object;

  res = create_sample_db (fixture, &db, &error);

  g_assert_true (res);
  g_assert_no_error (error);
  g_assert_nonnull (db.path);

  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");
  g_hash_table_insert (query, "limit", "5");
  g_hash_table_insert (query, "offset", "0");
  g_hash_table_insert (query, "lang", "asd_LOL.LMAO");

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "Cannot create XapianStem for language*");
  object = xb_database_manager_query_db (fixture->manager, db, query, &error);

  g_test_assert_expected_messages ();
  g_assert_nonnull (object);
  g_assert_no_error (error);
  assert_json_query_object (object, 5, 0, "a");

  json_object_unref (object);
  g_hash_table_unref (query);
  g_free ((char *) db.path);
}

static void
test_create_invalid_db_fails (DatabaseManagerFixture *fixture,
                              gconstpointer user_data)
{
  gboolean res;
  GError *error = NULL;

  XbDatabase db = { .path = test_get_invalid_db_path () };
  res = xb_database_manager_create_db (fixture->manager, db, &error);
  g_free ((char *) db.path);

  g_assert_false (res);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PATH);
  g_error_free (error);
}

static void
test_creates_db (DatabaseManagerFixture *fixture,
                 gconstpointer user_data)
{
  gboolean res;
  XbDatabase db;
  GError *error = NULL;

  res = create_sample_db (fixture, &db, &error);

  g_assert_true (res);
  g_assert_no_error (error);
  g_assert_nonnull (db.path);

  g_free ((char *) db.path);
}

static void
test_database_manager_new_succeeds (DatabaseManagerFixture *fixture,
                                    gconstpointer user_data)
{
  g_assert_nonnull (fixture->manager);
}

int
main (int argc,
      gchar **argv)
{
  g_test_init (&argc, &argv, NULL);

#define ADD_DBMANAGER_TEST(path, func) \
  g_test_add ((path), DatabaseManagerFixture, NULL, setup, (func), teardown)

  ADD_DBMANAGER_TEST ("/dbmanager/new-succeeds",
                      test_database_manager_new_succeeds);
  ADD_DBMANAGER_TEST ("/dbmanager/creates-db",
                      test_creates_db);
  ADD_DBMANAGER_TEST ("/dbmanager/create-invalid-db-fails",
                      test_create_invalid_db_fails);
  ADD_DBMANAGER_TEST ("/dbmanager/queries-db",
                      test_queries_db);
  ADD_DBMANAGER_TEST ("/dbmanager/query-invalid-db-fails",
                      test_query_invalid_db_fails);
  ADD_DBMANAGER_TEST ("/dbmanager/query-invalid-params-fails",
                      test_query_invalid_params_fails);
  ADD_DBMANAGER_TEST ("/dbmanager/query-invalid-lang-succeeds",
                      test_query_invalid_lang_succeeds);

#undef ADD_DBMANAGER_TEST

  return g_test_run ();
}
