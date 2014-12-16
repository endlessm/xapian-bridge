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

#ifndef __XB_DATABASE_MANAGER_H__
#define __XB_DATABASE_MANAGER_H__

#include <glib-object.h>
#include <json-glib/json-glib.h>
#include <xapian-glib.h>

G_BEGIN_DECLS

#define XB_TYPE_DATABASE_MANAGER (xb_database_manager_get_type())
#define XB_DATABASE_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), XB_TYPE_DATABASE_MANAGER, XbDatabaseManager))
#define XB_DATABASE_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), XB_TYPE_DATABASE_MANAGER, XbDatabaseManagerClass))
#define XB_IS_DATABASE_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XB_TYPE_DATABASE_MANAGER))
#define XB_IS_DATABASE_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XB_TYPE_DATABASE_MANAGER))
#define XB_DATABASE_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), XB_TYPE_DATABASE_MANAGER, XbDatabaseManagerClass))

typedef struct _XbDatabaseManager      XbDatabaseManager;
typedef struct _XbDatabaseManagerClass XbDatabaseManagerClass;

struct _XbDatabaseManagerClass
{
    GObjectClass parent_class;
};

struct _XbDatabaseManager
{
    GObject parent;
};

GType xb_database_manager_get_type (void) G_GNUC_CONST;

XbDatabaseManager *xb_database_manager_new (void);

gboolean xb_database_manager_create_db (XbDatabaseManager *self,
                                        const gchar *path,
                                        const gchar *lang,
                                        GError **error_out);

JsonObject *xb_database_manager_query_db (XbDatabaseManager *self,
                                          const gchar *path,
                                          GHashTable *query,
                                          GError **error_out);

G_END_DECLS

#endif /* __XB_DATABASE_MANAGER_H__ */
