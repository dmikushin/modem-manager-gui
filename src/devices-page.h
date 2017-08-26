/*
 *      devices-page.h
 *      
 *      Copyright 2012-2014 Alex <alex@linuxonly.ru>
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
 
 
#ifndef __DEVICES_PAGE_H__
#define __DEVICES_PAGE_H__

#include <gtk/gtk.h>

#include "main.h"

enum _mmgui_main_devlist_columns {
	MMGUI_MAIN_DEVLIST_ENABLED = 0,
	MMGUI_MAIN_DEVLIST_DESCRIPTION,
	MMGUI_MAIN_DEVLIST_ID,
	MMGUI_MAIN_DEVLIST_IDENTIFIER,
	MMGUI_MAIN_DEVLIST_COLUMNS
};

/*Devices*/
gboolean mmgui_main_device_handle_added_from_thread(gpointer data);
gboolean mmgui_main_device_handle_removed_from_thread(gpointer data);
void mmgui_main_device_handle_enabled_local_status(mmgui_application_t mmguiapp);
gboolean mmgui_main_device_handle_enabled_status_from_thread(gpointer data);
gboolean mmgui_main_device_handle_blocked_status_from_thread(gpointer data);
gboolean mmgui_main_device_handle_prepared_status_from_thread(gpointer data);
gboolean mmgui_main_device_handle_connection_status_from_thread(gpointer data);
gboolean mmgui_main_device_select_from_list(mmgui_application_t mmguiapp, gchar *identifier);
void mmgui_main_device_list_fill(mmgui_application_t mmguiapp);
void mmgui_main_device_list_init(mmgui_application_t mmguiapp);

#endif /* __DEVICES_PAGE_H__ */
