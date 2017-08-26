/*
 *      ussd-page.c
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

#include "modem-settings.h"
#include "ussdlist.h"
#include "mmguicore.h"
#include "resources.h"

#include "ussd-page.h"
#include "main.h"

enum _mmgui_main_sms_completion {
	MMGUI_MAIN_USSD_COMPLETION_CAPTION = 0,
	MMGUI_MAIN_USSD_COMPLETION_NAME,
	MMGUI_MAIN_USSD_COMPLETION_COMMAND,
	MMGUI_MAIN_USSD_COMPLETION_COLUMNS
};

static void mmgui_main_ussd_list_read_callback(gchar *command, gchar *description, gboolean reencode, gpointer data);
static gboolean mmgui_main_ussd_list_add_command_to_xml_export_foreach(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static void mmgui_main_ussd_list_command_cell_edited_signal(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer data);
static void mmgui_main_ussd_list_description_cell_edited_signal(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer data);

/*USSD*/
void mmgui_main_ussd_command_add_button_clicked_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeViewColumn *column;
	GtkTreePath *path;
	gchar *pathstr;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->ussdedittreeview));
	
	if (model != NULL) {
		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_MAIN_USSDLIST_COMMAND, "*100#", MMGUI_MAIN_USSDLIST_DESCRIPTION, _("Sample command"), -1);
		
		pathstr = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(model), &iter);
		path = gtk_tree_path_new_from_string(pathstr);
		column = gtk_tree_view_get_column(GTK_TREE_VIEW(mmguiapp->window->ussdedittreeview), 0);
		
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(mmguiapp->window->ussdedittreeview), path, 0, TRUE, 0.5, 0.0);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(mmguiapp->window->ussdedittreeview), path, column, TRUE);
		
		gtk_tree_path_free(path);
		g_free(pathstr);
	}
}

void mmgui_main_ussd_command_remove_button_clicked_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->ussdedittreeview));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->ussdedittreeview));
	
	if ((model != NULL) && (selection != NULL)) {
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
		}
	}
}

void mmgui_main_ussd_menu_update_callback(gchar *command, gchar *description, gboolean reencode, gpointer data)
{
	mmgui_application_t mmguiapp;
	gchar *caption;
	GtkTreeIter iter;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	if ((command != NULL) && (description != NULL)) {
		/*Add USSD command to combo box*/
		if (mmguicore_ussd_validate_request(command) == MMGUI_USSD_VALIDATION_REQUEST) {
			caption = g_strdup_printf("%s - %s", command, description);
			gtk_list_store_append(GTK_LIST_STORE(mmguiapp->window->ussdcompletionmodel), &iter);
			gtk_list_store_set(GTK_LIST_STORE(mmguiapp->window->ussdcompletionmodel), &iter, MMGUI_MAIN_USSD_COMPLETION_CAPTION, caption, MMGUI_MAIN_USSD_COMPLETION_NAME, description, MMGUI_MAIN_USSD_COMPLETION_COMMAND, command, -1); 
			g_free(caption);
		}
	} else if (mmguiapp->core != NULL) {
		/*Set encoding flag for device*/
		if (reencode) {
			mmguicore_ussd_set_encoding(mmguiapp->core, MMGUI_USSD_ENCODING_UCS2);
		} else {
			mmguicore_ussd_set_encoding(mmguiapp->core, MMGUI_USSD_ENCODING_GSM7);
		}
	}
}

static void mmgui_main_ussd_list_read_callback(gchar *command, gchar *description, gboolean reencode, gpointer data)
{
	mmgui_application_data_t mmguiappdata;
	GtkTreeIter iter;
	
	mmguiappdata = (mmgui_application_data_t)data;
	
	if (mmguiappdata == NULL) return;
	if ((mmguiappdata->mmguiapp == NULL) || (mmguiappdata->data == NULL)) return;
	
	if ((command != NULL) && (description != NULL)) {
		if (mmguicore_ussd_validate_request(command) == MMGUI_USSD_VALIDATION_REQUEST) {
			gtk_list_store_append(GTK_LIST_STORE((GtkTreeModel *)(mmguiappdata->data)), &iter);
			gtk_list_store_set(GTK_LIST_STORE((GtkTreeModel *)(mmguiappdata->data)), &iter, MMGUI_MAIN_USSDLIST_COMMAND, command, MMGUI_MAIN_USSDLIST_DESCRIPTION, description, -1);
		}
	} /*else if (mmguicore_devices_get_current(mmguicore) != NULL) {
		if (reencode) {
			mmguicore_ussd_set_encoding(mmguicore, MMGUI_USSD_ENCODING_UCS2);
		} else {
			mmguicore_ussd_set_encoding(mmguicore, MMGUI_USSD_ENCODING_GSM7);
		}
	}*/
	else {
		if (reencode) {
			gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(mmguiappdata->mmguiapp->window->ussdencodingtoolbutton), TRUE);
		} else {
			gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(mmguiappdata->mmguiapp->window->ussdencodingtoolbutton), FALSE);
		}
	}
}

