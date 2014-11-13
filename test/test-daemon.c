#include <gio/gio.h>
#include <libsoup/soup.h>
#include <signal.h>
#include <sys/prctl.h>

#include "test-util.h"

typedef struct {
  GSubprocess *daemon;
  gchar *cache_path;
  gchar *port;
} DaemonFixture;

static void
setup_xapian_bridge_process (gpointer user_data)
{
  /* send SIGTERM when parent dies, to avoid leaving zombies around */
  prctl (PR_SET_PDEATHSIG, SIGTERM);
}

static void
setup (DaemonFixture *fixture,
       gconstpointer user_data)
{
  gchar *daemon_path;
  const gchar *argv[2];
  GError *error = NULL;
  GSubprocessLauncher *launcher;
  GSocketClient *client;
  GSocketConnection *connection;
  gint port;

  daemon_path = g_test_build_filename (G_TEST_BUILT, "xapian-bridge", NULL);
  argv[0] = daemon_path;
  argv[1] = NULL;

  fixture->cache_path = g_dir_make_tmp ("xb-daemon-test-XXXXXX", &error);
  g_assert_no_error (error);

  port = g_random_int_range (12500, 13000);
  fixture->port = g_strdup_printf ("%d", port);

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);
  g_subprocess_launcher_set_child_setup (launcher, setup_xapian_bridge_process, NULL, NULL);
  g_subprocess_launcher_setenv (launcher, "XB_DATABASE_CACHE_DIR", fixture->cache_path, TRUE);
  g_subprocess_launcher_setenv (launcher, "XB_PORT", fixture->port, TRUE);

  fixture->daemon = g_subprocess_launcher_spawnv (launcher, argv, &error);
  g_assert_no_error (error);

  /* wait until the server starts listening */
  client = g_socket_client_new ();
  while ((connection = g_socket_client_connect_to_host (client, "localhost", port,
                                                        NULL, &error)) == NULL)
    {
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED);
      g_clear_error (&error);
    }

  g_assert_no_error (error);
  g_assert_nonnull (connection);

  g_object_unref (client);
  g_object_unref (connection);
  g_object_unref (launcher);
  g_free (daemon_path);
}

static void
teardown (DaemonFixture *fixture,
          gconstpointer user_data)
{
  GError *error = NULL;

  g_subprocess_send_signal (fixture->daemon, SIGTERM);

  g_subprocess_wait (fixture->daemon, NULL, &error);
  g_assert_no_error (error);

  g_clear_object (&fixture->daemon);

  test_clear_dir (fixture->cache_path);
  g_free (fixture->cache_path);
  g_free (fixture->port);
}

static gchar *
build_request_uri (DaemonFixture *fixture,
                   const gchar *endpoint,
                   const gchar *query)
{
  if (query != NULL)
    return g_strdup_printf ("http://localhost:%s/%s?%s",
                            fixture->port, endpoint, query);
  else
    return g_strdup_printf ("http://localhost:%s/%s",
                            fixture->port, endpoint);
}

static GString *
flush_stream_to_string (GInputStream *stream)
{
  GError *error = NULL;
  gsize bytes_read;
  GString *reply;
  char buf[4096];

  reply = g_string_new (NULL);

  while (g_input_stream_read_all (stream, buf, 4096, &bytes_read, NULL, &error))
    {
      g_assert_no_error (error);
      if (bytes_read == 0)
        break;

      g_string_append_len (reply, buf, bytes_read);
    }

  return reply;
}

