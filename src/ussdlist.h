/*
 *      ussdlist.h
 *      
 *      Copyright 2012 Alex <alex@linuxonly.ru>
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

#ifndef __USSDLIST_H__
#define __USSDLIST_H__

#include <gtk/gtk.h>

struct _ussdlist_entry {
	gchar *command;
	gchar *description;
};

typedef struct _ussdlist_entry *ussdlist_entry_t;

typedef void (*ussdlist_read_callback)(gchar *command, gchar *description, gboolean reencode, gpointer data);

gboolean ussdlist_read_commands(ussdlist_read_callback callback, const gchar *persistentid, const gchar *internalid, gpointer data);
gboolean ussdlist_start_xml_export(gboolean reencode);
gboolean ussdlist_add_command_to_xml_export(gchar *command, gchar *description);
gboolean ussdlist_end_xml_export(const gchar *persistentid);

#endif /* __USSDLIST_H__ */
