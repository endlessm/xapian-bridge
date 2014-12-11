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
#define QUERY_PARAM_OFFSET "offset"
#define QUERY_PARAM_ORDER "order"
#define QUERY_PARAM_QUERYSTR "q"
#define QUERY_PARAM_SORT_BY "sortBy"

#define RESULTS_MEMBER_NUM_RESULTS "numResults"
#define RESULTS_MEMBER_OFFSET "offset"
#define RESULTS_MEMBER_QUERYSTR "query"
#define RESULTS_MEMBER_RESULTS "results"
#define RESULTS_MEMBER_SPELL_CORRECTED_RESULTS "spellCorrectedResults"

#define PREFIX_METADATA_KEY "XbPrefixes"
#define STOPWORDS_METADATA_KEY "XbStopwords"
#define QUERY_PARSER_FLAGS XAPIAN_QUERY_PARSER_FEATURE_DEFAULT | \
  XAPIAN_QUERY_PARSER_FEATURE_WILDCARD |\
  XAPIAN_QUERY_PARSER_FEATURE_SPELLING_CORRECTION

typedef struct {
  XapianDatabase *db;
  XapianQueryParser *qp;
  gchar *lang;
} DatabasePayload;

static void
database_payload_free (DatabasePayload *payload)
{
  g_clear_object (&payload->db);
  g_clear_object (&payload->qp);
  g_free (payload->lang);

  g_slice_free (DatabasePayload, payload);
}

static DatabasePayload *
database_payload_new (XapianDatabase *db,
		      XapianQueryParser *qp,
		      const gchar *lang)
{
  DatabasePayload *payload;

  payload = g_slice_new0 (DatabasePayload);
  payload->db = g_object_ref (db);
  payload->qp = g_object_ref (qp);
  payload->lang = g_strdup (lang);

  return payload;
}

typedef struct {
  /* string path => struct DatabasePayload */
  GHashTable *databases;
  /* string lang_name => object XapianStem */
  GHashTable *stemmers;
} XbDatabaseManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XbDatabaseManager, xb_database_manager, G_TYPE_OBJECT)

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

/* Returns whether the manager has a database at path */
gboolean
xb_database_manager_has_db (XbDatabaseManager *self,
			    const gchar *path)
{
  XbDatabaseManagerPrivate *priv = xb_database_manager_get_instance_private (self);
  return (g_hash_table_lookup (priv->databases, path) != NULL);
}

static gboolean
stem_supports_language (const gchar *lang)
{
  gchar **available_langs;
  gint idx;
  gboolean retval = FALSE;
  const gchar *xapian_lang = NULL;

  /* Mapping from ISO 639 lang codes to
   * Xapian's internal language strings.
   */
  static const struct {
    const gchar *iso_code;
    const gchar *xapian_code;
  } lang_code_map[] = {
    { "ar", "arabic" },
    { "es", "spanish" },
    { "en", "english" },
    { "fr", "french" },
    { "pt", "portuguese" },
  };

  for (idx = 0; idx < G_N_ELEMENTS (lang_code_map); idx++)
    {
      if (g_strcmp0 (lang, lang_code_map[idx].xapian_code) == 0)
        xapian_lang = lang_code_map[idx].xapian_code;
    }

  if (xapian_lang == NULL)
    return FALSE;

  available_langs = xapian_stem_get_available_languages ();
  for (idx = 0; available_langs[idx] != NULL; idx++)
    {
      if (g_strcmp0 (xapian_lang, available_langs[idx]) == 0)
        {
          retval = TRUE;
          break;
        }
    }

  g_strfreev (available_langs);
  return retval;
}