static gboolean mmgui_main_ussd_list_add_command_to_xml_export_foreach(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	gchar *command, *description;
	
	gtk_tree_model_get(model, iter, MMGUI_MAIN_USSDLIST_COMMAND, &command, MMGUI_MAIN_USSDLIST_DESCRIPTION, &description, -1);
	
	ussdlist_add_command_to_xml_export(command, description);
	
	return FALSE;
}

void mmgui_main_ussd_edit(mmgui_application_t mmguiapp)
{
	struct _mmgui_application_data mmguiappdata;
	GtkTreeModel *model;
	/*gint response;*/
	
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->ussdedittreeview));
	
	mmguiappdata.mmguiapp = mmguiapp;
	mmguiappdata.data = model;
	
	if (model != NULL) {
		//Detach model
		g_object_ref(model);
		gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->ussdedittreeview), NULL);
		gtk_list_store_clear(GTK_LIST_STORE(model));
		//Fill model
		if (!ussdlist_read_commands(mmgui_main_ussd_list_read_callback, mmguicore_devices_get_identifier(mmguiapp->core), mmguicore_devices_get_internal_identifier(mmguiapp->core), &mmguiappdata)) {
			/*Get current USSD encoding and set button state*/
			if (mmguiapp->core != NULL) {
				if (mmguicore_ussd_get_encoding(mmguiapp->core) == MMGUI_USSD_ENCODING_GSM7) {
					gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(mmguiapp->window->ussdencodingtoolbutton), FALSE);
				} else if (mmguicore_ussd_get_encoding(mmguiapp->core) == MMGUI_USSD_ENCODING_UCS2) {
					gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(mmguiapp->window->ussdencodingtoolbutton), TRUE);
				}
			}
		}
		//Attach model
		gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->ussdedittreeview), model);
		g_object_unref(model);
		
		/*response = */gtk_dialog_run(GTK_DIALOG(mmguiapp->window->ussdeditdialog));
		
		if (mmguicore_devices_get_current(mmguiapp->core) != NULL) {
			//Write commands to XML file
			ussdlist_start_xml_export(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(mmguiapp->window->ussdencodingtoolbutton)));
			gtk_tree_model_foreach(model, mmgui_main_ussd_list_add_command_to_xml_export_foreach, NULL);
			ussdlist_end_xml_export(mmguicore_devices_get_identifier(mmguiapp->core));
			//Update USSD menu
			gtk_list_store_clear(GTK_LIST_STORE(mmguiapp->window->ussdcompletionmodel));
			ussdlist_read_commands(mmgui_main_ussd_menu_update_callback, mmguicore_devices_get_identifier(mmguiapp->core), mmguicore_devices_get_internal_identifier(mmguiapp->core), mmguiapp);
			//Update USSD reencoding flag
			if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(mmguiapp->window->ussdencodingtoolbutton))) {
				mmguicore_ussd_set_encoding(mmguiapp->core, MMGUI_USSD_ENCODING_UCS2);
			} else {
				mmguicore_ussd_set_encoding(mmguiapp->core, MMGUI_USSD_ENCODING_GSM7);
			}
		}
		
		gtk_widget_hide(mmguiapp->window->ussdeditdialog);
	}
}

void mmgui_main_ussd_edit_button_clicked_signal(GtkEditable *editable, gpointer data)
{
	mmgui_application_t mmguiapp;
				
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_ussd_edit(mmguiapp);
}

