#include <json-glib/json-glib.h>

#include "xb-prefix-store.h"
#include "test-util.h"

typedef struct {
  XbPrefixStore *store;
} PrefixStoreFixture;

static JsonObject *
get_prefix_object (const gchar *field,
                   const gchar *prefix)
{
  JsonObject *object;

  object = json_object_new ();
  json_object_set_string_member (object, "field", field);
  json_object_set_string_member (object, "prefix", prefix);
  return object;
}

static void
teardown (PrefixStoreFixture *fixture,
          gconstpointer user_data)
{
  g_clear_object (&fixture->store);
}

static void
setup (PrefixStoreFixture *fixture,
       gconstpointer user_data)
{
  fixture->store = xb_prefix_store_new ();
}

static void
assert_generated_json (JsonObject *object,
                       const gchar *expected)
{
  gchar *generated;

  generated = test_generate_json (object, TRUE);

  g_assert_cmpstr (expected, ==, generated);

  g_free (generated);
}

static void
test_stores_whole_prefix_map (PrefixStoreFixture *fixture,
                              gconstpointer user_data)
{
  const gchar *json;
  JsonParser *parser;
  JsonNode *node;
  JsonObject *object;

  json =
    "{\n" \
    "  \"prefixes\" : [\n" \
    "    {\n" \
    "      \"field\" : \"wu\",\n" \
    "      \"prefix\" : \"tang\"\n" \
    "    }\n" \
    "  ],\n" \
    "  \"booleanPrefixes\" : [\n" \
    "    {\n" \
    "      \"field\" : \"foo\",\n" \
    "      \"prefix\" : \"bar\"\n" \
    "    }\n" \
    "  ]\n" \
    "}";

  parser = json_parser_new ();
  json_parser_load_from_data (parser, json, -1, NULL);
  node = json_parser_get_root (parser);
  object = json_node_get_object (node);

  xb_prefix_store_store_prefix_map (fixture->store, "en", object);
  g_object_unref (parser);

  object = xb_prefix_store_get (fixture->store, "en");
  assert_generated_json (object, json);
}

static void
test_no_duplicates_for_get_all (PrefixStoreFixture *fixture,
                                gconstpointer user_data)
{
  JsonObject *foo1, *foo2, *wu1, *wu2;
  const gchar *expected;
  JsonObject *object;

  expected =
    "{\n" \
    "  \"prefixes\" : [\n" \
    "    {\n" \
    "      \"field\" : \"foo\",\n" \
    "      \"prefix\" : \"bar\"\n" \
    "    }\n" \
    "  ],\n" \
    "  \"booleanPrefixes\" : [\n" \
    "    {\n" \
    "      \"field\" : \"wu\",\n" \
    "      \"prefix\" : \"tang\"\n" \
    "    }\n" \
    "  ]\n" \
    "}";

  foo1 = get_prefix_object ("foo", "bar");
  foo2 = get_prefix_object ("foo", "bar");
  wu1 = get_prefix_object ("wu", "tang");
  wu2 = get_prefix_object ("wu", "tang");

  xb_prefix_store_store_prefix (fixture->store, "en", foo1);
  xb_prefix_store_store_prefix (fixture->store, "es", foo2);
  xb_prefix_store_store_boolean_prefix (fixture->store, "en", wu1);
  xb_prefix_store_store_boolean_prefix (fixture->store, "es", wu2);

  object = xb_prefix_store_get_all (fixture->store);
  assert_generated_json (object, expected);

  json_object_unref (foo1);
  json_object_unref (foo2);
  json_object_unref (wu1);
  json_object_unref (wu2);
}

static void
test_unites_all_languages (PrefixStoreFixture *fixture,
                           gconstpointer user_data)
{
  JsonObject *foo, *baz, *wu, *el;
  const char *expected;
  JsonObject *object;

  expected =
    "{\n" \
    "  \"prefixes\" : [\n" \
    "    {\n" \
    "      \"field\" : \"baz\",\n" \
    "      \"prefix\" : \"bang\"\n" \
    "    },\n" \
    "    {\n" \
    "      \"field\" : \"foo\",\n" \
    "      \"prefix\" : \"bar\"\n" \
    "    }\n" \
    "  ],\n" \
    "  \"booleanPrefixes\" : [\n" \
    "    {\n" \
    "      \"field\" : \"el\",\n" \
    "      \"prefix\" : \"gato\"\n" \
    "    },\n" \
    "    {\n" \
    "      \"field\" : \"wu\",\n" \
    "      \"prefix\" : \"tang\"\n" \
    "    }\n" \
    "  ]\n" \
    "}";

  foo = get_prefix_object ("foo", "bar");
  baz = get_prefix_object ("baz", "bang");
  wu = get_prefix_object ("wu", "tang");
  el = get_prefix_object ("el", "gato");

  xb_prefix_store_store_prefix (fixture->store, "en", foo);
  xb_prefix_store_store_prefix (fixture->store, "es", baz);
  xb_prefix_store_store_boolean_prefix (fixture->store, "en", wu);
  xb_prefix_store_store_boolean_prefix (fixture->store, "es", el);

  object = xb_prefix_store_get_all (fixture->store);
  assert_generated_json (object, expected);

  json_object_unref (foo);
  json_object_unref (baz);
  json_object_unref (wu);
  json_object_unref (el);
}

