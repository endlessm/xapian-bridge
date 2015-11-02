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

#include <xapian-glib.h>

#define QUERY_PARAM_COLLAPSE_KEY "collapse"
#define QUERY_PARAM_CUTOFF "cutoff"
#define QUERY_PARAM_LIMIT "limit"
#define QUERY_PARAM_LANG "lang"
#define QUERY_PARAM_MATCH_ALL "matchAll"
#define QUERY_PARAM_OFFSET "offset"
#define QUERY_PARAM_ORDER "order"
#define QUERY_PARAM_QUERYSTR "q"
#define QUERY_PARAM_SORT_BY "sortBy"

#define QUERY_RESULTS_MEMBER_NUM_RESULTS "numResults"
#define QUERY_RESULTS_MEMBER_OFFSET "offset"
#define QUERY_RESULTS_MEMBER_QUERYSTR "query"
#define QUERY_RESULTS_MEMBER_RESULTS "results"

#define FIX_RESULTS_MEMBER_SPELL_CORRECTED_RESULT "spellCorrectedQuery"
#define FIX_RESULTS_MEMBER_STOP_WORD_CORRECTED_RESULT "stopWordCorrectedQuery"

#define PREFIX_METADATA_KEY "XbPrefixes"
#define STOPWORDS_METADATA_KEY "XbStopwords"
#define QUERY_PARSER_FLAGS XAPIAN_QUERY_PARSER_FEATURE_DEFAULT | \
  XAPIAN_QUERY_PARSER_FEATURE_WILDCARD |\
  XAPIAN_QUERY_PARSER_FEATURE_PURE_NOT |\
  XAPIAN_QUERY_PARSER_FEATURE_SPELLING_CORRECTION

typedef struct {
  XapianDatabase *db;
  XapianQueryParser *qp;
  XbDatabaseManager *manager;
  GFileMonitor *monitor;
  gchar *path;
} DatabasePayload;

static void
database_payload_free (DatabasePayload *payload)
{
  g_clear_object (&payload->db);
  g_clear_object (&payload->qp);

  g_file_monitor_cancel (payload->monitor);
  g_clear_object (&payload->monitor);
  g_free (payload->path);

  g_slice_free (DatabasePayload, payload);
}

static DatabasePayload *
database_payload_new (XapianDatabase *db,
                      XapianQueryParser *qp,
                      XbDatabaseManager *manager,
                      GFileMonitor *monitor,
                      const gchar *path)
{
  DatabasePayload *payload;

  payload = g_slice_new0 (DatabasePayload);
  payload->db = g_object_ref (db);
  payload->qp = g_object_ref (qp);
  payload->manager = manager;
  payload->monitor = g_object_ref (monitor);
  payload->path = g_strdup (path);

  return payload;
}

typedef struct {
  /* string path => struct DatabasePayload */
  GHashTable *databases;
  /* string lang_name => object XapianStem */
  GHashTable *stemmers;
} XbDatabaseManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XbDatabaseManager, xb_database_manager, G_TYPE_OBJECT)

static void
xb_database_manager_invalidate_db (XbDatabaseManager *self,
                                   const gchar *path)
{
  if (self != NULL)
    {
      XbDatabaseManagerPrivate *priv = xb_database_manager_get_instance_private (self);
      g_hash_table_remove (priv->databases, path);
    }
}

static void
database_monitor_changed (GFileMonitor *monitor,
                          GFile *file,
                          GFile *other_file,
                          GFileMonitorEvent event_type,
                          gpointer user_data)
{
  DatabasePayload *payload = user_data;

  switch (event_type)
    {
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_DELETED:
    case G_FILE_MONITOR_EVENT_MOVED:
    case G_FILE_MONITOR_EVENT_UNMOUNTED:
      xb_database_manager_invalidate_db (payload->manager, payload->path);
      break;
    default:
      break;
    }
}

static GFileMonitor *
xb_database_manager_monitor_db (XbDatabaseManager *self,
                                const gchar *path)
{
  GFile *file;
  GFileMonitor *monitor;
  GError *error = NULL;

  file = g_file_new_for_path (path);
  monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, &error);
  g_object_unref (file);

  if (error != NULL)
    {
      /* Non-fatal */
      g_warning ("Could not monitor database at path %s: %s",
                 path, error->message);
      g_error_free (error);
      return NULL;
    }

  return monitor;
}