void mmgui_main_ussd_command_combobox_changed_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
	GtkTreeIter iter;
	const gchar *command;
		
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(mmguiapp->window->ussdcombobox), &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(mmguiapp->window->ussdcompletionmodel), &iter, MMGUI_MAIN_USSD_COMPLETION_COMMAND, &command, -1);
		
		if (command != NULL) {
			gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->ussdentry), command);
			g_free((gchar *)command);
		}
	}
}

void mmgui_main_ussd_command_entry_changed_signal(GtkEditable *editable, gpointer data)
{
	const gchar *request;
	enum _mmgui_ussd_validation validationid;
	enum _mmgui_ussd_state sessionstate;
	mmgui_application_t mmguiapp;
		
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	//Validate request
	request = gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->ussdentry));
	validationid = mmguicore_ussd_validate_request((gchar *)request);
	
	if (validationid == MMGUI_USSD_VALIDATION_REQUEST) {
		//Simple request
		#if GTK_CHECK_VERSION(3,10,0)
			gtk_entry_set_icon_from_icon_name(GTK_ENTRY(mmguiapp->window->ussdentry), GTK_ENTRY_ICON_SECONDARY, NULL);
		#else
			gtk_entry_set_icon_from_stock(GTK_ENTRY(mmguiapp->window->ussdentry), GTK_ENTRY_ICON_SECONDARY, NULL);
		#endif
		gtk_entry_set_icon_tooltip_markup(GTK_ENTRY(mmguiapp->window->ussdentry), GTK_ENTRY_ICON_SECONDARY, NULL);
		gtk_widget_set_sensitive(mmguiapp->window->ussdsend, TRUE);
	} else if (validationid == MMGUI_USSD_VALIDATION_INVALID) {
		//Incorrect request
		#if GTK_CHECK_VERSION(3,10,0)
			gtk_entry_set_icon_from_icon_name(GTK_ENTRY(mmguiapp->window->ussdentry), GTK_ENTRY_ICON_SECONDARY, "dialog-warning");
		#else
			gtk_entry_set_icon_from_stock(GTK_ENTRY(mmguiapp->window->ussdentry), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CAPS_LOCK_WARNING);
		#endif
		gtk_entry_set_icon_tooltip_markup(GTK_ENTRY(mmguiapp->window->ussdentry), GTK_ENTRY_ICON_SECONDARY, _("<b>USSD request is not valid</b>\n<small>Request must be 160 symbols long\nstarted with '*' and ended with '#'</small>"));
		gtk_widget_set_sensitive(mmguiapp->window->ussdsend, FALSE);
	} else if (validationid == MMGUI_USSD_VALIDATION_RESPONSE) {
		//Response
		sessionstate = mmguicore_ussd_get_state(mmguiapp->core);
		if (sessionstate == MMGUI_USSD_STATE_USER_RESPONSE) {
			//Response expected
			#if GTK_CHECK_VERSION(3,10,0)
				gtk_entry_set_icon_from_icon_name(GTK_ENTRY(mmguiapp->window->ussdentry), GTK_ENTRY_ICON_SECONDARY, NULL);
			#else
				gtk_entry_set_icon_from_stock(GTK_ENTRY(mmguiapp->window->ussdentry), GTK_ENTRY_ICON_SECONDARY, NULL);
			#endif
			gtk_entry_set_icon_tooltip_markup(GTK_ENTRY(mmguiapp->window->ussdentry), GTK_ENTRY_ICON_SECONDARY, NULL);
			gtk_widget_set_sensitive(mmguiapp->window->ussdsend, TRUE);
		} else {
			//Response not expected
			#if GTK_CHECK_VERSION(3,10,0)
				gtk_entry_set_icon_from_icon_name(GTK_ENTRY(mmguiapp->window->ussdentry), GTK_ENTRY_ICON_SECONDARY, "dialog-warning");
			#else
				gtk_entry_set_icon_from_stock(GTK_ENTRY(mmguiapp->window->ussdentry), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CAPS_LOCK_WARNING);
			#endif
			gtk_entry_set_icon_tooltip_markup(GTK_ENTRY(mmguiapp->window->ussdentry), GTK_ENTRY_ICON_SECONDARY, _("<b>USSD request is not valid</b>\n<small>Request must be 160 symbols long\nstarted with '*' and ended with '#'</small>"));
			gtk_widget_set_sensitive(mmguiapp->window->ussdsend, FALSE);
		}
	}
}

void mmgui_main_ussd_command_entry_activated_signal(GtkEntry *entry, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_ussd_request_send(mmguiapp);
}