static SoupMessage *
delete_db (DaemonFixture *fixture,
           const gchar *name)
{
  SoupSession *session;
  SoupRequestHTTP *req;
  GInputStream *stream;
  GError *error = NULL;
  GString *reply;
  SoupMessage *message;
  gchar *req_uri;

  session = soup_session_new ();
  req_uri = build_request_uri (fixture, name, NULL);
  req = soup_session_request_http (session, SOUP_METHOD_DELETE, req_uri,
                                   &error);
  g_assert_no_error (error);
  g_free (req_uri);

  stream = soup_request_send (SOUP_REQUEST (req), NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (stream);

  reply = flush_stream_to_string (stream);
  g_assert_cmpint (reply->len, ==, 0);

  message = soup_request_http_get_message (req);

  g_string_free (reply, TRUE);
  g_object_unref (req);
  g_object_unref (session);

  return message;
}

static SoupMessage *
create_db (DaemonFixture *fixture,
           const gchar *name,
           const gchar *query)
{
  SoupSession *session;
  SoupRequestHTTP *req;
  GInputStream *stream;
  GError *error = NULL;
  GString *reply;
  SoupMessage *message;
  gchar *req_uri;

  if (query != NULL)
    req_uri = build_request_uri (fixture, name, query);
  else
    req_uri = build_request_uri (fixture, name, NULL);

  session = soup_session_new ();
  req = soup_session_request_http (session, SOUP_METHOD_PUT, req_uri,
                                   &error);
  g_assert_no_error (error);
  g_free (req_uri);

  stream = soup_request_send (SOUP_REQUEST (req), NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (stream);

  reply = flush_stream_to_string (stream);
  g_assert_cmpint (reply->len, ==, 0);

  message = soup_request_http_get_message (req);

  g_string_free (reply, TRUE);
  g_object_unref (req);
  g_object_unref (session);

  return message;
}

static void
create_db_and_assert_ok (DaemonFixture *fixture,
                         const gchar *name,
                         const gchar *query)
{
  SoupMessage *message;

  message = create_db (fixture, name, query);
  g_assert_cmpint (message->status_code, ==, 200);

  g_object_unref (message);
}

static void
assert_db_does_not_exist (DaemonFixture *fixture,
                          const gchar *name)
{
  SoupSession *session;
  SoupRequestHTTP *req;
  GInputStream *stream;
  GError *error = NULL;
  GString *reply;
  SoupMessage *message;
  gchar *req_uri;

  req_uri = build_request_uri (fixture, name, NULL);

  session = soup_session_new ();
  req = soup_session_request_http (session, SOUP_METHOD_GET,
                                   req_uri,
                                   &error);
  g_assert_no_error (error);
  g_free (req_uri);

  stream = soup_request_send (SOUP_REQUEST (req), NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (stream);

  reply = flush_stream_to_string (stream);
  g_assert_cmpint (reply->len, ==, 0);

  message = soup_request_http_get_message (req);
  g_assert_cmpint (message->status_code, ==, 404);

  g_string_free (reply, TRUE);
  g_object_unref (req);
  g_object_unref (session);
  g_object_unref (message);
}

static void
assert_db_exists (DaemonFixture *fixture,
                  const gchar *name)
{
  SoupSession *session;
  SoupRequestHTTP *req;
  GInputStream *stream;
  GError *error = NULL;
  GString *reply;
  SoupMessage *message;
  gchar *req_uri;

  req_uri = build_request_uri (fixture, name, NULL);

  session = soup_session_new ();
  req = soup_session_request_http (session, SOUP_METHOD_GET,
                                   req_uri,
                                   &error);
  g_assert_no_error (error);
  g_free (req_uri);

  stream = soup_request_send (SOUP_REQUEST (req), NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (stream);

  reply = flush_stream_to_string (stream);
  g_assert_cmpint (reply->len, ==, 0);

  message = soup_request_http_get_message (req);
  g_assert_cmpint (message->status_code, ==, 200);

  g_string_free (reply, TRUE);
  g_object_unref (req);
  g_object_unref (session);
  g_object_unref (message);
}

static gchar *
get_sample_db_query (void)
{
  gchar *path, *retval;

  path = test_get_sample_db_path ();
  retval = g_strdup_printf ("path=%s", path);
  g_free (path);

  return retval;
}

static gchar *
get_lang_db_query (const gchar *lang)
{
  gchar *path, *retval;

  path = test_get_sample_db_path ();
  retval = g_strdup_printf ("path=%s&lang=%s", path, lang);
  g_free (path);

  return retval;
}

static void
test_delete_meta_is_not_allowed (DaemonFixture *fixture,
                                 gconstpointer user_data)
{
  SoupMessage *msg;
  SoupMessageHeaders *headers;
  const gchar *allow;

  assert_db_exists (fixture, "_all");

  msg = delete_db (fixture, "_all");
  g_assert_cmpint (msg->status_code, ==, 405);

  headers = msg->response_headers;
  allow = soup_message_headers_get_one (headers, "Allow");
  g_assert_cmpstr (allow, ==, SOUP_METHOD_GET);

  g_object_unref (msg);
}

static void
test_delete_invalid_fails (DaemonFixture *fixture,
                           gconstpointer user_data)
{
  SoupMessage *msg;

  assert_db_does_not_exist (fixture, "invalid");

  msg = delete_db (fixture, "invalid");
  g_assert_cmpint (msg->status_code, ==, 404);

  g_object_unref (msg);
}

static void
test_delete_removes_db (DaemonFixture *fixture,
                        gconstpointer user_data)
{
  gchar *db_query;
  SoupMessage *msg;

  db_query = get_sample_db_query ();
  create_db_and_assert_ok (fixture, "test", db_query);
  g_free (db_query);

  assert_db_exists (fixture, "test");

  msg = delete_db (fixture, "test");
  g_assert_cmpint (msg->status_code, ==, 200);

  assert_db_does_not_exist (fixture, "test");

  g_object_unref (msg);
}

static void
test_put_meta_is_not_allowed (DaemonFixture *fixture,
                              gconstpointer user_data)
{
  SoupMessage *msg;
  gchar *db_query;

  db_query = get_sample_db_query ();
  msg = create_db (fixture, "_all", db_query);
  g_assert_cmpint (msg->status_code, ==, 405);

  g_free (db_query);
  g_object_unref (msg);
}

static void
test_put_no_path_fails (DaemonFixture *fixture,
                        gconstpointer user_data)
{
  SoupMessage *msg;

  msg = create_db (fixture, "test", NULL);
  g_assert_cmpint (msg->status_code, ==, 400);

  g_object_unref (msg);
}

static void
test_put_lang_creates_db (DaemonFixture *fixture,
                          gconstpointer user_data)
{
  gchar *db_query;

  db_query = get_lang_db_query ("my");
  create_db_and_assert_ok (fixture, "test", db_query);
  g_free (db_query);

  assert_db_exists (fixture, "test");
}

static void
test_put_creates_db (DaemonFixture *fixture,
                     gconstpointer user_data)
{
  gchar *db_query;

  db_query = get_sample_db_query ();
  create_db_and_assert_ok (fixture, "test", db_query);
  g_free (db_query);

  assert_db_exists (fixture, "test");
}

static void
test_get_query_returns_json (DaemonFixture *fixture,
                             gconstpointer user_data)
{
  gchar *db_query;
  SoupSession *session;
  SoupRequestHTTP *req;
  GInputStream *stream;
  GError *error = NULL;
  GString *reply;
  SoupMessage *message;
  gchar *req_uri;
  const gchar *content_type;

  db_query = get_sample_db_query ();
  create_db_and_assert_ok (fixture, "test", db_query);
  g_free (db_query);

  req_uri = build_request_uri (fixture, "test/query", "q=a&offset=0&limit=5");

  session = soup_session_new ();
  req = soup_session_request_http (session, SOUP_METHOD_GET,
                                   req_uri,
                                   &error);
  g_assert_no_error (error);
  g_free (req_uri);

  stream = soup_request_send (SOUP_REQUEST (req), NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (stream);

  reply = flush_stream_to_string (stream);
  g_assert_cmpint (reply->len, >, 0);

  message = soup_request_http_get_message (req);
  g_assert_cmpint (message->status_code, ==, 200);

  content_type = soup_message_headers_get_content_type (message->response_headers, NULL);
  g_assert_cmpstr (content_type, ==, "application/json");

  g_string_free (reply, TRUE);
  g_object_unref (req);
  g_object_unref (session);
  g_object_unref (message);
}

static void
test_get_returns_ok (DaemonFixture *fixture,
                     gconstpointer user_data)
{
  assert_db_exists (fixture, "_all");
}

static void
test_get_empty_fails (DaemonFixture *fixture,
                      gconstpointer user_data)
{
  assert_db_does_not_exist (fixture, "test");
}

static void
test_daemon_starts_successfully (DaemonFixture *fixture,
                                 gconstpointer user_data)
{
  g_assert_nonnull (fixture->daemon);
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);

#define ADD_DAEMON_TEST(path, func) \
  g_test_add ((path), DaemonFixture, NULL, setup, (func), teardown)

  ADD_DAEMON_TEST ("/daemon/starts-successfully",
                   test_daemon_starts_successfully);
  ADD_DAEMON_TEST ("/daemon/get-empty-fails",
                   test_get_empty_fails);
  ADD_DAEMON_TEST ("/daemon/get-returns-ok",
                   test_get_returns_ok);
  ADD_DAEMON_TEST ("/daemon/get-query-returns-json",
                   test_get_query_returns_json);
  ADD_DAEMON_TEST ("/daemon/put-creates-db",
                   test_put_creates_db);
  ADD_DAEMON_TEST ("/daemon/put-lang-creates-db",
                   test_put_lang_creates_db);
  ADD_DAEMON_TEST ("/daemon/put-no-path-fails",
                   test_put_no_path_fails);
  ADD_DAEMON_TEST ("/daemon/put-meta-not-allowed",
                   test_put_meta_is_not_allowed);
  ADD_DAEMON_TEST ("/daemon/delete-removes-db",
                   test_delete_removes_db);
  ADD_DAEMON_TEST ("/daemon/delete-invalid-fails",
                   test_delete_invalid_fails);
  ADD_DAEMON_TEST ("/daemon/delete-meta-not-allowed",
                   test_delete_meta_is_not_allowed);

#undef ADD_DAEMON_TEST

  return g_test_run ();
}