static void
xb_database_manager_register_prefixes (XbDatabaseManager *self,
                                       XapianDatabase *db,
                                       XapianQueryParser *query_parser,
                                       const gchar *lang,
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

  /* Store the prefix association metadata for this given lang, so
   * the various meta databases which query over this database can
   * use its prefixes
   */
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
					const gchar *lang,
					GError **error_out)
{
  XbDatabaseManagerPrivate *priv = xb_database_manager_get_instance_private (self);
  XapianDatabase *db;
  XapianStem *stem;
  GError *error = NULL;
  const gchar *stem_lang;
  XapianQueryParser *query_parser;
  DatabasePayload *payload;

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

  stem = g_hash_table_lookup (priv->stemmers, lang);
  if (stem == NULL)
    {
      if (stem_supports_language (lang))
        stem_lang = lang;
      else
        stem_lang = "none";

      stem = xapian_stem_new_for_language (stem_lang, &error);
      if (error != NULL)
        {
          g_set_error (error_out, XB_ERROR,
                       XB_ERROR_UNSUPPORTED_LANG,
                       "Cannot create XapianStem for language %s: %s",
                       lang, error->message);
          g_error_free (error);
          return NULL;
        }

      g_hash_table_insert (priv->stemmers, g_strdup (lang), stem);
    }

  g_assert (stem != NULL);

  /* Create a XapianQueryParser for this particular database, stemming by its
   * registered language.
   */
  query_parser = xapian_query_parser_new ();
  xapian_query_parser_set_stemming_strategy (query_parser, XAPIAN_STEM_STRATEGY_STEM_SOME);
  xapian_query_parser_set_stemmer (query_parser, stem);
  xapian_query_parser_set_database (query_parser, db);

  xb_database_manager_register_prefixes (self, db, query_parser, lang, path);

  if (!xb_database_manager_register_stopwords (self, db, query_parser, &error))
    {
      /* Non-fatal */
      g_warning ("Could not add stop words for database %s: %s.", path, error->message);
      g_clear_error (&error);
    }

  payload = database_payload_new (db, query_parser, lang);
  g_hash_table_insert (priv->databases, g_strdup (path), payload);
  g_object_unref (db);
  g_object_unref (query_parser);

  return payload;
}

static DatabasePayload *
xb_database_manager_ensure_db_for_query (XbDatabaseManager *self,
					 const gchar *path,
					 GHashTable *query,
					 GError **error_out)
{
  XbDatabaseManagerPrivate *priv = xb_database_manager_get_instance_private (self);
  const gchar *lang;
  GError *error = NULL;
  DatabasePayload *payload;

  lang = g_hash_table_lookup (query, "lang");
  if (lang == NULL)
    lang = "none";

  payload = g_hash_table_lookup (priv->databases, path);
  if (payload == NULL)
    payload = xb_database_manager_create_db_internal (self, path, lang, &error);

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
			       const gchar *lang,
			       GError **error_out)
{
  GError *error = NULL;

  xb_database_manager_create_db_internal (self, path, lang, &error);
  if (error != NULL)
    {
      g_propagate_error (error_out, error);
      return FALSE;
    }

  return TRUE;
}

/* Deletes the database indexed at path (if any) */
gboolean
xb_database_manager_remove_db (XbDatabaseManager *self,
                               const gchar *path,
                               GError **error_out)
{
  XbDatabaseManagerPrivate *priv = xb_database_manager_get_instance_private (self);

  if (!xb_database_manager_has_db (self, path))
    {
      g_set_error (error_out, XB_ERROR,
                   XB_ERROR_DATABASE_NOT_FOUND,
                   "Cannnot find database at path %s",
                   path);
      return FALSE;
    }

  g_hash_table_remove (priv->databases, path);

  return TRUE;
}

