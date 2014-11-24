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

static XapianDatabase *
create_sample_db (DatabaseManagerFixture *fixture,
                  GError **error)
{
  XapianDatabase *db;
  gchar *path;

  path = test_get_sample_db_path ();
  db = xb_database_manager_create_db (fixture->manager, "test",
                                      path, "en", error);
  g_free (path);

  return db;
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

  query_str = json_object_get_string_member (object, "query");
  g_assert_cmpstr (str, ==, query_str);
}

static void
test_query_all_invalid_params_fails (DatabaseManagerFixture *fixture,
                                     gconstpointer user_data)
{
  GHashTable *query;
  JsonObject *object;
  XapianDatabase *db;
  GError *error = NULL;

  db = create_sample_db (fixture, &error);

  g_assert_nonnull (db);
  g_assert_no_error (error);

  /* create an empty query */
  query = g_hash_table_new (g_str_hash, g_str_equal);

  object = xb_database_manager_query_all (fixture->manager, query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);

  /* create a query without limit or offset */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");

  object = xb_database_manager_query_all (fixture->manager, query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);

  /* create a query without offset */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");
  g_hash_table_insert (query, "limit", "5");

  object = xb_database_manager_query_all (fixture->manager, query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);

  /* create a query without query string */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "limit", "5");
  g_hash_table_insert (query, "offset", "0");

  object = xb_database_manager_query_all (fixture->manager, query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);
}

static void
test_queries_all (DatabaseManagerFixture *fixture,
                  gconstpointer user_data)
{
  GHashTable *query;
  JsonObject *object;
  XapianDatabase *db;
  GError *error = NULL;

  db = create_sample_db (fixture, &error);

  g_assert_nonnull (db);
  g_assert_no_error (error);

  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");
  g_hash_table_insert (query, "limit", "5");
  g_hash_table_insert (query, "offset", "0");

  object = xb_database_manager_query_all (fixture->manager, query, &error);

  g_assert_nonnull (object);
  g_assert_no_error (error);
  assert_json_query_object (object, 5, 0, "a");

  json_object_unref (object);
  g_hash_table_unref (query);
}

static void
test_query_by_lang_invalid_params_fails (DatabaseManagerFixture *fixture,
                                         gconstpointer user_data)
{
  GHashTable *query;
  JsonObject *object;
  XapianDatabase *db;
  GError *error = NULL;

  db = create_sample_db (fixture, &error);

  g_assert_nonnull (db);
  g_assert_no_error (error);

  /* create an empty query */
  query = g_hash_table_new (g_str_hash, g_str_equal);

  object = xb_database_manager_query_lang (fixture->manager, "en", query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);

  /* create a query without limit or offset */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");

  object = xb_database_manager_query_lang (fixture->manager, "en", query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);

  /* create a query without offset */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");
  g_hash_table_insert (query, "limit", "5");

  object = xb_database_manager_query_lang (fixture->manager, "en", query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);

  /* create a query without query string */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "limit", "5");
  g_hash_table_insert (query, "offset", "0");

  object = xb_database_manager_query_lang (fixture->manager, "en", query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);
}

static void
test_queries_by_lang (DatabaseManagerFixture *fixture,
                      gconstpointer user_data)
{
  GHashTable *query;
  JsonObject *object;
  XapianDatabase *db;
  GError *error = NULL;

  db = create_sample_db (fixture, &error);

  g_assert_nonnull (db);
  g_assert_no_error (error);

  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");
  g_hash_table_insert (query, "limit", "5");
  g_hash_table_insert (query, "offset", "0");

  object = xb_database_manager_query_lang (fixture->manager, "en", query, &error);

  g_assert_nonnull (object);
  g_assert_no_error (error);
  assert_json_query_object (object, 5, 0, "a");

  json_object_unref (object);
  g_hash_table_unref (query);
}

static void
test_query_invalid_params_fails (DatabaseManagerFixture *fixture,
                                 gconstpointer user_data)
{
  GHashTable *query;
  JsonObject *object;
  XapianDatabase *db;
  GError *error = NULL;

  db = create_sample_db (fixture, &error);

  g_assert_nonnull (db);
  g_assert_no_error (error);

  /* create an empty query */
  query = g_hash_table_new (g_str_hash, g_str_equal);

  object = xb_database_manager_query_db (fixture->manager, "test", query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);
  
  /* create a query without limit or offset */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");

  object = xb_database_manager_query_db (fixture->manager, "test", query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);

  /* create a query without offset */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");
  g_hash_table_insert (query, "limit", "5");

  object = xb_database_manager_query_db (fixture->manager, "test", query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);

  /* create a query without query string */
  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "limit", "5");
  g_hash_table_insert (query, "offset", "0");

  object = xb_database_manager_query_db (fixture->manager, "test", query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PARAMS);
  g_clear_error (&error);
  g_hash_table_unref (query);
}