/* Registers the prefixes and booleanPrefixes contained in the JSON object
 * to the query parser.
 */
static void
xb_database_manager_add_queryparser_prefixes (XbDatabaseManager *self,
                                              XapianQueryParser *query_parser,
                                              JsonObject *object)
{
  JsonNode *element_node;
  JsonObject *element_object;
  JsonArray *array;
  GList *elements, *l;

  array = json_object_get_array_member (object, "prefixes");
  elements = json_array_get_elements (array);

  for (l = elements; l != NULL; l = l->next)
    {
      element_node = l->data;
      element_object = json_node_get_object (element_node);
      xapian_query_parser_add_prefix (query_parser,
                                      json_object_get_string_member (element_object, "field"),
                                      json_object_get_string_member (element_object, "prefix"));
    }

  g_list_free (elements);

  array = json_object_get_array_member (object, "booleanPrefixes");
  elements = json_array_get_elements (array);

  for (l = elements; l != NULL; l = l->next)
    {
      element_node = l->data;
      element_object = json_node_get_object (element_node);
      xapian_query_parser_add_boolean_prefix (query_parser,
                                              json_object_get_string_member (element_object, "field"),
                                              json_object_get_string_member (element_object, "prefix"),
                                              FALSE);
    }

  g_list_free (elements);
}

static void
xb_database_manager_add_queryparser_standard_prefixes (XbDatabaseManager *self,
                                                       XapianQueryParser *query_parser)
{
  /* TODO: these should be configurable */
  static const struct {
    const gchar *field;
    const gchar *prefix;
  } standard_prefixes[] = {
    { "title", "S" },
    { "exact_title", "XEXACTS" },
  }, standard_boolean_prefixes[] = {
    { "tag", "K" },
    { "id", "Q" },
  };
  gint idx;

  for (idx = 0; idx < G_N_ELEMENTS (standard_prefixes); idx++)
    xapian_query_parser_add_prefix (query_parser,
                                    standard_prefixes[idx].field,
                                    standard_prefixes[idx].prefix);

  for (idx = 0; idx < G_N_ELEMENTS (standard_boolean_prefixes); idx++)
    xapian_query_parser_add_boolean_prefix (query_parser,
                                            standard_boolean_prefixes[idx].field,
                                            standard_boolean_prefixes[idx].prefix,
                                            FALSE);
}

static void
xb_database_manager_finalize (GObject *object)
{
  XbDatabaseManager *self = XB_DATABASE_MANAGER (object);
  XbDatabaseManagerPrivate *priv = xb_database_manager_get_instance_private (self);

  g_clear_pointer (&priv->databases, g_hash_table_unref);
  g_clear_pointer (&priv->stemmers, g_hash_table_unref);

  G_OBJECT_CLASS (xb_database_manager_parent_class)->finalize (object);
}

static void
xb_database_manager_class_init (XbDatabaseManagerClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    gobject_class->finalize = xb_database_manager_finalize;
}

static void
xb_database_manager_init (XbDatabaseManager *self)
{
  XbDatabaseManagerPrivate *priv = xb_database_manager_get_instance_private (self);

  priv->stemmers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  g_hash_table_insert (priv->stemmers, g_strdup ("none"), xapian_stem_new ());

  priv->databases = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           g_free, (GDestroyNotify) database_payload_free);
}

static void
xb_database_manager_register_prefixes (XbDatabaseManager *self,
                                       XapianDatabase *db,
                                       XapianQueryParser *query_parser,
                                       const gchar *path)
{
  gchar *metadata_json;
  GError *error = NULL;
  JsonParser *parser = NULL;
  JsonNode *root;

  /* Attempt to read the database's custom prefix association metadata */
  metadata_json = xapian_database_get_metadata (db, PREFIX_METADATA_KEY, &error);
  if (error != NULL)
    {
      g_warning ("Could not register prefixes for database %s: %s",
                 path, error->message);
      goto out;
    }

  parser = json_parser_new ();
  json_parser_load_from_data (parser, metadata_json, -1, &error);
  if (error != NULL)
    {
      g_warning ("Could not parse JSON prefixes metadata for database %s: %s",
                 path, error->message);
      goto out;
    }

  root = json_parser_get_root (parser);
  if (root != NULL)
    xb_database_manager_add_queryparser_prefixes (self, query_parser,
                                                  json_node_get_object (root));

 out:
  /* If there was an error, just use the "standard" prefix map */
  if (error != NULL)
    xb_database_manager_add_queryparser_standard_prefixes (self, query_parser);

  g_clear_error (&error);
  g_clear_object (&parser);
  g_free (metadata_json);
}

