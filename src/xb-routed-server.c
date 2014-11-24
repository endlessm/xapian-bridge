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

#include "xb-routed-server.h"

typedef struct {
  XbRouter *router;
} XbRoutedServerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XbRoutedServer, xb_routed_server, SOUP_TYPE_SERVER);

static void
xb_routed_server_finalize (GObject *object)
{
  XbRoutedServer *self = XB_ROUTED_SERVER (object);
  XbRoutedServerPrivate *priv = xb_routed_server_get_instance_private (self);

  g_clear_object (&priv->router);

  G_OBJECT_CLASS (xb_routed_server_parent_class)->finalize (object);
}

static void
xb_routed_server_class_init (XbRoutedServerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  gobject_class->finalize = xb_routed_server_finalize;
}

static void
routed_server_handler (SoupServer *server,
                       SoupMessage *msg,
                       const char *path,
                       GHashTable *query,
                       SoupClientContext *client,
                       gpointer user_data)
{
  XbRoutedServer *self = user_data;
  XbRoutedServerPrivate *priv = xb_routed_server_get_instance_private (self);

  xb_router_handle_route (priv->router, msg->method, path, msg, query);
}

static void
xb_routed_server_init (XbRoutedServer *self)
{
  XbRoutedServerPrivate *priv = xb_routed_server_get_instance_private (self);
  priv->router = xb_router_new ();

  /* Setup a singleton handler for all HTTP requests; this will use the
   * router to actually delegate requests to each method handler.
   */
  soup_server_add_handler (SOUP_SERVER (self), NULL, routed_server_handler, self, NULL);
}

void
xb_routed_server_get (XbRoutedServer *self,
                      const gchar *path,
                      XbRouterCallback callback,
                      gpointer user_data)
{
  XbRoutedServerPrivate *priv = xb_routed_server_get_instance_private (self);
  xb_router_add_route (priv->router, SOUP_METHOD_GET, path, callback, user_data);
}

void
xb_routed_server_put (XbRoutedServer *self,
                      const gchar *path,
                      XbRouterCallback callback,
                      gpointer user_data)
{
  XbRoutedServerPrivate *priv = xb_routed_server_get_instance_private (self);
  xb_router_add_route (priv->router, SOUP_METHOD_PUT, path, callback, user_data);
}

void
xb_routed_server_delete (XbRoutedServer *self,
                         const gchar *path,
                         XbRouterCallback callback,
                         gpointer user_data)
{
  XbRoutedServerPrivate *priv = xb_routed_server_get_instance_private (self);
  xb_router_add_route (priv->router, SOUP_METHOD_DELETE, path, callback, user_data);
}

void
xb_routed_server_post (XbRoutedServer *self,
                       const gchar *path,
                       XbRouterCallback callback,
                       gpointer user_data)
{
  XbRoutedServerPrivate *priv = xb_routed_server_get_instance_private (self);
  xb_router_add_route (priv->router, SOUP_METHOD_POST, path, callback, user_data);
}

XbRoutedServer *
xb_routed_server_new (void)
{
  return g_object_new (XB_TYPE_ROUTED_SERVER, NULL);
}
