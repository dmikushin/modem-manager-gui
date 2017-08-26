/*
 *      contacts-page.h
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
 
 
#ifndef __CONTACTS_PAGE_H__
#define __CONTACTS_PAGE_H__

#include <gtk/gtk.h>

#include "main.h"

enum _mmgui_main_contactslist_columns {
	MMGUI_MAIN_CONTACTSLIST_NAME = 0,
	MMGUI_MAIN_CONTACTSLIST_NUMBER,
	MMGUI_MAIN_CONTACTSLIST_EMAIL,
	MMGUI_MAIN_CONTACTSLIST_GROUP,
	MMGUI_MAIN_CONTACTSLIST_NAME2,
	MMGUI_MAIN_CONTACTSLIST_NUMBER2,
	MMGUI_MAIN_CONTACTSLIST_HIDDEN,
	MMGUI_MAIN_CONTACTSLIST_STORAGE,
	MMGUI_MAIN_CONTACTSLIST_ID,
	MMGUI_MAIN_CONTACTSLIST_TYPE,
	MMGUI_MAIN_CONTACTSLIST_COLUMNS
};

enum _mmgui_main_contact_type {
	MMGUI_MAIN_CONTACT_UNKNOWN = 0,
	MMGUI_MAIN_CONTACT_HEADER,
	MMGUI_MAIN_CONTACT_MODEM,
	MMGUI_MAIN_CONTACT_GNOME,
	MMGUI_MAIN_CONTACT_KDE
};

//CONTACTS
void mmgui_main_contacts_sms(mmgui_application_t mmguiapp);
void mmgui_main_contacts_sms_button_clicked_signal(GObject *object, gpointer data);
void mmgui_main_contacts_new(mmgui_application_t mmguiapp);
void mmgui_main_contacts_new_button_clicked_signal(GObject *object, gpointer user_data);
void mmgui_main_contacts_remove(mmgui_application_t mmguiapp);
void mmgui_main_contacts_remove_button_clicked_signal(GObject *object, gpointer user_data);
void mmgui_main_contacts_list_fill(mmgui_application_t mmguiapp);
void mmgui_main_contacts_addressbook_list_fill(mmgui_application_t mmguiapp, guint contacttype);
void mmgui_main_contacts_load_from_system_addressbooks(mmgui_application_t mmguiapp);
void mmgui_main_contacts_list_init(mmgui_application_t mmguiapp);
void mmgui_main_contacts_state_clear(mmgui_application_t mmguiapp);

#endif /* __CONTACTS_PAGE_H__ */