static gboolean
xb_database_manager_register_stopwords (XbDatabaseManager *self,
                                        XapianDatabase *db,
                                        XapianQueryParser *query_parser,
                                        GError **error_out)
{
  XapianSimpleStopper *stopper;
  gchar *stopwords_json;
  GError *error = NULL;
  JsonParser *parser;
  JsonNode *node;
  JsonArray *array;
  GList *elements, *l;

  stopwords_json = xapian_database_get_metadata (db, STOPWORDS_METADATA_KEY, &error);
  if (error != NULL)
    {
      g_propagate_error (error_out, error);
      return FALSE;
    }

  parser = json_parser_new ();
  json_parser_load_from_data (parser, stopwords_json, -1, &error);
  if (error != NULL)
    {
      g_propagate_error (error_out, error);
      g_object_unref (parser);
      return FALSE;
    }

  /* empty JSON is not an error */
  node = json_parser_get_root (parser);
  if (node != NULL)
    {
      array = json_node_get_array (node);
      elements = json_array_get_elements (array);

      stopper = xapian_simple_stopper_new ();
      for (l = elements; l != NULL; l = l->next)
        xapian_simple_stopper_add (stopper, json_node_get_string (l->data));

      xapian_query_parser_set_stopper (query_parser, XAPIAN_STOPPER (stopper));
      g_object_unref (stopper);
    }

  g_object_unref (parser);

  return TRUE;
}

/* Creates a new XapianDatabase for the given path, and indexes it by path,
 * overwriting any existing database with the same name.
 */
static DatabasePayload *
xb_database_manager_create_db_internal (XbDatabaseManager *self,
                                        const gchar *path,
                                        GError **error_out)
{
  XbDatabaseManagerPrivate *priv = xb_database_manager_get_instance_private (self);
  XapianDatabase *db;
  GError *error = NULL;
  XapianQueryParser *query_parser;
  DatabasePayload *payload;
  GFileMonitor *monitor;

  g_assert (!g_hash_table_contains (priv->databases, path));

  db = xapian_database_new_with_path (path, &error);
  if (error != NULL)
    {
      g_set_error (error_out, XB_ERROR,
                   XB_ERROR_INVALID_PATH,
                   "Cannot create XapianDatabase for path %s: %s",
                   path, error->message);
      g_error_free (error);
      return NULL;
    }

  /* Create a XapianQueryParser for this particular database, stemming by its
   * registered language.
   */
  query_parser = xapian_query_parser_new ();
  xapian_query_parser_set_database (query_parser, db);

  xb_database_manager_register_prefixes (self, db, query_parser, path);

  if (!xb_database_manager_register_stopwords (self, db, query_parser, &error))
    {
      /* Non-fatal */
      g_warning ("Could not add stop words for database %s: %s.", path, error->message);
      g_clear_error (&error);
    }

  monitor = xb_database_manager_monitor_db (self, path);
  payload = database_payload_new (db, query_parser, self, monitor, path);
  g_hash_table_insert (priv->databases, g_strdup (path), payload);

  g_signal_connect (monitor, "changed",
                    G_CALLBACK (database_monitor_changed), payload);

  g_object_unref (db);
  g_object_unref (query_parser);
  g_object_unref (monitor);

  return payload;
}

static DatabasePayload *
xb_database_manager_ensure_db (XbDatabaseManager *self,
                               const gchar *path,
                               GError **error_out)
{
  XbDatabaseManagerPrivate *priv = xb_database_manager_get_instance_private (self);
  GError *error = NULL;
  DatabasePayload *payload;

  payload = g_hash_table_lookup (priv->databases, path);
  if (payload == NULL)
    payload = xb_database_manager_create_db_internal (self, path, &error);

  if (error != NULL)
    {
      g_propagate_error (error_out, error);
      return NULL;
    }

  return payload;
}

