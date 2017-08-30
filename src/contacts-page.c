/*
 *      contacts-page.c
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

#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <locale.h>
#include <libintl.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gprintf.h>

#include "mmguicore.h"
#include "resources.h"

#include "sms-page.h"
#include "contacts-page.h"
#include "main.h"


static mmgui_contact_t mmgui_main_contacts_list_get_selected(mmgui_application_t mmguiapp, guint *type);
static void mmgui_main_contacts_list_cursor_changed_signal(GtkTreeView *tree_view, gpointer data);
static void mmgui_main_contacts_sms_menu_activate_signal(GtkMenuItem *menuitem, gpointer data);
static void mmgui_main_contacts_dialog_entry_changed_signal(GtkEditable *editable, gpointer data);


/*CONTACTS*/
static mmgui_contact_t mmgui_main_contacts_list_get_selected(mmgui_application_t mmguiapp, guint *type)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	guint id, contacttype;
	mmgui_contact_t contact;
	
	if (mmguiapp == NULL) return NULL;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contactstreeview));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->contactstreeview));
	
	contact = NULL;
	
	if ((model != NULL) && (selection != NULL)) {
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_tree_model_get(model, &iter, MMGUI_MAIN_CONTACTSLIST_ID, &id, MMGUI_MAIN_CONTACTSLIST_TYPE, &contacttype, -1);
			if (contacttype != MMGUI_MAIN_CONTACT_HEADER) {
				if (contacttype == MMGUI_MAIN_CONTACT_MODEM) {
					//Contact from modem
					contact = mmguicore_contacts_get(mmguiapp->core, id);
				} else if (contacttype == MMGUI_MAIN_CONTACT_GNOME) {
					//Contact from GNOME addressbook
					contact = mmgui_addressbooks_get_gnome_contact(mmguiapp->addressbooks, id);
				} else if (contacttype == MMGUI_MAIN_CONTACT_KDE) {
					//Contact from KDE addressbook
					contact = mmgui_addressbooks_get_kde_contact(mmguiapp->addressbooks, id);
				}
			}
			//Set contact type if needed
			if (type != NULL) *(type) = contacttype;
		} else {
			if (type != NULL) *(type) = MMGUI_MAIN_CONTACT_UNKNOWN;
		}
	} else {
		if (type != NULL) *(type) = MMGUI_MAIN_CONTACT_UNKNOWN;
	}
		
	return contact;
}

