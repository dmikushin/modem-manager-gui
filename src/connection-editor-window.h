/*
 *      connection-editor-window.h
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

#ifndef __CONNECTION_EDITOR_WINDOW_H__
#define __CONNECTION_EDITOR_WINDOW_H__

#include <gtk/gtk.h>

#include "main.h"

enum _mmgui_connection_editor_window_list_columns {
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION = 0,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_UUID,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NUMBER,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_USERNAME,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_PASSWORD,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_APN,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NETWORK_ID,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_TYPE,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_HOME_ONLY,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS1,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS2,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED,
	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_COLUMNS
};

void mmgui_main_connection_editor_window_open(mmgui_application_t mmguiapp);
void mmgui_main_connection_editor_window_list_init(mmgui_application_t mmguiapp);
void mmgui_main_connection_editor_window_list_fill(mmgui_application_t mmguiapp);

#endif /* __CONNECTION_EDITOR_WINDOW_H__ */