gboolean
xb_database_manager_create_db (XbDatabaseManager *self,
                               const gchar *path,
                               GError **error_out)
{
  GError *error = NULL;

  xb_database_manager_create_db_internal (self, path, &error);
  if (error != NULL)
    {
      g_propagate_error (error_out, error);
      return FALSE;
    }

  return TRUE;
}

static JsonObject *
xb_database_manager_fetch_results (XbDatabaseManager *self,
                                   XapianEnquire *enquire,
                                   XapianQuery *query,
                                   const gchar *query_str,
                                   GHashTable *query_options,
                                   GError **error_out)
{
  const gchar *str;
  gchar *document_data;
  guint limit, offset;
  XapianMSet *matches;
  XapianMSetIterator *iter;
  XapianDocument *document;
  GError *error = NULL;
  JsonObject *retval;
  JsonArray *results_array;

  str = g_hash_table_lookup (query_options, QUERY_PARAM_OFFSET);
  if (str == NULL)
    {
      g_set_error_literal (error_out, XB_ERROR,
                           XB_ERROR_INVALID_PARAMS,
                           "Offset parameter is required for the query");
      return NULL;
    }

  offset = (guint) g_ascii_strtod (str, NULL);

  str = g_hash_table_lookup (query_options, QUERY_PARAM_LIMIT);
  if (str == NULL)
    {
      g_set_error_literal (error_out, XB_ERROR,
                           XB_ERROR_INVALID_PARAMS,
                           "Limit parameter is required for the query");
      return NULL;
    }

  limit = (guint) g_ascii_strtod (str, NULL);

  xapian_enquire_set_query (enquire, query, xapian_query_get_length (query));
  matches = xapian_enquire_get_mset (enquire, offset, limit, &error);
  if (error != NULL)
    {
      g_propagate_error (error_out, error);
      return NULL;
    }

  retval = json_object_new ();
  json_object_set_int_member (retval, QUERY_RESULTS_MEMBER_NUM_RESULTS, xapian_mset_get_size (matches));
  json_object_set_int_member (retval, QUERY_RESULTS_MEMBER_OFFSET, offset);
  if (query_str != NULL)
      json_object_set_string_member (retval, QUERY_RESULTS_MEMBER_QUERYSTR, query_str);

  results_array = json_array_new ();
  json_object_set_array_member (retval, QUERY_RESULTS_MEMBER_RESULTS, results_array);

  iter = xapian_mset_get_begin (matches);
  while (xapian_mset_iterator_next (iter))
    {
      document = xapian_mset_iterator_get_document (iter, &error);
      if (error != NULL)
        {
          g_warning ("Unable to fetch document from iterator: %s",
                     error->message);
          g_clear_error (&error);
          continue;
        }

      document_data = xapian_document_get_data (document);
      json_array_add_string_element (results_array, document_data);
      g_free (document_data);
    }

  g_object_unref (iter);
  g_object_unref (matches);

  return retval;
}

/* Checks if the given database is empty (has no documents). Empty databases
 * cause problems with XapianEnquire, so we need to assert that a db isn't empty
 * before making an XapianEnquire for it.
 */
static gboolean
database_is_empty (XapianDatabase *db)
{
  char *uuid;

  /* "If the backend does not support UUIDs or this database has no
   * subdatabases, the UUID will be empty."
   * http://xapian.org/docs/apidoc/html/classXapian_1_1Database.html
   */
  uuid = xapian_database_get_uuid (db);
  if (uuid == NULL || uuid[0] == '\0')
    return TRUE;

  g_free (uuid);
  return FALSE;
}

static JsonObject *
create_empty_query_results (void)
{
  JsonObject *object;

  object = json_object_new ();
  json_object_set_int_member (object, QUERY_RESULTS_MEMBER_NUM_RESULTS, 0);
  json_object_set_int_member (object, QUERY_RESULTS_MEMBER_OFFSET, 0);
  json_object_set_array_member (object, QUERY_RESULTS_MEMBER_RESULTS, json_array_new ());

  return object;
}