static JsonObject *
document_to_json_object (XapianDocument *document)
{
  gchar *data;
  JsonParser *parser;
  GError *error = NULL;
  JsonObject *object = NULL;
  JsonNode *root;

  data = xapian_document_get_data (document);
  parser = json_parser_new ();
  json_parser_load_from_data (parser, data, -1, &error);

  if (error != NULL)
    {
      g_warning ("Unable to convert XapianDocument to JsonObject: %s",
		 error->message);
      g_clear_error (&error);
      goto out;
    }

  root = json_parser_get_root (parser);
  object = json_object_ref (json_node_get_object (root));

 out:
  g_free (data);
  g_object_unref (parser);

  return object;
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
  guint limit, offset;
  XapianMSet *matches;
  XapianMSetIterator *iter;
  XapianDocument *document;
  GError *error = NULL;
  JsonObject *retval, *document_object;
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
  json_object_set_int_member (retval, RESULTS_MEMBER_NUM_RESULTS, xapian_mset_get_size (matches));
  json_object_set_int_member (retval, RESULTS_MEMBER_OFFSET, offset);
  json_object_set_string_member (retval, RESULTS_MEMBER_QUERYSTR, query_str);

  results_array = json_array_new ();
  json_object_set_array_member (retval, RESULTS_MEMBER_RESULTS, results_array);

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

      document_object = document_to_json_object (document);
      json_array_add_object_element (results_array, document_object);
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
  json_object_set_int_member (object, RESULTS_MEMBER_NUM_RESULTS, 0);
  json_object_set_int_member (object, RESULTS_MEMBER_OFFSET, 0);
  json_object_set_array_member (object, RESULTS_MEMBER_RESULTS, json_array_new ());

  return object;
}

/* Queries the database with the given parameters, and returns a JSON object
 * with the following members:
 *   - numResults: number of results being returned
 *   - offset: index from which results were gathered
 *   - query: the query string that produced the results
 *   - results: an array of strings for every result document, sorted according
 *              to the query parameters
 *   - spellCorrectedResults: similar to results, but include spell corrections
 */
static JsonObject *
xb_database_manager_query (XbDatabaseManager *self,
			   DatabasePayload *payload,
                           GHashTable *query_options,
                           GError **error_out)
{
  XapianQuery *parsed_query = NULL, *corrected_query = NULL;
  gchar *corrected_query_str = NULL, *query_str = NULL;
  XapianEnquire *enquire = NULL;
  GError *error = NULL;
  const gchar *str;
  JsonObject *results = NULL, *corrected_results;

  if (database_is_empty (payload->db))
    return create_empty_query_results ();

  str = g_hash_table_lookup (query_options, QUERY_PARAM_QUERYSTR);
  if (str == NULL)
    {
      g_set_error (error_out, XB_ERROR,
                   XB_ERROR_INVALID_PARAMS,
                   "Query parameter is required for the query");
      goto out;
    }

  /* save the query string aside */
  query_str = g_strdup (str);

  enquire = xapian_enquire_new (payload->db, &error);
  if (error != NULL)
    {
      g_propagate_error (error_out, error);
      goto out;
    }

  parsed_query = xapian_query_parser_parse_query_full (payload->qp, str,
                                                       QUERY_PARSER_FLAGS, "", &error);

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

  /* corrected_query_str will be the empty string if no correction is found */
  corrected_query_str = xapian_query_parser_get_corrected_query_string (payload->qp);
  if (corrected_query_str != NULL && corrected_query_str[0] != '\0')
    {
      corrected_query = xapian_query_parser_parse_query_full (payload->qp, corrected_query_str,
                                                              QUERY_PARSER_FLAGS, "", &error);
      if (error != NULL)
        {
          g_warning ("Unable to parse corrected query: %s", error->message);
          g_error_free (error);
        }
    }

  if (corrected_query != NULL)
    {
      corrected_results = xb_database_manager_fetch_results (self, enquire, corrected_query,
                                                             corrected_query_str, query_options, &error);
      if (error != NULL)
        {
          g_warning ("Unable to fetch corrected results: %s", error->message);
          g_error_free (error);
        }
      else
        {
          json_object_set_object_member (results,
                                         RESULTS_MEMBER_SPELL_CORRECTED_RESULTS,
                                         corrected_results);
        }

      g_object_unref (corrected_query);
    }

 out:
  g_clear_object (&parsed_query);
  g_clear_object (&enquire);
  g_free (corrected_query_str);
  g_free (query_str);

  return results;
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

  payload = xb_database_manager_ensure_db_for_query (self, path, query, &error);
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
