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

#ifndef __XB_ROUTER_H__
#define __XB_ROUTER_H__

#include <glib-object.h>
#include <libsoup/soup.h>

G_BEGIN_DECLS

#define XB_TYPE_ROUTER (xb_router_get_type())
#define XB_ROUTER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), XB_TYPE_ROUTER, XbRouter))
#define XB_ROUTER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), XB_TYPE_ROUTER, XbRouterClass))
#define XB_IS_ROUTER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XB_TYPE_ROUTER))
#define XB_IS_ROUTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XB_TYPE_ROUTER))
#define XB_ROUTER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), XB_TYPE_ROUTER, XbRouterClass))

typedef struct _XbRouter      XbRouter;
typedef struct _XbRouterClass XbRouterClass;

typedef void (* XbRouterCallback) (GHashTable *params,
                                   GHashTable *query,
                                   SoupMessage *message,
                                   gpointer user_data);

struct _XbRouterClass
{
    GObjectClass parent_class;
};

struct _XbRouter
{
    GObject parent;
};

GType xb_router_get_type (void) G_GNUC_CONST;

XbRouter *xb_router_new (void);

void xb_router_add_route (XbRouter *self,
                          const gchar *method,
                          const gchar *path,
                          XbRouterCallback callback,
                          gpointer user_data);

void xb_router_handle_route (XbRouter *self,
                             const gchar *method,
                             const gchar *path,
                             SoupMessage *message,
                             GHashTable *query);

G_END_DECLS

#endif /* __XB_ROUTER_H__ */
