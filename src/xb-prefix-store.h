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

#ifndef __XB_PREFIX_STORE_H__
#define __XB_PREFIX_STORE_H__

#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define XB_TYPE_PREFIX_STORE (xb_prefix_store_get_type())
#define XB_PREFIX_STORE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), XB_TYPE_PREFIX_STORE, XbPrefixStore))
#define XB_PREFIX_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), XB_TYPE_PREFIX_STORE, XbPrefixStoreClass))
#define IS_XB_PREFIX_STORE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XB_TYPE_PREFIX_STORE))
#define IS_XB_PREFIX_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XB_TYPE_PREFIX_STORE))
#define XB_PREFIX_STORE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), XB_TYPE_PREFIX_STORE, XbPrefixStoreClass))

typedef struct _XbPrefixStore      XbPrefixStore;
typedef struct _XbPrefixStoreClass XbPrefixStoreClass;

struct _XbPrefixStoreClass
{
    GObjectClass parent_class;
};

struct _XbPrefixStore
{
    GObject parent;
};

GType xb_prefix_store_get_type (void) G_GNUC_CONST;

XbPrefixStore *xb_prefix_store_new (void);

void xb_prefix_store_store_prefix_map (XbPrefixStore *self,
                                       const gchar *lang,
                                       JsonObject *object);

JsonObject *xb_prefix_store_get_all (XbPrefixStore *self);
JsonObject *xb_prefix_store_get (XbPrefixStore *self,
                                 const gchar *lang);

void xb_prefix_store_store_prefix (XbPrefixStore *self,
                                   const gchar *lang,
                                   JsonObject *object);
void xb_prefix_store_store_boolean_prefix (XbPrefixStore *self,
                                           const gchar *lang,
                                           JsonObject *object);

G_END_DECLS

#endif /* __XB_PREFIX_STORE_H__ */