static void mmgui_main_contacts_list_cursor_changed_signal(GtkTreeView *tree_view, gpointer data)
{
	mmgui_application_t mmguiapp;
	mmgui_contact_t contact;
	guint contactscaps;
	gboolean validated;
	GtkWidget *menu_sms1, *menu_sms2;
	guint contacttype;
	static struct _mmgui_application_data appdata[2];
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	validated = FALSE;
	contacttype = MMGUI_MAIN_CONTACT_UNKNOWN;
	
	contact = mmgui_main_contacts_list_get_selected(mmguiapp, &contacttype);
	
	if ((contact != NULL) && (contacttype != MMGUI_MAIN_CONTACT_UNKNOWN)) {
		if (contacttype != MMGUI_MAIN_CONTACT_HEADER) {
			//Destroy old menu
			if (mmguiapp->window->contactssmsmenu != NULL) {
				gtk_widget_destroy(mmguiapp->window->contactssmsmenu);
				mmguiapp->window->contactssmsmenu = NULL;
			}
			//Create new menu
			if (mmguiapp->options->smspageenabled) {
				mmguiapp->window->contactssmsmenu = gtk_menu_new();
			}
			//Contacts numbers validation						
			if ((contact->number != NULL) && (mmguicore_sms_validate_number((const gchar *)contact->number))) {
				if (mmguiapp->options->smspageenabled) {
					menu_sms1 = gtk_menu_item_new_with_label(contact->number);
					gtk_menu_shell_append(GTK_MENU_SHELL(mmguiapp->window->contactssmsmenu), menu_sms1);
					appdata[0].mmguiapp = mmguiapp;
					appdata[0].data = GINT_TO_POINTER(0);
					g_signal_connect(G_OBJECT(menu_sms1), "activate", G_CALLBACK(mmgui_main_contacts_sms_menu_activate_signal), &(appdata[0]));
				}
				validated = TRUE;
			}
			if ((contact->number2 != NULL) && (mmguicore_sms_validate_number((const gchar *)contact->number2))) {
				if (mmguiapp->options->smspageenabled) {
					menu_sms2 = gtk_menu_item_new_with_label(contact->number2);
					gtk_menu_shell_append(GTK_MENU_SHELL(mmguiapp->window->contactssmsmenu), menu_sms2);
					appdata[1].mmguiapp = mmguiapp;
					appdata[1].data = GINT_TO_POINTER(1);
					g_signal_connect(G_OBJECT(menu_sms2), "activate", G_CALLBACK(mmgui_main_contacts_sms_menu_activate_signal), &(appdata[1]));
				}
				validated = TRUE;
			}
			//Set buttons state
			if (validated) {
				contactscaps = mmguicore_contacts_get_capabilities(mmguiapp->core);
				if ((contactscaps & MMGUI_CONTACTS_CAPS_EDIT) && (contacttype == MMGUI_MAIN_CONTACT_MODEM)) {
					gtk_widget_set_sensitive(mmguiapp->window->removecontactbutton, TRUE);
				} else {
					gtk_widget_set_sensitive(mmguiapp->window->removecontactbutton, FALSE);
				}
				/*SMS send button*/
				if (mmguiapp->options->smspageenabled) {
					gtk_widget_set_sensitive(mmguiapp->window->smstocontactbutton, TRUE);
					//Show menu if contact data validated
					gtk_widget_show_all(GTK_WIDGET(mmguiapp->window->contactssmsmenu));
					gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(mmguiapp->window->smstocontactbutton), mmguiapp->window->contactssmsmenu);
				}
			} else {
				gtk_widget_set_sensitive(mmguiapp->window->removecontactbutton, FALSE);
				gtk_widget_set_sensitive(mmguiapp->window->smstocontactbutton, FALSE);
			}
		}
	} else if (contacttype == MMGUI_MAIN_CONTACT_HEADER) {
		//Header selected
		gtk_widget_set_sensitive(mmguiapp->window->removecontactbutton, FALSE);
		gtk_widget_set_sensitive(mmguiapp->window->smstocontactbutton, FALSE);
	}
}

void mmgui_main_contacts_sms(mmgui_application_t mmguiapp)
{
	mmgui_contact_t contact;
	gchar *number;
	guint smscaps, contacttype;
	
	if (mmguiapp == NULL) return;
	
	smscaps = mmguicore_sms_get_capabilities(mmguiapp->core);
	
	if (!(smscaps & MMGUI_SMS_CAPS_SEND)) return;
	
	number = NULL;
	contacttype = MMGUI_MAIN_CONTACT_UNKNOWN;
	
	contact = mmgui_main_contacts_list_get_selected(mmguiapp, &contacttype);
		
	if ((contact != NULL) && (contacttype != MMGUI_MAIN_CONTACT_UNKNOWN)) {
		//Find apporitate number
		if (contact->number != NULL) {
			number = contact->number;
		} else if (contact->number2 != NULL) {
			number = contact->number2;
		}
		//Switch to SMS page and send message
		if (number != NULL) {
			gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(mmguiapp->window->smsbutton), TRUE);
			mmgui_main_sms_send(mmguiapp, number, "");
		}
	}
	
}

void mmgui_main_contacts_sms_button_clicked_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_contacts_sms(mmguiapp);
}