static void
test_query_invalid_db_fails (DatabaseManagerFixture *fixture,
                             gconstpointer user_data)
{
  GHashTable *query;
  JsonObject *object;
  XapianDatabase *db;
  GError *error = NULL;

  db = create_sample_db (fixture, &error);

  g_assert_nonnull (db);
  g_assert_no_error (error);

  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");
  g_hash_table_insert (query, "limit", "5");
  g_hash_table_insert (query, "offset", "0");

  object = xb_database_manager_query_db (fixture->manager, "invalid", query, &error);

  g_assert_null (object);
  g_assert_error (error, XB_ERROR, XB_ERROR_DATABASE_NOT_FOUND);

  g_error_free (error);
  g_hash_table_unref (query);
}

static void
test_queries_db (DatabaseManagerFixture *fixture,
                 gconstpointer user_data)
{
  GHashTable *query;
  JsonObject *object;
  XapianDatabase *db;
  GError *error = NULL;

  db = create_sample_db (fixture, &error);

  g_assert_nonnull (db);
  g_assert_no_error (error);

  query = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (query, "q", "a");
  g_hash_table_insert (query, "limit", "5");
  g_hash_table_insert (query, "offset", "0");

  object = xb_database_manager_query_db (fixture->manager, "test", query, &error);

  g_assert_nonnull (object);
  g_assert_no_error (error);
  assert_json_query_object (object, 5, 0, "a");

  json_object_unref (object);
  g_hash_table_unref (query);
}

static void
test_remove_invalid_db_fails (DatabaseManagerFixture *fixture,
                              gconstpointer user_data)
{
  GError *error = NULL;
  XapianDatabase *db;
  gboolean removed;

  db = create_sample_db (fixture, &error);

  g_assert_nonnull (db);
  g_assert_no_error (error);

  removed = xb_database_manager_remove_db (fixture->manager, "invalid", &error);
  g_assert_false (removed);
  g_assert_error (error, XB_ERROR, XB_ERROR_DATABASE_NOT_FOUND);
  g_error_free (error);
}

static void
test_removes_db (DatabaseManagerFixture *fixture,
                 gconstpointer user_data)
{
  GError *error = NULL;
  XapianDatabase *db;
  gboolean removed;

  db = create_sample_db (fixture, &error);

  g_assert_nonnull (db);
  g_assert_no_error (error);

  removed = xb_database_manager_remove_db (fixture->manager, "test", &error);
  g_assert_true (removed);
  g_assert_no_error (error);
}

static void
test_has_invalid_db_fails (DatabaseManagerFixture *fixture,
                           gconstpointer user_data)
{
  gboolean has_db;

  has_db = xb_database_manager_has_db (fixture->manager, "invalid");
  g_assert_false (has_db);
}

static void
test_has_db (DatabaseManagerFixture *fixture,
             gconstpointer user_data)
{
  gboolean has_db;
  GError *error = NULL;
  XapianDatabase *db;

  db = create_sample_db (fixture, &error);

  g_assert_nonnull (db);
  g_assert_no_error (error);
  
  has_db = xb_database_manager_has_db (fixture->manager, "test");

  g_assert_true (has_db);
}

static void
test_create_invalid_db_fails (DatabaseManagerFixture *fixture,
                              gconstpointer user_data)
{
  XapianDatabase *db;
  gchar *path;
  GError *error = NULL;

  path = test_get_invalid_db_path ();
  db = xb_database_manager_create_db (fixture->manager, "test",
                                      path, "en", &error);
  g_free (path);

  g_assert_null (db);
  g_assert_error (error, XB_ERROR, XB_ERROR_INVALID_PATH);
  g_error_free (error);
}

static void
test_creates_db (DatabaseManagerFixture *fixture,
                 gconstpointer user_data)
{
  XapianDatabase *db;
  GError *error = NULL;

  db = create_sample_db (fixture, &error);

  g_assert_nonnull (db);
  g_assert_no_error (error);
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
  ADD_DBMANAGER_TEST ("/dbmanager/has-db",
                      test_has_db);
  ADD_DBMANAGER_TEST ("/dbmanager/has-invalid-db-fails",
                      test_has_invalid_db_fails);
  ADD_DBMANAGER_TEST ("/dbmanager/removes-db",
                      test_removes_db);
  ADD_DBMANAGER_TEST ("/dbmanager/remove-invalid-db-fails",
                      test_remove_invalid_db_fails);
  ADD_DBMANAGER_TEST ("/dbmanager/queries-db",
                      test_queries_db);
  ADD_DBMANAGER_TEST ("/dbmanager/query-invalid-db-fails",
                      test_query_invalid_db_fails);
  ADD_DBMANAGER_TEST ("/dbmanager/query-invalid-params-fails",
                      test_query_invalid_params_fails);
  ADD_DBMANAGER_TEST ("/dbmanager/queries-by-lang",
                      test_queries_by_lang);
  ADD_DBMANAGER_TEST ("/dbmanager/query-by-lang-invalid-params-fail",
                      test_query_by_lang_invalid_params_fails);
  ADD_DBMANAGER_TEST ("/dbmanager/queries-all",
                      test_queries_all);
  ADD_DBMANAGER_TEST ("/dbmanager/query-all-invalid-params-fail",
                      test_query_all_invalid_params_fails);

#undef ADD_DBMANAGER_TEST

  return g_test_run ();
}