static void
test_no_duplicates (PrefixStoreFixture *fixture,
                    gconstpointer user_data)
{
  JsonObject *foo;
  JsonObject *object;
  JsonArray *prefixes;

  foo = get_prefix_object ("foo", "bar");

  xb_prefix_store_store_prefix (fixture->store, "en", foo);
  xb_prefix_store_store_prefix (fixture->store, "en", foo);
  xb_prefix_store_store_prefix (fixture->store, "en", foo);

  object = xb_prefix_store_get (fixture->store, "en");
  prefixes = json_object_get_array_member (object, "prefixes");

  g_assert_cmpint (json_array_get_length (prefixes), ==, 1);

  json_object_unref (object);
  json_object_unref (foo);
}

static void
test_retrieves_by_lang (PrefixStoreFixture *fixture,
                        gconstpointer user_data)
{
  JsonObject *foo, *bar;
  const char *expected;
  JsonObject *object;

  expected = "{\n" \
    "  \"prefixes\" : [\n" \
    "    {\n" \
    "      \"field\" : \"foo\",\n" \
    "      \"prefix\" : \"bar\"\n" \
    "    }\n" \
    "  ],\n" \
    "  \"booleanPrefixes\" : [\n" \
    "    {\n" \
    "      \"field\" : \"bar\",\n" \
    "      \"prefix\" : \"baz\"\n" \
    "    }\n" \
    "  ]\n" \
    "}";

  foo = get_prefix_object ("foo", "bar");
  xb_prefix_store_store_prefix (fixture->store, "en", foo);

  bar = get_prefix_object ("bar", "baz");
  xb_prefix_store_store_boolean_prefix (fixture->store, "en", bar);

  object = xb_prefix_store_get (fixture->store, "en");
  assert_generated_json (object, expected);
}

static void
test_stores_by_lang (PrefixStoreFixture *fixture,
                     gconstpointer user_data)
{
  JsonObject *foo, *bar, *all_prefixes;
  JsonArray *prefixes, *boolean_prefixes;

  foo = get_prefix_object ("foo", "bar");
  xb_prefix_store_store_prefix (fixture->store, "en", foo);
  json_object_unref (foo);

  bar = get_prefix_object ("bar", "baz");
  xb_prefix_store_store_boolean_prefix (fixture->store, "en", bar);
  json_object_unref (bar);

  all_prefixes = xb_prefix_store_get_all (fixture->store);
  g_assert_nonnull (all_prefixes);

  prefixes = json_object_get_array_member (all_prefixes, "prefixes");
  g_assert_nonnull (prefixes);
  g_assert_cmpint (json_array_get_length (prefixes), ==, 1);

  boolean_prefixes = json_object_get_array_member (all_prefixes, "booleanPrefixes");
  g_assert_nonnull (boolean_prefixes);
  g_assert_cmpint (json_array_get_length (boolean_prefixes), ==, 1);

  json_object_unref (all_prefixes);
}

static void
test_prefix_store_new_succeeds (PrefixStoreFixture *fixture,
                                gconstpointer user_data)
{
  g_assert_nonnull (fixture->store);
}

int
main (int argc,
      gchar **argv)
{
  g_test_init (&argc, &argv, NULL);

#define ADD_PREFIX_STORE_TEST(path, func) \
  g_test_add ((path), PrefixStoreFixture, NULL, setup, (func), teardown)

  ADD_PREFIX_STORE_TEST ("/prefix-store/new-succeeds",
                         test_prefix_store_new_succeeds);
  ADD_PREFIX_STORE_TEST ("/prefix-store/stores-by-lang",
                         test_stores_by_lang);
  ADD_PREFIX_STORE_TEST ("/prefix-store/retrieves-by-lang",
                         test_retrieves_by_lang);
  ADD_PREFIX_STORE_TEST ("/prefix-store/does-not-store-duplicates",
                         test_no_duplicates);
  ADD_PREFIX_STORE_TEST ("/prefix-store/unites-all-languages-for-get-all",
                         test_unites_all_languages);
  ADD_PREFIX_STORE_TEST ("/prefix-store/does-not-return-duplicates-for-get-all",
                         test_no_duplicates_for_get_all);
  ADD_PREFIX_STORE_TEST ("/prefix-store/stores-whole-prefix-maps",
                         test_stores_whole_prefix_map);

#undef ADD_PREFIX_STORE_TEST

  return g_test_run ();
}