static void mmgui_main_contacts_sms_menu_activate_signal(GtkMenuItem *menuitem, gpointer data)
{
	mmgui_application_data_t appdata;
	mmgui_contact_t contact;
	guint contacttype;
	
	appdata = (mmgui_application_data_t)data;
	
	if (appdata == NULL) return;
	
	contacttype = MMGUI_MAIN_CONTACT_UNKNOWN;
	
	contact = mmgui_main_contacts_list_get_selected(appdata->mmguiapp, &contacttype);
		
	if ((contact != NULL) && (contacttype != MMGUI_MAIN_CONTACT_UNKNOWN)) {
		if ((GPOINTER_TO_INT(appdata->data) == 0) && (contact->number != NULL)) {
			//First number: switch to SMS page and send message if number found
			gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(appdata->mmguiapp->window->smsbutton), TRUE);
			mmgui_main_sms_send(appdata->mmguiapp, contact->number, "");
		} else if ((GPOINTER_TO_INT(appdata->data) == 1) && (contact->number2 != NULL)) {
			//Second number: switch to SMS page and send message if number found
			gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(appdata->mmguiapp->window->smsbutton), TRUE);
			mmgui_main_sms_send(appdata->mmguiapp, contact->number2, "");
		}
	}
}

static void mmgui_main_contacts_dialog_entry_changed_signal(GtkEditable *editable, gpointer data)
{
	mmgui_application_t mmguiapp;
	guint16 namelen, numberlen, number2len;
	const gchar *number, *number2;
	gboolean valid;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	namelen = gtk_entry_get_text_length(GTK_ENTRY(mmguiapp->window->contactnameentry));
	numberlen = gtk_entry_get_text_length(GTK_ENTRY(mmguiapp->window->contactnumberentry));
	number2len = gtk_entry_get_text_length(GTK_ENTRY(mmguiapp->window->contactnumber2entry));
	
	if ((namelen == 0) || (numberlen == 0)) {
		gtk_widget_set_sensitive(mmguiapp->window->newcontactaddbutton, FALSE);
	} else {
		valid = TRUE;
		//Number2 validation
		if (number2len > 0) {
			number2 = gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->contactnumber2entry));
			if (!mmguicore_sms_validate_number(number2)) {
				valid = FALSE;
			}
		}
		//Number validation
		if (valid) {
			number = gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->contactnumberentry));
			if (!mmguicore_sms_validate_number(number)) {
				valid = FALSE;
			}
		}
		//Set state of add button
		if (valid) {
			gtk_widget_set_sensitive(mmguiapp->window->newcontactaddbutton, TRUE);
		} else {
			gtk_widget_set_sensitive(mmguiapp->window->newcontactaddbutton, FALSE);
		}
	}
}