void mmgui_main_ussd_send_button_clicked_signal(GtkButton *button, gpointer data)
{
	mmgui_application_t mmguiapp;
		
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_ussd_request_send(mmguiapp);
}

static gboolean mmgui_main_ussd_entry_match_selected_signal(GtkEntryCompletion *widget, GtkTreeModel *model, GtkTreeIter *iter, gpointer userdata)
{
	mmgui_application_t mmguiapp;
	const gchar *command;
	
	mmguiapp = (mmgui_application_t)userdata;
	
	if (mmguiapp == NULL) return FALSE;
	
	gtk_tree_model_get(GTK_TREE_MODEL(mmguiapp->window->ussdcompletionmodel), iter, MMGUI_MAIN_USSD_COMPLETION_COMMAND, &command, -1);
	
	if (command != NULL) {
		gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->ussdentry), command);
		g_free((gchar *)command);
		return TRUE;
	}
	
	return FALSE;
}

void mmgui_main_ussd_request_send(mmgui_application_t mmguiapp)
{
	gchar *request;
	enum _mmgui_ussd_validation validationid;
	enum _mmgui_ussd_state sessionstate;
	GtkTextBuffer *buffer;
	GtkTextIter startiter, enditer;
	GtkTextMark *position;
	gchar *statusmsg;
	
	if (mmguiapp == NULL) return;
	
	if (gtk_entry_get_text_length(GTK_ENTRY(mmguiapp->window->ussdentry)) == 0) return;
	
	request = (gchar *)gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->ussdentry));
	
	//Session state and request validation
	validationid = mmguicore_ussd_validate_request(request);
	sessionstate = mmguicore_ussd_get_state(mmguiapp->core);
	//Text view buffer
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mmguiapp->window->ussdtext));
	
	if (validationid != MMGUI_USSD_VALIDATION_INVALID) {
		if (!((validationid == MMGUI_USSD_VALIDATION_RESPONSE) && (sessionstate != MMGUI_USSD_STATE_USER_RESPONSE))) {
			if (mmguicore_ussd_send(mmguiapp->core, request)) {
				if (validationid == MMGUI_USSD_VALIDATION_REQUEST) {
					/*save last request*/
					mmgui_modem_settings_set_string(mmguiapp->modemsettings, "ussd_sent_request", request);
					/*clear text view*/
					gtk_text_buffer_get_bounds(buffer, &startiter, &enditer);
					gtk_text_buffer_delete(buffer, &startiter, &enditer);
					/*add request text*/
					gtk_text_buffer_get_end_iter(buffer, &enditer);
					gtk_text_buffer_insert_with_tags(buffer, &enditer, request, -1, mmguiapp->window->ussdrequesttag, NULL);
					gtk_text_buffer_get_end_iter(buffer, &enditer);
					gtk_text_buffer_insert_with_tags(buffer, &enditer, "\n", -1, mmguiapp->window->ussdrequesttag, NULL);
					statusmsg = g_strdup_printf(_("Sending USSD request %s..."), request);
				} else {
					/*add response text*/
					gtk_text_buffer_get_end_iter(buffer, &enditer);
					gtk_text_buffer_insert_with_tags(buffer, &enditer, request, -1, mmguiapp->window->ussdrequesttag, NULL);
					gtk_text_buffer_get_end_iter(buffer, &enditer);
					gtk_text_buffer_insert_with_tags(buffer, &enditer, "\n", -1, mmguiapp->window->ussdrequesttag, NULL);
					statusmsg = g_strdup_printf(_("Sending USSD response %s..."), request);
				}
				/*scroll to the end of buffer*/
				position = gtk_text_buffer_create_mark(buffer, "position", &enditer, FALSE);
				gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(mmguiapp->window->ussdtext), position);
				gtk_text_buffer_delete_mark(buffer, position);
				//show progress dialog
				//mmgui_main_ui_progress_dialog_open(mmguiapp);
				mmgui_ui_infobar_show(mmguiapp, statusmsg, MMGUI_MAIN_INFOBAR_TYPE_PROGRESS);
				g_free(statusmsg);
			} else {
				mmgui_main_ui_error_dialog_open(mmguiapp, _("<b>Error sending USSD</b>"), _("Wrong USSD request or device not ready"));
			}
		} else {
			mmgui_main_ui_error_dialog_open(mmguiapp, _("<b>Error sending USSD</b>"), _("USSD session terminated. You can send new request"));
		}
	} else {
		mmgui_main_ui_error_dialog_open(mmguiapp, _("<b>Error sending USSD</b>"), _("Wrong USSD request"));
	}
}

