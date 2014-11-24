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

#include "xb-prefix-store.h"

typedef struct {
  GHashTable *prefixes_by_lang;
} XbPrefixStorePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XbPrefixStore, xb_prefix_store, G_TYPE_OBJECT);

static void
xb_prefix_store_finalize (GObject *object)
{
  XbPrefixStore *self = XB_PREFIX_STORE (object);
  XbPrefixStorePrivate *priv = xb_prefix_store_get_instance_private (self);

  g_clear_pointer (&priv->prefixes_by_lang, g_hash_table_unref);

  G_OBJECT_CLASS (xb_prefix_store_parent_class)->finalize (object);
}

static void
xb_prefix_store_class_init (XbPrefixStoreClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  gobject_class->finalize = xb_prefix_store_finalize;
}

static void
xb_prefix_store_init (XbPrefixStore *self)
{
  XbPrefixStorePrivate *priv = xb_prefix_store_get_instance_private (self);
  priv->prefixes_by_lang = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, (GDestroyNotify) json_object_unref);
}

XbPrefixStore *
xb_prefix_store_new (void)
{
  return g_object_new (XB_TYPE_PREFIX_STORE, NULL);
}

/* Returns whether the field -> prefix association specified within the given
 * JSON object exists in the given JSON array.
 */
static gboolean
array_contains_pair (JsonArray *array,
                     JsonObject *object)
{
  JsonNode *element_node;
  JsonObject *element;
  GList *elements, *l;
  const gchar *object_field, *object_prefix, *element_field, *element_prefix;
  gboolean retval = FALSE;

  object_field = json_object_get_string_member (object, "field");
  object_prefix = json_object_get_string_member (object, "prefix");

  elements = json_array_get_elements (array);
  for (l = elements; l != NULL; l = l->next)
    {
      element_node = l->data;
      element = json_node_get_object (element_node);
      element_field = json_object_get_string_member (element, "field");
      element_prefix = json_object_get_string_member (element, "prefix");

      if (g_strcmp0 (element_field, object_field) == 0 &&
          g_strcmp0 (element_prefix, object_prefix) == 0)
        {
          retval = TRUE;
          break;
        }
    }

  g_list_free (elements);
  return retval;
}

static JsonObject *
xb_prefix_store_new_empty_store (void)
{
  JsonObject *prefixes;
  JsonArray *array;

  prefixes = json_object_new ();

  array = json_array_new ();
  json_object_set_array_member (prefixes, "prefixes", array);

  array = json_array_new ();
  json_object_set_array_member (prefixes, "booleanPrefixes", array);

  return prefixes;
}

/* Stores a (lang, object) pair, if it has not been already stored
 * for the given lang.
 */
static void
xb_prefix_store_store (XbPrefixStore *self,
                       const gchar *lang,
                       JsonObject *object,
                       gboolean is_boolean)
{
  XbPrefixStorePrivate *priv = xb_prefix_store_get_instance_private (self);
  JsonObject *prefixes;
  JsonArray *array;

  prefixes = g_hash_table_lookup (priv->prefixes_by_lang, lang);
  if (prefixes == NULL)
    {
      prefixes = xb_prefix_store_new_empty_store ();
      g_hash_table_insert (priv->prefixes_by_lang, g_strdup (lang), prefixes);
    }

  if (is_boolean)
    array = json_object_get_array_member (prefixes, "booleanPrefixes");
  else
    array = json_object_get_array_member (prefixes, "prefixes");

  if (!array_contains_pair (array, object))
    json_array_add_object_element (array, json_object_ref (object));
}

/* Given a prefix map (like the one retured by xb_prefix_store_get()),
 * store the prefixes and booleanPrefixes contained within accordingly.
 */
void
xb_prefix_store_store_prefix_map (XbPrefixStore *self,
                                  const gchar *lang,
                                  JsonObject *object)
{
  JsonObject *element_object;
  JsonArray *array;
  GList *elements, *l;
  JsonNode *element_node;

  array = json_object_get_array_member (object, "prefixes");
  elements = json_array_get_elements (array);

  for (l = elements; l != NULL; l = l->next)
    {
      element_node = l->data;
      element_object = json_node_get_object (element_node);
      xb_prefix_store_store_prefix (self, lang, element_object);
    }

  g_list_free (elements);

  array = json_object_get_array_member (object, "booleanPrefixes");
  elements = json_array_get_elements (array);

  for (l = elements; l != NULL; l = l->next)
    {
      element_node = l->data;
      element_object = json_node_get_object (element_node);
      xb_prefix_store_store_boolean_prefix (self, lang, element_object);
    }

  g_list_free (elements);
}

/* Same as xb_prefix_store_get(), but returns the unqiue associations across all
 * languages.
 *
 * Caller should use json_object_unref() when done with the return value.
 */
JsonObject *
xb_prefix_store_get_all (XbPrefixStore *self)
{
  XbPrefixStorePrivate *priv = xb_prefix_store_get_instance_private (self);
  GHashTableIter iter;
  const gchar *lang;
  JsonObject *object, *element_object;
  JsonObject *prefixes;
  JsonArray *array, *all_prefixes, *all_boolean_prefixes;
  GList *elements, *l;
  JsonNode *element_node;

  prefixes = xb_prefix_store_new_empty_store ();
  all_prefixes = json_object_get_array_member (prefixes, "prefixes");
  all_boolean_prefixes = json_object_get_array_member (prefixes, "booleanPrefixes");
  
  g_hash_table_iter_init (&iter, priv->prefixes_by_lang);
  while (g_hash_table_iter_next (&iter, (gpointer *) &lang, (gpointer *) &object))
    {
      array = json_object_get_array_member (object, "prefixes");
      elements = json_array_get_elements (array);

      for (l = elements; l != NULL; l = l->next)
        {
          element_node = l->data;
          element_object = json_node_get_object (element_node);

          if (!array_contains_pair (all_prefixes, element_object))
            json_array_add_object_element (all_prefixes, json_object_ref (element_object));
        }

      g_list_free (elements);

      array = json_object_get_array_member (object, "booleanPrefixes");
      elements = json_array_get_elements (array);

      for (l = elements; l != NULL; l = l->next)
        {
          element_node = l->data;
          element_object = json_node_get_object (element_node);

          if (!array_contains_pair (all_boolean_prefixes, element_object))
            json_array_add_object_element (all_boolean_prefixes, json_object_ref (element_object));
        }

      g_list_free (elements);
    }

  return prefixes;
}

/* Returns a prefix map as a JSON object, representing the unique field -> prefix
 * associations for a certain language.
 *
 * Caller should use json_object_unref() when done with the return value.
 */
JsonObject *
xb_prefix_store_get (XbPrefixStore *self,
                     const gchar *lang)
{
  XbPrefixStorePrivate *priv = xb_prefix_store_get_instance_private (self);
  JsonObject *object;

  object = g_hash_table_lookup (priv->prefixes_by_lang, lang);
  if (object != NULL)
    return json_object_ref (object);

  return NULL;
}

/* Stores the field -> prefix association specified in the JSON object for a
 * given lang, doing nothing if it already was stored.
 */
void
xb_prefix_store_store_prefix (XbPrefixStore *self,
                              const gchar *lang,
                              JsonObject *object)
{
  xb_prefix_store_store (self, lang, object, FALSE);
}

/* Same as xb_prefix_store_store_prefix(), but stores the association
 * as a booleanPrefix.
 */
void
xb_prefix_store_store_boolean_prefix (XbPrefixStore *self,
                                      const gchar *lang,
                                      JsonObject *object)
{
  xb_prefix_store_store (self, lang, object, TRUE);
}