void mmgui_main_contacts_new(mmgui_application_t mmguiapp)
{
	guint contactscaps;
	gboolean extsensitive;
	gulong editsignal[3];
	gint response;
	mmgui_contact_t contact;
	GtkTreeModel *model;
	GtkTreeIter iter, child;
	
	if (mmguiapp == NULL) return;
	
	contactscaps = mmguicore_contacts_get_capabilities(mmguiapp->core);
	
	if (!(contactscaps & MMGUI_CONTACTS_CAPS_EDIT)) return;
	
	/*Capabilities*/
	extsensitive = (gboolean)(contactscaps & MMGUI_CONTACTS_CAPS_EXTENDED);
	gtk_widget_set_sensitive(mmguiapp->window->contactemailentry, extsensitive);
	gtk_widget_set_sensitive(mmguiapp->window->contactgroupentry, extsensitive);
	gtk_widget_set_sensitive(mmguiapp->window->contactname2entry, extsensitive);
	gtk_widget_set_sensitive(mmguiapp->window->contactnumber2entry, extsensitive);
	/*Clear entries*/
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->contactnameentry), "");
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->contactnumberentry), "");
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->contactemailentry), "");
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->contactgroupentry), "");
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->contactname2entry), "");
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->contactnumber2entry), "");
	/*Bind signals*/
	editsignal[0] = g_signal_connect(G_OBJECT(mmguiapp->window->contactnameentry), "changed", G_CALLBACK(mmgui_main_contacts_dialog_entry_changed_signal), mmguiapp);
	editsignal[1] = g_signal_connect(G_OBJECT(mmguiapp->window->contactnumberentry), "changed", G_CALLBACK(mmgui_main_contacts_dialog_entry_changed_signal), mmguiapp);
	editsignal[2] = g_signal_connect(G_OBJECT(mmguiapp->window->contactnumber2entry), "changed", G_CALLBACK(mmgui_main_contacts_dialog_entry_changed_signal), mmguiapp);
	g_signal_emit_by_name(G_OBJECT(mmguiapp->window->contactnameentry), "changed");
	/*Run dialog*/
	response = gtk_dialog_run(GTK_DIALOG(mmguiapp->window->newcontactdialog));
	/*Unbind signals*/
	g_signal_handler_disconnect(G_OBJECT(mmguiapp->window->contactnameentry), editsignal[0]);
	g_signal_handler_disconnect(G_OBJECT(mmguiapp->window->contactnumberentry), editsignal[1]);
	g_signal_handler_disconnect(G_OBJECT(mmguiapp->window->contactnumber2entry), editsignal[2]);
	gtk_widget_hide(mmguiapp->window->newcontactdialog);
	/*Add contact*/
	if (response) {
		/*Form contact*/
		contact = (mmgui_contact_t)g_new0(struct _mmgui_contact, 1);
		contact->name = g_strdup(gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->contactnameentry)));
		contact->number = g_strdup(gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->contactnumberentry)));
		contact->email = g_strdup(gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->contactemailentry)));
		contact->group = g_strdup(gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->contactgroupentry)));
		contact->name2 = g_strdup(gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->contactname2entry)));
		contact->number2 = g_strdup(gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->contactnumber2entry)));
		contact->hidden = FALSE;
		contact->storage = 0;
		/*Add to device*/
		if (mmguicore_contacts_add(mmguiapp->core, contact)) {
			/*Add to list*/
			model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contactstreeview));
			if (model != NULL) {
				gtk_tree_model_get_iter(model, &iter, mmguiapp->window->contmodempath);
				gtk_tree_store_append(GTK_TREE_STORE(model), &child, &iter);
				gtk_tree_store_set(GTK_TREE_STORE(model), &child, MMGUI_MAIN_CONTACTSLIST_NAME, contact->name,
																MMGUI_MAIN_CONTACTSLIST_NUMBER, contact->number,
																MMGUI_MAIN_CONTACTSLIST_EMAIL, contact->email,
																MMGUI_MAIN_CONTACTSLIST_GROUP, contact->group,
																MMGUI_MAIN_CONTACTSLIST_NAME2, contact->name2,
																MMGUI_MAIN_CONTACTSLIST_NUMBER2, contact->number2,
																MMGUI_MAIN_CONTACTSLIST_HIDDEN, contact->hidden,
																MMGUI_MAIN_CONTACTSLIST_STORAGE, contact->storage,
																MMGUI_MAIN_CONTACTSLIST_ID, contact->id,
																MMGUI_MAIN_CONTACTSLIST_TYPE, MMGUI_MAIN_CONTACT_MODEM,
																-1);
			}
		} else {
			/*Can not add, free resources*/
			mmguicore_contacts_free_single(contact, TRUE);
			mmgui_main_ui_error_dialog_open(mmguiapp, _("<b>Error adding contact</b>"), mmguicore_get_last_error(mmguiapp->core));
		}
	}
}

void mmgui_main_contacts_new_button_clicked_signal(GObject *object, gpointer user_data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)user_data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_contacts_new(mmguiapp);
}

