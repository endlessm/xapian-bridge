#include "xb-router.h"

typedef struct {
  XbRouter *router;
} RouterFixture;

static void
teardown (RouterFixture *fixture,
          gconstpointer user_data)
{
  g_clear_object (&fixture->router);
}

static void
setup (RouterFixture *fixture,
       gconstpointer user_data)
{
  fixture->router = xb_router_new ();
}

static void
set_boolean_signal_callback (XbRouter *router,
                             gpointer user_data)
{
  gboolean *called = user_data;
  *called = TRUE;
}

static void
set_boolean_callback (GHashTable *params,
                      GHashTable *query,
                      SoupMessage *message,
                      gpointer user_data)
{
  gboolean *called = user_data;
  *called = TRUE;
}


static void
calls_handler_with_dict_callback (GHashTable *params,
                                  GHashTable *query,
                                  SoupMessage *message,
                                  gpointer user_data)
{
  const gchar *thing, *thang;

  thing = g_hash_table_lookup (params, "thing");
  g_assert_cmpstr (thing, ==, "yr");

  thang = g_hash_table_lookup (params, "thang");
  g_assert_cmpstr (thang, ==, "stomp");
}

static void
test_calls_handler_with_dict (RouterFixture *fixture,
                              gconstpointer user_data)
{
  xb_router_add_route (fixture->router, SOUP_METHOD_GET, "/bron/:thing/aur/:thang",
                       calls_handler_with_dict_callback, NULL);
  xb_router_handle_route (fixture->router, SOUP_METHOD_GET, "/bron/yr/aur/stomp",
                          NULL, NULL);
}

static void
increment_count_callback (GHashTable *params,
                          GHashTable *query,
                          SoupMessage *message,
                          gpointer user_data)
{
  gint *count = user_data;
  (*count)++;
}

static void
test_match_not_only_suffix (RouterFixture *fixture,
                            gconstpointer user_data)
{
  gboolean thing_called = FALSE;

  xb_router_add_route (fixture->router, SOUP_METHOD_GET, "/:anything",
                       set_boolean_callback, &thing_called);
  xb_router_handle_route (fixture->router, SOUP_METHOD_GET, "/foo/bar",
                          NULL, NULL);

  g_assert_false (thing_called);
}

static void
test_match_complex_paths (RouterFixture *fixture,
                          gconstpointer user_data)
{
  gint thing_calls = 0;
  gint aur_calls = 0;
  gint thang_calls = 0;

  xb_router_add_route (fixture->router, SOUP_METHOD_GET, "/bron/:thing",
                       increment_count_callback, &thing_calls);
  xb_router_add_route (fixture->router, SOUP_METHOD_GET, "/bron/:thing/aur",
                       increment_count_callback, &aur_calls);
  xb_router_add_route (fixture->router, SOUP_METHOD_GET, "/bron/:thing/aur/:thang",
                       increment_count_callback, &thang_calls);

  xb_router_handle_route (fixture->router, SOUP_METHOD_GET, "/bron/yr",
                          NULL, NULL);
  xb_router_handle_route (fixture->router, SOUP_METHOD_GET, "/bron/yr/aur",
                          NULL, NULL);
  xb_router_handle_route (fixture->router, SOUP_METHOD_GET, "/bron/yr/aur/stomp",
                          NULL, NULL);

  g_assert_cmpint (thing_calls, ==, 1);
  g_assert_cmpint (aur_calls, ==, 1);
  g_assert_cmpint (thang_calls, ==, 1);
}

static void
test_match_path_pattern (RouterFixture *fixture,
                         gconstpointer user_data)
{
  gint count = 0;

  xb_router_add_route (fixture->router, SOUP_METHOD_GET, "/:whatever",
                       increment_count_callback, &count);
  xb_router_handle_route (fixture->router, SOUP_METHOD_GET, "/foo",
                          NULL, NULL);
  xb_router_handle_route (fixture->router, SOUP_METHOD_GET, "/bar",
                          NULL, NULL);

  g_assert_cmpint (count, ==, 2);
}