static JsonObject *
xb_database_manager_fix_query_internal (XbDatabaseManager *self,
                           DatabasePayload *payload,
                           GHashTable *query_options,
                           GError **error_out)
{
  gchar *spell_corrected_query_str = NULL, *no_stop_words = NULL;
  gchar **words = NULL, **words_iter = NULL;
  gchar **filtered_words = NULL, **filtered_iter = NULL;
  GError *error = NULL;
  const gchar *query_str;
  const gchar *match_all;
  XapianStopper *stopper;
  JsonObject *retval;

  retval = json_object_new ();

  query_str = g_hash_table_lookup (query_options, QUERY_PARAM_QUERYSTR);
  match_all = g_hash_table_lookup (query_options, QUERY_PARAM_MATCH_ALL);

  stopper = xapian_query_parser_get_stopper (payload->qp);

  if (query_str == NULL || match_all != NULL)
    {
      g_set_error (error_out, XB_ERROR,
                  XB_ERROR_INVALID_PARAMS,
                  "Query parameter must be set, and must not be match all.");
      goto out;
    }
  else if (stopper != NULL)
    {
      words = g_strsplit (query_str, " ", -1);
      filtered_words = g_new0 (gchar *, g_strv_length (words) + 1);

      filtered_iter = filtered_words;
      for (words_iter = words; *words_iter != NULL; words_iter++)
        if (!xapian_stopper_is_stop_term (stopper, *words_iter))
          *filtered_iter++ = *words_iter;

      no_stop_words = g_strjoinv (" ", filtered_words);
      json_object_set_string_member (retval, FIX_RESULTS_MEMBER_STOP_WORD_CORRECTED_RESULT,
                                     no_stop_words);
    }

  /* Parse the user's query so we can request a spelling correction. */
  xapian_query_parser_parse_query_full (payload->qp, query_str,
                                        QUERY_PARSER_FLAGS, "", &error);

  if (error != NULL)
    {
      g_propagate_error (error_out, error);
      goto out;
    }

  spell_corrected_query_str = xapian_query_parser_get_corrected_query_string (payload->qp);
  if (spell_corrected_query_str != NULL && spell_corrected_query_str[0] != '\0')
    {
      json_object_set_string_member (retval, FIX_RESULTS_MEMBER_SPELL_CORRECTED_RESULT,
                                     spell_corrected_query_str);
    }

 out:
  g_free (filtered_words);
  g_free (no_stop_words);
  g_free (spell_corrected_query_str);
  g_strfreev (words);

  return retval;
}

/* Queries the database with the given parameters, and returns a JSON object
 * with the following members:
 *   - numResults: number of results being returned
 *   - offset: index from which results were gathered
 *   - query: the query string that produced the results
 *   - results: an array of strings for every result document, sorted according
 *              to the query parameters
 */
