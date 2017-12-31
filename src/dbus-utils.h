/*
 *      dbus-utils.h
 *      
 *      Copyright 2017 Alex <alex@linuxonly.ru>
 *      
 *      This program is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 3 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DBUS_UTILS_H__
#define __DBUS_UTILS_H__

gboolean mmgui_dbus_utils_session_service_activate(gchar *interface, guint *status);
GHashTable *mmgui_dbus_utils_list_service_interfaces(GDBusConnection *connection, gchar *servicename, gchar *servicepath);

#endif /* __DBUS_UTILS_H__ */