void mmgui_main_ussd_request_send_end(mmgui_application_t mmguiapp, mmguicore_t mmguicore, const gchar *answer)
{
	enum _mmgui_ussd_state sessionstate;
	GtkTextBuffer *buffer;
	GtkTextIter enditer;
	GtkTextMark *position;
	
	if ((mmguiapp == NULL) || (mmguicore == NULL)) return;
	
	sessionstate = mmguicore_ussd_get_state(mmguicore);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mmguiapp->window->ussdtext));
	
	if (answer != NULL) {
		/*Add answer text*/
		gtk_text_buffer_get_end_iter(buffer, &enditer);
		gtk_text_buffer_insert_with_tags(buffer, &enditer, answer, -1, mmguiapp->window->ussdanswertag, NULL);
		if (sessionstate == MMGUI_USSD_STATE_USER_RESPONSE) {
			/*Add session hint*/
			gtk_text_buffer_get_end_iter(buffer, &enditer);
			gtk_text_buffer_insert_with_tags(buffer, &enditer, _("\nUSSD session is active. Waiting for your input...\n"), -1, mmguiapp->window->ussdhinttag, NULL);
		} else {
			/*Add new line symbol*/
			gtk_text_buffer_get_end_iter(buffer, &enditer);
			gtk_text_buffer_insert_with_tags(buffer, &enditer, "\n", -1, mmguiapp->window->ussdanswertag, NULL);
		}
		/*Scroll to the end of buffer*/
		position = gtk_text_buffer_create_mark(buffer, "position", &enditer, FALSE);
		gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(mmguiapp->window->ussdtext), position);
		gtk_text_buffer_delete_mark(buffer, position);
	} else {
		/*Add error hint*/
		gtk_text_buffer_get_end_iter(buffer, &enditer);
		gtk_text_buffer_insert_with_tags(buffer, &enditer, _("\nNo answer received..."), -1, mmguiapp->window->ussdhinttag, NULL);
		/*Scroll to the end of buffer*/
		position = gtk_text_buffer_create_mark(buffer, "position", &enditer, FALSE);
		gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(mmguiapp->window->ussdtext), position);
		gtk_text_buffer_delete_mark(buffer, position);
		/*Show menubar with error message*/
		mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_FAIL, mmguicore_get_last_error(mmguiapp->core));
	}
}

static void mmgui_main_ussd_list_command_cell_edited_signal(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	if (mmguicore_ussd_validate_request(new_text) == MMGUI_USSD_VALIDATION_REQUEST) {
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->ussdedittreeview));
		if (gtk_tree_model_get_iter_from_string(model, &iter, path)) {
			gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_MAIN_USSDLIST_COMMAND, new_text, -1);
		}
	}
}

static void mmgui_main_ussd_list_description_cell_edited_signal(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	if (g_ascii_strcasecmp(new_text, "") != 0) {
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->ussdedittreeview));
		if (gtk_tree_model_get_iter_from_string(model, &iter, path)) {
			gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_MAIN_USSDLIST_DESCRIPTION, new_text, -1);
		}
	}
}

void mmgui_main_ussd_restore_settings_for_modem(mmgui_application_t mmguiapp)
{
	gchar *request;
		
	if (mmguiapp == NULL) return;
	
	/*Saved USSD commands*/
	ussdlist_read_commands(mmgui_main_ussd_menu_update_callback, mmguicore_devices_get_identifier(mmguiapp->core), mmguicore_devices_get_internal_identifier(mmguiapp->core), mmguiapp);
	
	/*Last USSD request*/
	request = mmgui_modem_settings_get_string(mmguiapp->modemsettings, "ussd_sent_request", "*100#");
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->ussdentry), request);
	g_free(request);
}

