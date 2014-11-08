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

#ifndef __XB_ROUTED_SERVER_H__
#define __XB_ROUTED_SERVER_H__

#include <glib-object.h>
#include <libsoup/soup.h>

#include "xb-router.h"

G_BEGIN_DECLS

#define XB_TYPE_ROUTED_SERVER (xb_routed_server_get_type())
#define XB_ROUTED_SERVER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), XB_TYPE_ROUTED_SERVER, XbRoutedServer))
#define XB_ROUTED_SERVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), XB_TYPE_ROUTED_SERVER, XbRoutedServerClass))
#define XB_IS_ROUTED_SERVER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XB_TYPE_ROUTED_SERVER))
#define XB_IS_ROUTED_SERVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XB_TYPE_ROUTED_SERVER))
#define XB_ROUTED_SERVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), XB_TYPE_ROUTED_SERVER, XbRoutedServerClass))

typedef struct _XbRoutedServer      XbRoutedServer;
typedef struct _XbRoutedServerClass XbRoutedServerClass;

struct _XbRoutedServerClass
{
    SoupServerClass parent_class;
};

struct _XbRoutedServer
{
    SoupServer parent;
};

GType xb_routed_server_get_type (void) G_GNUC_CONST;

XbRoutedServer *xb_routed_server_new (void);

void xb_routed_server_get (XbRoutedServer *self,
                           const gchar *path,
                           XbRouterCallback callback,
                           gpointer user_data);
void xb_routed_server_put (XbRoutedServer *self,
                           const gchar *path,
                           XbRouterCallback callback,
                           gpointer user_data);
void xb_routed_server_delete (XbRoutedServer *self,
                              const gchar *path,
                              XbRouterCallback callback,
                              gpointer user_data);
void xb_routed_server_post (XbRoutedServer *self,
                            const gchar *path,
                            XbRouterCallback callback,
                            gpointer user_data);

G_END_DECLS

#endif /* __XB_ROUTED_SERVER_H__ */
