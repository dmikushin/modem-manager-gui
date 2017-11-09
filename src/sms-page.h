/*
 *      sms-page.h
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
 
 
#ifndef __SMS_PAGE_H__
#define __SMS_PAGE_H__

#include <gtk/gtk.h>

#include "../resources.h"
#include "main.h"

/*SMS*/
gboolean mmgui_main_sms_get_message_list_from_thread(gpointer data);
gboolean mmgui_main_sms_get_message_from_thread(gpointer data);
gboolean mmgui_main_sms_handle_new_day_from_thread(gpointer data);
gboolean mmgui_main_sms_send(mmgui_application_t mmguiapp, const gchar *number, const gchar *text);
void mmgui_main_sms_remove(mmgui_application_t mmguiapp);
void mmgui_main_sms_remove_button_clicked_signal(GObject *object, gpointer data);
void mmgui_main_sms_new(mmgui_application_t mmguiapp);
void mmgui_main_sms_new_button_clicked_signal(GObject *object, gpointer data);
void mmgui_main_sms_answer(mmgui_application_t mmguiapp);
void mmgui_main_sms_answer_button_clicked_signal(GObject *object, gpointer data);
gboolean mmgui_main_sms_list_fill(mmgui_application_t mmguiapp);
void mmgui_main_sms_load_contacts_from_system_addressbooks(mmgui_application_t mmguiapp);
void mmgui_main_sms_restore_settings_for_modem(mmgui_application_t mmguiapp);
void mmgui_main_sms_restore_contacts_for_modem(mmgui_application_t mmguiapp);
#if RESOURCE_SPELLCHECKER_ENABLED
gboolean mmgui_main_sms_spellcheck_init(mmgui_application_t mmguiapp);
#endif
void mmgui_main_sms_list_init(mmgui_application_t mmguiapp);
void mmgui_main_sms_list_clear(mmgui_application_t mmguiapp);

#endif /* __SMS_PAGE_H__ */
