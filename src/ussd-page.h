/*
 *      ussd-page.h
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
 
 
#ifndef __USSD_PAGE_H__
#define __USSD_PAGE_H__

#include <gtk/gtk.h>

#include "main.h"

enum _mmgui_main_ussdlist_columns {
	MMGUI_MAIN_USSDLIST_COMMAND = 0,
	MMGUI_MAIN_USSDLIST_DESCRIPTION,
	MMGUI_MAIN_USSDLIST_COLUMNS
};

//USSD
void mmgui_main_ussd_command_add_button_clicked_signal(GObject *object, gpointer data);
void mmgui_main_ussd_command_remove_button_clicked_signal(GObject *object, gpointer data);
void mmgui_main_ussd_menu_update_callback(gchar *command, gchar *description, gboolean reencode, gpointer data);
void mmgui_main_ussd_edit(mmgui_application_t mmguiapp);
void mmgui_main_ussd_edit_button_clicked_signal(GtkEditable *editable, gpointer data);
void mmgui_main_ussd_command_combobox_changed_signal(GObject *object, gpointer data);
void mmgui_main_ussd_command_entry_changed_signal(GtkEditable *editable, gpointer data);
void mmgui_main_ussd_command_entry_activated_signal(GtkEntry *entry, gpointer data);
void mmgui_main_ussd_send_button_clicked_signal(GtkButton *button, gpointer data);
void mmgui_main_ussd_request_send(mmgui_application_t mmguiapp);
void mmgui_main_ussd_request_send_end(mmgui_application_t mmguiapp, mmguicore_t mmguicore, const gchar *answer);
void mmgui_main_ussd_restore_settings_for_modem(mmgui_application_t mmguiapp);
void mmgui_main_ussd_accelerators_init(mmgui_application_t mmguiapp);
void mmgui_main_ussd_list_init(mmgui_application_t mmguiapp);
void mmgui_main_ussd_state_clear(mmgui_application_t mmguiapp);

#endif /* __USSD_PAGE_H__ */
