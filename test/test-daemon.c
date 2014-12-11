#include <gio/gio.h>
#include <libsoup/soup.h>
#include <signal.h>
#include <sys/prctl.h>

#include "test-util.h"

typedef struct {
  GSubprocess *daemon;
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

  port = g_random_int_range (12500, 13000);
  fixture->port = g_strdup_printf ("%d", port);

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);
  g_subprocess_launcher_set_child_setup (launcher, setup_xapian_bridge_process, NULL, NULL);
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

  g_free (fixture->port);
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

static void
test_get_query_returns_json (DaemonFixture *fixture,
                             gconstpointer user_data)
{
  gchar *db_path;
  SoupSession *session;
  SoupRequestHTTP *req;
  GInputStream *stream;
  GError *error = NULL;
  GString *reply;
  SoupMessage *message;
  gchar *req_uri;
  const gchar *content_type;

  db_path = test_get_sample_db_path_for_query ();
  req_uri = g_strdup_printf ("http://localhost:%s/query?path=%s&q=a&offset=0&limit=5",
			     fixture->port, db_path);
  g_free (db_path);

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
  ADD_DAEMON_TEST ("/daemon/get-query-returns-json",
                   test_get_query_returns_json);

#undef ADD_DAEMON_TEST

  return g_test_run ();
}