void mmgui_main_ussd_accelerators_init(mmgui_application_t mmguiapp)
{
	/*Accelerators*/
	mmguiapp->window->ussdaccelgroup = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(mmguiapp->window->ussdeditdialog), mmguiapp->window->ussdaccelgroup);
	gtk_widget_add_accelerator(mmguiapp->window->newussdtoolbutton, "clicked", mmguiapp->window->ussdaccelgroup, GDK_KEY_n, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
	gtk_widget_add_accelerator(mmguiapp->window->removeussdtoolbutton, "clicked", mmguiapp->window->ussdaccelgroup, GDK_KEY_d, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
	gtk_widget_add_accelerator(mmguiapp->window->ussdencodingtoolbutton, "clicked", mmguiapp->window->ussdaccelgroup, GDK_KEY_e, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
}

void mmgui_main_ussd_list_init(mmgui_application_t mmguiapp)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *store;
	GtkTextBuffer *buffer;
	
	if (mmguiapp == NULL) return;
	
	/*List*/
	renderer = gtk_cell_renderer_text_new();
	g_object_set (renderer, "editable", TRUE, "editable-set", TRUE, NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Command"), renderer, "text", MMGUI_MAIN_USSDLIST_COMMAND, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->ussdedittreeview), column);
	g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(mmgui_main_ussd_list_command_cell_edited_signal), mmguiapp);
	
	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "editable", TRUE, "editable-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, "ellipsize-set", TRUE, NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Description"), renderer, "text", MMGUI_MAIN_USSDLIST_DESCRIPTION, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->ussdedittreeview), column);
	g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(mmgui_main_ussd_list_description_cell_edited_signal), mmguiapp);
	
	store = gtk_list_store_new(MMGUI_MAIN_USSDLIST_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
	
	gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->ussdedittreeview), GTK_TREE_MODEL(store));
	g_object_unref(store);
	
	/*Initialize textview tags*/
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mmguiapp->window->ussdtext));
	mmguiapp->window->ussdrequesttag = gtk_text_buffer_create_tag(buffer, "request", "weight", PANGO_WEIGHT_BOLD, "indent", 5, "left_margin", 5, "right_margin", 5, NULL);
	mmguiapp->window->ussdhinttag = gtk_text_buffer_create_tag(buffer, "hint", "weight", PANGO_WEIGHT_NORMAL, "style", PANGO_STYLE_ITALIC, "scale", PANGO_SCALE_SMALL, "indent", 5, "left_margin", 5, "right_margin", 5, NULL);
	mmguiapp->window->ussdanswertag = gtk_text_buffer_create_tag(buffer, "answer", "weight", PANGO_WEIGHT_NORMAL, "indent", 5, "left_margin", 5, "right_margin", 5, NULL);
	
	/*Initialize combobox model*/
	mmguiapp->window->ussdcompletionmodel = gtk_list_store_new(MMGUI_MAIN_USSD_COMPLETION_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	gtk_combo_box_set_model(GTK_COMBO_BOX(mmguiapp->window->ussdcombobox), GTK_TREE_MODEL(mmguiapp->window->ussdcompletionmodel));
	gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(mmguiapp->window->ussdcombobox), MMGUI_MAIN_USSD_COMPLETION_CAPTION);
	/*Entry autocompletion*/
	mmguiapp->window->ussdcompletion = gtk_entry_completion_new();
    gtk_entry_completion_set_text_column(mmguiapp->window->ussdcompletion, MMGUI_MAIN_USSD_COMPLETION_NAME);
    gtk_entry_set_completion(GTK_ENTRY(mmguiapp->window->ussdentry), mmguiapp->window->ussdcompletion);
	gtk_entry_completion_set_model(mmguiapp->window->ussdcompletion, GTK_TREE_MODEL(mmguiapp->window->ussdcompletionmodel));
	g_signal_connect(G_OBJECT(mmguiapp->window->ussdcompletion), "match-selected", G_CALLBACK(mmgui_main_ussd_entry_match_selected_signal), mmguiapp); 
}

void mmgui_main_ussd_state_clear(mmgui_application_t mmguiapp)
{
	GtkTextBuffer *buffer;
	GtkTextIter siter, eiter;
	if (mmguiapp == NULL) return;
	
	/*Clear USSD menu*/
	gtk_list_store_clear(GTK_LIST_STORE(mmguiapp->window->ussdcompletionmodel));
		
	/*Clear USSD text field*/
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mmguiapp->window->ussdtext));
	if (buffer != NULL) {
		gtk_text_buffer_get_bounds(buffer, &siter, &eiter);
		gtk_text_buffer_delete(buffer, &siter, &eiter);
	}
}