static void
test_no_path_not_handled (RouterFixture *fixture,
                          gconstpointer user_data)
{
  gboolean foo_called = FALSE;
  gboolean signal_called = FALSE;

  xb_router_add_route (fixture->router, SOUP_METHOD_GET, "/foo",
                       set_boolean_callback, &foo_called);
  g_signal_connect (fixture->router, "path-not-handled",
                    G_CALLBACK (set_boolean_signal_callback), &signal_called);
  xb_router_handle_route (fixture->router, SOUP_METHOD_GET, "/foo",
                          NULL, NULL);

  g_assert_true (foo_called);
  g_assert_false (signal_called);
}

static void
test_different_handlers_same_route (RouterFixture *fixture,
                                    gconstpointer user_data)
{
  gboolean get_called = FALSE;
  gboolean post_called = FALSE;

  xb_router_add_route (fixture->router, SOUP_METHOD_GET, "/foo",
                       set_boolean_callback, &get_called);
  xb_router_add_route (fixture->router, SOUP_METHOD_POST, "/foo",
                       set_boolean_callback, &post_called);
  xb_router_handle_route (fixture->router, SOUP_METHOD_GET, "/foo",
                          NULL, NULL);

  g_assert_true (get_called);
  g_assert_false (post_called);

  get_called = post_called = FALSE;
  xb_router_handle_route (fixture->router, SOUP_METHOD_POST, "/foo",
                          NULL, NULL);

  g_assert_true (post_called);
  g_assert_false (get_called);
}

static void
test_router_emits_path_not_handled (RouterFixture *fixture,
                                    gconstpointer user_data)
{
  gboolean foo_called = FALSE;
  gboolean signal_called = FALSE;

  xb_router_add_route (fixture->router, SOUP_METHOD_GET, "/foo",
                       set_boolean_callback, &foo_called);
  g_signal_connect (fixture->router, "path-not-handled",
                    G_CALLBACK (set_boolean_signal_callback), &signal_called);
  xb_router_handle_route (fixture->router, SOUP_METHOD_GET, "/bar",
                          NULL, NULL);

  g_assert_true (signal_called);
  g_assert_false (foo_called);
}

static void
test_router_handles_routes (RouterFixture *fixture,
                            gconstpointer user_data)
{
  gboolean called = FALSE;

  xb_router_add_route (fixture->router, SOUP_METHOD_GET, "/foo",
                       set_boolean_callback, &called);
  xb_router_handle_route (fixture->router, SOUP_METHOD_GET, "/foo",
                          NULL, NULL);

  g_assert_true (called);
}

static void
test_router_new_succeeds (RouterFixture *fixture,
                          gconstpointer user_data)
{
  g_assert_nonnull (fixture->router);
}

int
main (int argc,
      gchar **argv)
{
  g_test_init (&argc, &argv, NULL);

#define ADD_ROUTER_TEST(path, func) \
  g_test_add ((path), RouterFixture, NULL, setup, (func), teardown)

  ADD_ROUTER_TEST ("/router/new-succeeds",
                   test_router_new_succeeds);
  ADD_ROUTER_TEST ("/router/handles-routes",
                   test_router_handles_routes);
  ADD_ROUTER_TEST ("/router/emits-path-not-handled",
                   test_router_emits_path_not_handled);
  ADD_ROUTER_TEST ("/router/allows-different-handlers-for-same-route",
                   test_different_handlers_same_route);
  ADD_ROUTER_TEST ("/router/does-not-emit-path-not-handled-when-handled",
                   test_no_path_not_handled);
  ADD_ROUTER_TEST ("/router/matches-path-pattern",
                   test_match_path_pattern);
  ADD_ROUTER_TEST ("/router/matches-complex-paths",
                   test_match_complex_paths);
  ADD_ROUTER_TEST ("/router/does-not-match-only-suffix",
                   test_match_not_only_suffix);
  ADD_ROUTER_TEST ("/router/calls-handler-with-dict",
                   test_calls_handler_with_dict);

#undef ADD_ROUTER_TEST

  return g_test_run ();
}