void mmgui_main_contacts_remove(mmgui_application_t mmguiapp)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	guint id, contacttype, contactcaps;
	
	if (mmguiapp == NULL) return;
	
	contactcaps = mmguicore_contacts_get_capabilities(mmguiapp->core);
	
	if (!(contactcaps & MMGUI_CONTACTS_CAPS_EDIT)) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contactstreeview));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->contactstreeview));
	
	if ((model != NULL) && (selection != NULL)) {
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_tree_model_get(model, &iter, MMGUI_MAIN_CONTACTSLIST_ID, &id, MMGUI_MAIN_CONTACTSLIST_TYPE, &contacttype, -1);
			if (contacttype == MMGUI_MAIN_CONTACT_MODEM) {
				if (mmgui_main_ui_question_dialog_open(mmguiapp, _("<b>Remove contact</b>"), _("Really want to remove contact?"))) {
					if (mmguicore_contacts_delete(mmguiapp->core, id)) {
						gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
					} else {
						mmgui_main_ui_error_dialog_open(mmguiapp, _("<b>Error removing contact</b>"), _("Contact not removed from device"));
					}
				}
			}
		} else {
			mmgui_main_ui_error_dialog_open(mmguiapp, _("<b>Error removing contact</b>"), _("Contact not selected"));
		}
	}
}

void mmgui_main_contacts_remove_button_clicked_signal(GObject *object, gpointer user_data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)user_data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_contacts_remove(mmguiapp);
}

void mmgui_main_contacts_list_fill(mmgui_application_t mmguiapp)
{
	guint contactscaps;
	GSList *contacts;
	GtkTreeModel *model;
	GtkTreeIter iter, child;
	GSList *iterator;
	mmgui_contact_t contact;
	
	if (mmguiapp == NULL) return;
	
	contactscaps = mmguicore_contacts_get_capabilities(mmguiapp->core);
	
	if (contactscaps & MMGUI_CONTACTS_CAPS_EXPORT) {
		contacts = mmguicore_contacts_list(mmguiapp->core);
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contactstreeview));
		if (model != NULL) {
			gtk_tree_store_insert(GTK_TREE_STORE(model), &iter, NULL, 0);
			gtk_tree_store_set(GTK_TREE_STORE(model), &iter, MMGUI_MAIN_CONTACTSLIST_NAME, _("<b>Modem contacts</b>"), MMGUI_MAIN_CONTACTSLIST_ID, 0, MMGUI_MAIN_CONTACTSLIST_TYPE, MMGUI_MAIN_CONTACT_HEADER, -1);
			mmguiapp->window->contmodempath = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &iter);
			if (contacts != NULL) {
				//Add contacts
				gtk_tree_model_get_iter(model, &iter, mmguiapp->window->contmodempath);
				for (iterator=contacts; iterator; iterator=iterator->next) {
					contact = iterator->data;
					gtk_tree_store_append(GTK_TREE_STORE(model), &child, &iter);
					gtk_tree_store_set(GTK_TREE_STORE(model), &child, MMGUI_MAIN_CONTACTSLIST_NAME, contact->name,
																	MMGUI_MAIN_CONTACTSLIST_NUMBER, contact->number,
																	MMGUI_MAIN_CONTACTSLIST_EMAIL, contact->email,
																	MMGUI_MAIN_CONTACTSLIST_GROUP, contact->group,
																	MMGUI_MAIN_CONTACTSLIST_NAME2, contact->name2,
																	MMGUI_MAIN_CONTACTSLIST_NUMBER2, contact->number2,
																	MMGUI_MAIN_CONTACTSLIST_HIDDEN, contact->hidden,
																	MMGUI_MAIN_CONTACTSLIST_STORAGE, contact->storage,
																	MMGUI_MAIN_CONTACTSLIST_ID, contact->id,
																	MMGUI_MAIN_CONTACTSLIST_TYPE, MMGUI_MAIN_CONTACT_MODEM,
																	-1);
				}
			}
			gtk_tree_view_expand_all(GTK_TREE_VIEW(mmguiapp->window->contactstreeview));
		}
	}
}

