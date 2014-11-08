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

#ifndef __XB_DATABASE_CACHE_H__
#define __XB_DATABASE_CACHE_H__

#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define XB_TYPE_DATABASE_CACHE (xb_database_cache_get_type())
#define XB_DATABASE_CACHE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), XB_TYPE_DATABASE_CACHE, XbDatabaseCache))
#define XB_DATABASE_CACHE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), XB_TYPE_DATABASE_CACHE, XbDatabaseCacheClass))
#define XB_IS_DATABASE_CACHE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XB_TYPE_DATABASE_CACHE))
#define XB_IS_DATABASE_CACHE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XB_TYPE_DATABASE_CACHE))
#define XB_DATABASE_CACHE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), XB_TYPE_DATABASE_CACHE, XbDatabaseCacheClass))

typedef struct _XbDatabaseCache      XbDatabaseCache;
typedef struct _XbDatabaseCacheClass XbDatabaseCacheClass;

struct _XbDatabaseCacheClass
{
    GObjectClass parent_class;
};

struct _XbDatabaseCache
{
    GObject parent;
};

GType xb_database_cache_get_type (void) G_GNUC_CONST;

XbDatabaseCache *xb_database_cache_new (void);
XbDatabaseCache *xb_database_cache_new_with_cache_dir (const gchar *cache_dir);

const gchar *xb_database_cache_get_cache_dir (XbDatabaseCache *self);
void xb_database_cache_save (XbDatabaseCache *self);

JsonObject *xb_database_cache_get_entries (XbDatabaseCache *self);
void xb_database_cache_set_entry (XbDatabaseCache *self,
                                  const gchar *name,
                                  const gchar *path,
                                  const gchar *language);
void xb_database_cache_remove_entry (XbDatabaseCache *self,
                                     const gchar *name);

G_END_DECLS

#endif /* __XB_DATABASE_CACHE_H__ */
