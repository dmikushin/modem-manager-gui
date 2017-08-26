/*
 *      scan-page.h
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
 
 
#ifndef __SCAN_PAGE_H__
#define __SCAN_PAGE_H__

#include <gtk/gtk.h>

#include "main.h"

enum _mmgui_main_scanlist_columns {
	MMGUI_MAIN_SCANLIST_OPERATOR = 0,
	MMGUI_MAIN_SCANLIST_NAME,
	MMGUI_MAIN_SCANLIST_IDENIFIER,
	MMGUI_MAIN_SCANLIST_AVAILABILITY,
	MMGUI_MAIN_SCANLIST_COLUMNS
};

/*SCAN*/
void mmgui_main_scan_start(mmgui_application_t mmguiapp);
void mmgui_main_scan_start_button_clicked_signal(GObject *object, gpointer data);
void mmgui_main_scan_list_fill(mmgui_application_t mmguiapp, mmguicore_t mmguicore, GSList *netlist);
void mmgui_main_scan_list_init(mmgui_application_t mmguiapp);
void mmgui_main_scan_state_clear(mmgui_application_t mmguiapp);

#endif /* __SCAN_PAGE_H__ */