void mmgui_main_contacts_addressbook_list_fill(mmgui_application_t mmguiapp, guint contacttype)
{
	GSList *contacts;
	GtkTreeModel *model;
	GtkTreeIter iter, child;
	GSList *iterator;
	mmgui_contact_t contact;
	
	if (mmguiapp == NULL) return;
	
	contacts = NULL;
	
	if ((contacttype == MMGUI_MAIN_CONTACT_GNOME) && (mmguiapp->window->contgnomepath != NULL)) {
		//Contacts from GNOME addressbook
		if (mmgui_addressbooks_get_gnome_contacts_available(mmguiapp->addressbooks)) {
			contacts = mmgui_addressbooks_get_gnome_contacts_list(mmguiapp->addressbooks);
		}
	} else if (contacttype == MMGUI_MAIN_CONTACT_KDE) {
		//Contacts from KDE addressbook
		if (mmgui_addressbooks_get_kde_contacts_available(mmguiapp->addressbooks)) {
			contacts = mmgui_addressbooks_get_kde_contacts_list(mmguiapp->addressbooks);
		}
	}
	
	if (contacts != NULL) {
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contactstreeview));
		if (model != NULL) {
			g_object_ref(model);
			gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->contactstreeview), NULL);
			//Get patrent iterator
			if (contacttype == MMGUI_MAIN_CONTACT_GNOME) {
				gtk_tree_model_get_iter(model, &iter, mmguiapp->window->contgnomepath);
			} else if (contacttype == MMGUI_MAIN_CONTACT_KDE) {
				gtk_tree_model_get_iter(model, &iter, mmguiapp->window->contkdepath);
			}
			//Add contacts
			for (iterator=contacts; iterator; iterator=iterator->next) {
				contact = iterator->data;
				gtk_tree_store_append(GTK_TREE_STORE(model), &child, &iter);
				gtk_tree_store_set(GTK_TREE_STORE(model), &child, MMGUI_MAIN_CONTACTSLIST_NAME, contact->name,
																MMGUI_MAIN_CONTACTSLIST_NUMBER, contact->number,
																MMGUI_MAIN_CONTACTSLIST_EMAIL, contact->email,
																MMGUI_MAIN_CONTACTSLIST_GROUP, contact->group,
																MMGUI_MAIN_CONTACTSLIST_NAME2, contact->name2,
																MMGUI_MAIN_CONTACTSLIST_NUMBER2, contact->number2,
																MMGUI_MAIN_CONTACTSLIST_HIDDEN, contact->hidden,
																MMGUI_MAIN_CONTACTSLIST_STORAGE, contact->storage,
																MMGUI_MAIN_CONTACTSLIST_ID, contact->id,
																MMGUI_MAIN_CONTACTSLIST_TYPE, contacttype,
																-1);
			}
			//Attach model
			gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->contactstreeview), model);
			g_object_unref(model);
		}
		gtk_tree_view_expand_all(GTK_TREE_VIEW(mmguiapp->window->contactstreeview));
	}
}

void mmgui_main_contacts_load_from_system_addressbooks(mmgui_application_t mmguiapp)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contactstreeview));
	if (model) {
		if (mmgui_addressbooks_get_gnome_contacts_available(mmguiapp->addressbooks)) {
			gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
			gtk_tree_store_set(GTK_TREE_STORE(model), &iter, MMGUI_MAIN_CONTACTSLIST_NAME, _("<b>GNOME contacts</b>"), MMGUI_MAIN_CONTACTSLIST_ID, 0, MMGUI_MAIN_CONTACTSLIST_TYPE, MMGUI_MAIN_CONTACT_HEADER, -1);
			mmguiapp->window->contgnomepath = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &iter);
			mmgui_main_contacts_addressbook_list_fill(mmguiapp, MMGUI_MAIN_CONTACT_GNOME);
		}
		
		if (mmgui_addressbooks_get_kde_contacts_available(mmguiapp->addressbooks)) {
			gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
			gtk_tree_store_set(GTK_TREE_STORE(model), &iter, MMGUI_MAIN_CONTACTSLIST_NAME, _("<b>KDE contacts</b>"), MMGUI_MAIN_CONTACTSLIST_ID, 0, MMGUI_MAIN_CONTACTSLIST_TYPE, MMGUI_MAIN_CONTACT_HEADER, -1);
			mmguiapp->window->contkdepath = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &iter);
			mmgui_main_contacts_addressbook_list_fill(mmguiapp, MMGUI_MAIN_CONTACT_KDE);
		}
	}
}

