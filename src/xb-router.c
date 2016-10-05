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

#include "xb-router.h"

enum {
  PATH_NOT_HANDLED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

typedef struct {
  GList *routes;
} XbRouterPrivate;

typedef struct {
  const gchar *method;
  GRegex *path_regex;
  GList *path_spec_keys;
  XbRouterCallback callback;
  gpointer user_data;
} XbRouterRoute;

G_DEFINE_TYPE_WITH_PRIVATE (XbRouter, xb_router, G_TYPE_OBJECT);

/* Takes a path_spec and returns a GRegex that can be used to match a test path
 * against the given path_spec.
 */
static GRegex *
route_get_regexp_for_path (const gchar *path_spec,
                           GList **path_spec_keys)
{
  gchar **components, **regex_components;
  gchar *regex_join, *pattern;
  GList *keys = NULL;
  gint idx, len;
  GError *error = NULL;
  GRegex *regex;

  g_assert (path_spec_keys != NULL);

  components = g_strsplit (path_spec, "/", -1);
  len = g_strv_length (components);
  regex_components = g_malloc0 (sizeof (gchar *) * (len + 1));

  for (idx = 0; idx < len; idx++)
    {
      const gchar *component = components[idx];

      if (component[0] == ':')
        {
          keys = g_list_prepend (keys, g_strdup (component + 1));
          regex_components[idx] = g_strdup ("([^\\/]+)");
        }
      else
        {
          regex_components[idx] = g_strdup (component);
        }
    }

  regex_join = g_strjoinv ("\\/", regex_components);
  /* Make sure that we only match if the entire string matches */
  pattern = g_strconcat ("^", regex_join, "$", NULL);
  g_free (regex_join);
  g_strfreev (regex_components);

  regex = g_regex_new (pattern, 0, 0, &error);
  g_free (pattern);

  if (error != NULL)
    {
      g_warning ("Can't compile GRegex for path spec %s: %s",
                 path_spec, error->message);
      g_error_free (error);
      g_list_free_full (keys, g_free);
      return NULL;
    }

  *path_spec_keys = g_list_reverse (keys);

  return regex;
}

static XbRouterRoute *
xb_router_route_new (const gchar *method,
                     const gchar *path_spec,
                     XbRouterCallback callback,
                     gpointer user_data)
{
  XbRouterRoute *route;

  route = g_slice_new0 (XbRouterRoute);
  /* SOUP_METHOD defines are interned strings, so no need to g_strdup() */
  route->method = method;
  route->callback = callback;
  route->user_data = user_data;

  route->path_regex = route_get_regexp_for_path (path_spec, &route->path_spec_keys);

  return route;
}

static void
xb_router_route_free (XbRouterRoute *route)
{
  g_clear_pointer (&route->path_regex, g_regex_unref);
  g_list_free_full (route->path_spec_keys, g_free);

  g_slice_free (XbRouterRoute, route);
}

static void
xb_router_finalize (GObject *object)
{
  XbRouter *self = XB_ROUTER (object);
  XbRouterPrivate *priv = xb_router_get_instance_private (self);

  g_list_free_full (priv->routes, (GDestroyNotify) xb_router_route_free);

  G_OBJECT_CLASS (xb_router_parent_class)->finalize (object);
}

static void
xb_router_class_init (XbRouterClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  gobject_class->finalize = xb_router_finalize;

  signals[PATH_NOT_HANDLED] =
    g_signal_new ("path-not-handled",
                  XB_TYPE_ROUTER,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
xb_router_init (XbRouter *self)
{
}

void
xb_router_handle_route (XbRouter *self,
                        const gchar *method,
                        const gchar *path,
                        SoupMessage *message,
                        GHashTable *query)
{
  XbRouterPrivate *priv = xb_router_get_instance_private (self);
  GList *l, *k;
  GMatchInfo *match_info;
  XbRouterRoute *route;
  GHashTable *params;
  gchar *word;
  gint idx;
  gint64 start_time, end_time;

  start_time = g_get_monotonic_time ();

  if (method == NULL && message != NULL)
    method = message->method;

  for (l = priv->routes; l != NULL; l = l->next)
    {
      route = l->data;
      if (method != route->method)
        continue;

      if (!g_regex_match (route->path_regex, path, 0, &match_info))
        continue;

      /* We found our handler */
      params = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

      if (route->path_spec_keys != NULL)
        {
          /* For every key in the path_spec, get their value in the matched
           * path, and build a (path_spec_key -> value) dictionary.
           */
          for (idx = 1, k = route->path_spec_keys;
               idx < g_match_info_get_match_count (match_info) && k != NULL;
               idx++, k = k->next)
            {
              word = g_match_info_fetch (match_info, idx);
              g_hash_table_insert (params, g_strdup (k->data), word);
            }
        }

      route->callback (params, query, message, route->user_data);
      g_hash_table_unref (params);

      end_time = g_get_monotonic_time ();
      g_info ("%s %s handled in %.3f ms", method, path,
              (end_time - start_time) / 1000.0);

      return;
    }

  /* No handlers were found */
  g_info ("%s %s not handled", method, path);
  g_signal_emit (self, signals[PATH_NOT_HANDLED], 0);
}

/* Caller is responsible to keep user_data alive during the
 * lifecycle of the XbRouter.
 */
void
xb_router_add_route (XbRouter *self,
                     const gchar *method,
                     const gchar *path,
                     XbRouterCallback callback,
                     gpointer user_data)
{
  XbRouterRoute *route;
  XbRouterPrivate *priv = xb_router_get_instance_private (self);

  route = xb_router_route_new (method, path, callback, user_data);
  priv->routes = g_list_append (priv->routes, route);
}

XbRouter *
xb_router_new (void)
{
  return g_object_new (XB_TYPE_ROUTER, NULL);
}