static JsonObject *
xb_database_manager_query (XbDatabaseManager *self,
                           DatabasePayload *payload,
                           GHashTable *query_options,
                           GError **error_out)
{
  XapianQuery *parsed_query = NULL;
  gchar *query_str = NULL;
  XapianEnquire *enquire = NULL;
  GError *error = NULL;
  const gchar *lang;
  XbDatabaseManagerPrivate *priv = xb_database_manager_get_instance_private (self);
  XapianStem *stem;
  const gchar *str;
  const gchar *match_all;
  JsonObject *results = NULL;

  if (database_is_empty (payload->db))
    return create_empty_query_results ();

  str = g_hash_table_lookup (query_options, QUERY_PARAM_QUERYSTR);

  lang = g_hash_table_lookup (query_options, QUERY_PARAM_LANG);
  if (lang == NULL)
      lang = "none";

  stem = g_hash_table_lookup (priv->stemmers, lang);
  if (stem == NULL)
    {
      stem = xapian_stem_new_for_language (lang, &error);
      if (error != NULL)
        {
          g_warning ("Cannot create XapianStem for language %s: %s",
                     lang, error->message);
          g_clear_error (&error);

          stem = g_hash_table_lookup (priv->stemmers, "none");
        }
      else
        {
          g_hash_table_insert (priv->stemmers, g_strdup (lang), stem);
        }
    }

  g_assert (stem != NULL);

  xapian_query_parser_set_stemmer (payload->qp, stem);
  xapian_query_parser_set_stemming_strategy (payload->qp, XAPIAN_STEM_STRATEGY_STEM_SOME);

  enquire = xapian_enquire_new (payload->db, &error);
  if (error != NULL)
    {
      g_propagate_error (error_out, error);
      goto out;
    }

  match_all = g_hash_table_lookup (query_options, QUERY_PARAM_MATCH_ALL);
  if (match_all != NULL && str == NULL)
    {
      parsed_query = xapian_query_new_match_all ();
    }
  else if (str != NULL && match_all == NULL)
    {
      /* save the query string aside */
      query_str = g_strdup (str);
      parsed_query = xapian_query_parser_parse_query_full (payload->qp, query_str,
                                                           QUERY_PARSER_FLAGS, "", &error);
    }
  else
    {
      g_set_error (error_out, XB_ERROR,
                  XB_ERROR_INVALID_PARAMS,
                  "Exactly one of query parameter or match all parameter is required.");
      goto out;
    }

  if (error != NULL)
    {
      g_propagate_error (error_out, error);
      goto out;
    }

  str = g_hash_table_lookup (query_options, QUERY_PARAM_COLLAPSE_KEY);
  if (str != NULL)
    xapian_enquire_set_collapse_key (enquire, (guint) g_ascii_strtod (str, NULL));

  str = g_hash_table_lookup (query_options, QUERY_PARAM_SORT_BY);
  if (str != NULL)
    {
      gboolean reverse_order;
      const gchar *order;

      order = g_hash_table_lookup (query_options, QUERY_PARAM_ORDER);
      reverse_order = (g_strcmp0 (order, "desc") == 0);
      xapian_enquire_set_sort_by_value (enquire,
                                        (guint) g_ascii_strtod (str, NULL),
                                        reverse_order);
    }
  else
    {
      str = g_hash_table_lookup (query_options, QUERY_PARAM_CUTOFF);
      if (str != NULL)
        xapian_enquire_set_cutoff (enquire, (guint) g_ascii_strtod (str, NULL));
    }

  results = xb_database_manager_fetch_results (self, enquire, parsed_query,
                                               query_str, query_options, &error);

  if (error != NULL)
    {
      g_propagate_error (error_out, error);
      goto out;
    }

 out:
  g_clear_object (&parsed_query);
  g_clear_object (&enquire);
  g_free (query_str);

  return results;
}

JsonObject *
xb_database_manager_fix_query (XbDatabaseManager *self,
                              const gchar *path,
                              GHashTable *query,
                              GError **error_out)
{
  DatabasePayload *payload;
  GError *error = NULL;

  g_assert (path != NULL);

  payload = xb_database_manager_ensure_db (self, path, &error);
  if (error != NULL)
    {
      g_propagate_error (error_out, error);
      return FALSE;
    }

  return xb_database_manager_fix_query_internal (self, payload, query, error_out);
}

/* If a database exists, queries it with the following options:
 *   - collapse: see http://xapian.org/docs/collapsing.html
 *   - cutoff: percent between (0, 100) for the XapianEnquire cutoff parameter
 *   - limit: max number of results to return
 *   - offset: offset from which to start returning results
 *   - order: if sortBy is specified, either "desc" or "asc" (resp. "descending"
 *            and "ascending"
 *   - q: querystring that's parseable by a XapianQueryParser
 *   - sortBy: field to sort the results on
 */
JsonObject *
xb_database_manager_query_db (XbDatabaseManager *self,
                              const gchar *path,
                              GHashTable *query,
                              GError **error_out)
{
  DatabasePayload *payload;
  GError *error = NULL;

  g_assert (path != NULL);

  payload = xb_database_manager_ensure_db (self, path, &error);
  if (error != NULL)
    {
      g_propagate_error (error_out, error);
      return FALSE;
    }

  return xb_database_manager_query (self, payload, query, error_out);
}

XbDatabaseManager *
xb_database_manager_new (void)
{
  return g_object_new (XB_TYPE_DATABASE_MANAGER, NULL);
}