void mmgui_main_contacts_list_init(mmgui_application_t mmguiapp)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeStore *store;
			
	if (mmguiapp == NULL) return;
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("First name"), renderer, "markup", MMGUI_MAIN_CONTACTSLIST_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->contactstreeview), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("First number"), renderer, "markup", MMGUI_MAIN_CONTACTSLIST_NUMBER, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->contactstreeview), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("EMail"), renderer, "markup", MMGUI_MAIN_CONTACTSLIST_EMAIL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->contactstreeview), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Group"), renderer, "markup", MMGUI_MAIN_CONTACTSLIST_GROUP, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->contactstreeview), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Second name"), renderer, "markup", MMGUI_MAIN_CONTACTSLIST_NAME2, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->contactstreeview), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Second number"), renderer, "markup", MMGUI_MAIN_CONTACTSLIST_NUMBER2, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->contactstreeview), column);
	
	store = gtk_tree_store_new(MMGUI_MAIN_CONTACTSLIST_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
	
	mmguiapp->window->contmodempath = NULL;
	
	gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->contactstreeview), GTK_TREE_MODEL(store));
	g_object_unref(store);
	
	g_signal_connect(G_OBJECT(mmguiapp->window->contactstreeview), "cursor-changed", G_CALLBACK(mmgui_main_contacts_list_cursor_changed_signal), mmguiapp);
	
	mmguiapp->window->contactssmsmenu = NULL;
}

void mmgui_main_contacts_state_clear(mmgui_application_t mmguiapp)
{
	GtkTreeModel *model;
	GtkTreeIter catiter, contiter;
	GtkTreePath *refpath;
	GtkTreeRowReference *reference;
	GSList *reflist, *iterator;
	gboolean validcat, validcont;
	guint contacttype, numcontacts;
	gchar *pathstr;
	
	if (mmguiapp == NULL) return;
	
	/*Clear contacts list*/
	if (mmguiapp->window->contmodempath != NULL) {
		pathstr = gtk_tree_path_to_string(mmguiapp->window->contmodempath);
		if (pathstr != NULL) {
			model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contactstreeview));
			if (model != NULL) {
				reflist = NULL;
				numcontacts = 0;
				/*Iterate through model and save references*/
				validcat = gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(model), &catiter, pathstr);
				while (validcat) {
					if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(model), &catiter)) {
						validcont = gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &contiter, &catiter);
						while (validcont) {
							gtk_tree_model_get(GTK_TREE_MODEL(model), &contiter, MMGUI_MAIN_CONTACTSLIST_TYPE, &contacttype, -1);
							/*Save references only on contacts stored on device*/
							if (contacttype == MMGUI_MAIN_CONTACT_MODEM) {
								refpath = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &contiter);
								if (refpath != NULL) {
									reference = gtk_tree_row_reference_new(GTK_TREE_MODEL(model), refpath);
									if (reference != NULL) {
										reflist = g_slist_prepend(reflist, reference);
										numcontacts++;
									}
								}
							}
							validcont = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &contiter);
						}
					}
					validcat = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &catiter);
				}
				
				/*Remove contacts if any found*/
				if (numcontacts > 0) {
					for (iterator = reflist;  iterator != NULL;  iterator = iterator->next) {
						refpath = gtk_tree_row_reference_get_path((GtkTreeRowReference *)iterator->data);
						if (refpath) {
							if (gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &contiter, refpath)) {
								gtk_tree_store_remove(GTK_TREE_STORE(model), &contiter);
							}
						}
					}
					/*Clear resources allocated for references list*/
					g_slist_foreach(reflist, (GFunc)gtk_tree_row_reference_free, NULL);
					g_slist_free(reflist);
				}
				
				/*Remove category caption*/
				if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(model), &catiter, pathstr)) {
					gtk_tree_store_remove(GTK_TREE_STORE(model), &catiter);
				}
			}
			g_free(pathstr);
		}
	}
	
	gtk_widget_set_sensitive(mmguiapp->window->removecontactbutton, FALSE);
	gtk_widget_set_sensitive(mmguiapp->window->smstocontactbutton, FALSE);
}
