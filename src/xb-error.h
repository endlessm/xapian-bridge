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

#ifndef __XB_ERROR_H__
#define __XB_ERROR_H__

typedef enum {
  XB_ERROR_DATABASE_NOT_FOUND,
  XB_ERROR_INVALID_PATH,
  XB_ERROR_UNSUPPORTED_LANG,
  XB_ERROR_INVALID_PARAMS
} XbError;

#define XB_ERROR xb_error_quark()
GQuark xb_error_quark (void);

#endif /* __XB_ERROR_H__ */
