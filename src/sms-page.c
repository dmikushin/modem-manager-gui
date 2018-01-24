/*
 *      sms-page.c
 *      
 *      Copyright 2012-2017 Alex <alex@linuxonly.ru>
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
#include "notifications.h"
#include "ayatana.h"
#include "strformat.h"
#include "mmguicore.h"
#include "smsdb.h"
#include "contacts-page.h"
#include "../resources.h"
#include "settings.h"
#include "sms-page.h"
#include "main.h"

#if RESOURCE_SPELLCHECKER_ENABLED
	#include <gtkspell/gtkspell.h>
#endif


enum _mmgui_main_smslist_columns {
	MMGUI_MAIN_SMSLIST_ICON = 0,
	MMGUI_MAIN_SMSLIST_SMS,
	MMGUI_MAIN_SMSLIST_ID,
	MMGUI_MAIN_SMSLIST_FOLDER,
	MMGUI_MAIN_SMSLIST_ISFOLDER,
	MMGUI_MAIN_SMSLIST_COLUMNS
};

enum _mmgui_main_new_sms_validation {
	MMGUI_MAIN_NEW_SMS_VALIDATION_VALID = 0x00,
	MMGUI_MAIN_NEW_SMS_VALIDATION_WRONG_NUMBER = 0x01,
	MMGUI_MAIN_NEW_SMS_VALIDATION_WRONG_TEXT = 0x02
};

enum _mmgui_main_new_sms_dialog_result {
	MMGUI_MAIN_NEW_SMS_DIALOG_CLOSE = 0,
	MMGUI_MAIN_NEW_SMS_DIALOG_SEND,
	MMGUI_MAIN_NEW_SMS_DIALOG_SAVE
};

enum _mmgui_main_sms_source {
	MMGUI_MAIN_SMS_SOURCE_MODEM = 0,
	MMGUI_MAIN_SMS_SOURCE_GNOME,
	MMGUI_MAIN_SMS_SOURCE_KDE,
	MMGUI_MAIN_SMS_SOURCE_HISTORY,
	MMGUI_MAIN_SMS_SOURCE_CAPTION,
	MMGUI_MAIN_SMS_SOURCE_SEPARATOR
};

enum _mmgui_main_sms_completion {
	MMGUI_MAIN_SMS_COMPLETION_NAME = 0,
	MMGUI_MAIN_SMS_COMPLETION_NUMBER,
	MMGUI_MAIN_SMS_COMPLETION_SOURCE,
	MMGUI_MAIN_SMS_COMPLETION_COLUMNS
};

enum _mmgui_main_sms_list {
	MMGUI_MAIN_SMS_LIST_NAME = 0,
	MMGUI_MAIN_SMS_LIST_NUMBER,
	MMGUI_MAIN_SMS_LIST_SOURCE,
	MMGUI_MAIN_SMS_LIST_COLUMNS
};

struct _sms_selection_data {
	mmgui_application_t mmguiapp;
	gulong messageid;
};

typedef struct _sms_selection_data *sms_selection_data_t;

static void mmgui_main_sms_notification_show_window_callback(gpointer notification, gchar *action, gpointer userdata);
static void mmgui_main_sms_select_entry_from_list(mmgui_application_t mmguiapp, gulong entryid, gboolean isfolder);
static void mmgui_main_sms_get_message_list_hash_destroy_notify(gpointer data);
static void mmgui_main_sms_new_dialog_number_changed_signal(GtkEditable *editable, gpointer data);
static enum _mmgui_main_new_sms_dialog_result mmgui_main_sms_new_dialog(mmgui_application_t mmguiapp, const gchar *number, const gchar *text);
static void mmgui_main_sms_list_selection_changed_signal(GtkTreeSelection *selection, gpointer data);
static void mmgui_main_sms_list_row_activated_signal(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data);
static void mmgui_main_sms_add_to_list(mmgui_application_t mmguiapp, mmgui_sms_message_t sms, GtkTreeModel *model);
static gboolean mmgui_main_sms_autocompletion_select_entry_signal(GtkEntryCompletion *widget, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data);
static void mmgui_main_sms_autocompletion_model_fill(mmgui_application_t mmguiapp, guint source);
static void mmgui_main_sms_menu_model_fill(mmgui_application_t mmguiapp, guint source);
static void mmgui_main_sms_menu_data_func(GtkCellLayout *cell_layout, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data);
static gboolean mmgui_main_sms_menu_separator_func(GtkTreeModel *model, GtkTreeIter  *iter, gpointer data);
static gchar *mmgui_main_sms_list_select_entry_signal(GtkComboBox *combo, const gchar *path, gpointer user_data);
static void mmgui_main_sms_load_numbers_history(mmgui_application_t mmguiapp);
static void mmgui_main_sms_add_number_to_history(mmgui_application_t mmguiapp, gchar *number);
static gchar *mmgui_main_sms_get_first_number_from_history(mmgui_application_t mmguiapp, gchar *defnumber);
static gchar *mmgui_main_sms_get_name_for_number(mmgui_application_t mmguiapp, gchar *number);

/*SMS*/
static void mmgui_main_sms_notification_show_window_callback(gpointer notification, gchar *action, gpointer userdata)
{
	sms_selection_data_t seldata;
			
	seldata = (sms_selection_data_t)userdata;
	
	if (seldata == NULL) return;
	
	if (gtk_widget_get_sensitive(seldata->mmguiapp->window->smsbutton)) {
		/*Select received message*/
		mmgui_main_sms_select_entry_from_list(seldata->mmguiapp, seldata->messageid, FALSE);
		/*Open SMS page*/
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(seldata->mmguiapp->window->smsbutton), TRUE);
		/*Show window*/
		gtk_window_present(GTK_WINDOW(seldata->mmguiapp->window->window));
	}
	
	g_free(seldata);
}

static void mmgui_main_sms_select_entry_from_list(mmgui_application_t mmguiapp, gulong entryid, gboolean isfolder)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter folderiter, msgiter;
	GtkTreeIter *firstfolderiter, *entryiter;
	GtkTreePath *path;
    gboolean foldervalid, msgvalid;
    gulong curid;
    guint curfolder;
    gboolean curisfolder;
    
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->smslist));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->smslist));
	
	if ((model == NULL) || (selection == NULL)) return;
	
	firstfolderiter = NULL;
	entryiter = NULL;
	
	foldervalid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &folderiter);
	while ((foldervalid) && (entryiter == NULL)) {
		/*Folders*/
		gtk_tree_model_get(model, &folderiter, MMGUI_MAIN_SMSLIST_ID, &curid, MMGUI_MAIN_SMSLIST_FOLDER, &curfolder, MMGUI_MAIN_SMSLIST_ISFOLDER, &curisfolder, -1);
		if (curisfolder) {
			/*First folder*/
			if (firstfolderiter == NULL) {
				firstfolderiter = gtk_tree_iter_copy(&folderiter);
			}
			if (isfolder) {
				/*Save folder iterator*/
				if (curfolder == entryid) {
					entryiter = gtk_tree_iter_copy(&folderiter);
				}
			} else if (gtk_tree_model_iter_has_child(model, &folderiter)) {
				/*Messages*/
				if (gtk_tree_model_iter_children(model, &msgiter, &folderiter)) {
					do {
						gtk_tree_model_get(model, &msgiter, MMGUI_MAIN_SMSLIST_ID, &curid, MMGUI_MAIN_SMSLIST_FOLDER, &curfolder, MMGUI_MAIN_SMSLIST_ISFOLDER, &curisfolder, -1);
						/*Save message iterator*/
						if ((!curisfolder) && (curid == entryid)) {
							entryiter = gtk_tree_iter_copy(&msgiter);
						}
						msgvalid = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &msgiter);
					} while ((msgvalid) && (entryiter == NULL));
				}
			}
		}
		foldervalid = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &folderiter);
	}
	
	/*Unselect all rows as multiple selection mode enabled*/
	gtk_tree_selection_unselect_all(selection);
	
	if (entryiter != NULL) {
		/*Message found - select it*/
		gtk_tree_selection_select_iter(selection, entryiter);
		path = gtk_tree_model_get_path(model, entryiter);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(mmguiapp->window->smslist), path, NULL, TRUE, 0.5, 0.0);
		gtk_tree_path_free (path);
		g_signal_emit_by_name(G_OBJECT(mmguiapp->window->smslist), "cursor-changed", NULL);
		gtk_tree_iter_free(entryiter);
		if (firstfolderiter != NULL) {
			/*'Incoming' folder placement - forget it*/
			gtk_tree_iter_free(firstfolderiter);
		}
	} else if (firstfolderiter != NULL) {
		/*Select 'Incoming' folder if message isn't found*/
		gtk_tree_selection_select_iter(selection, firstfolderiter);
		path = gtk_tree_model_get_path(model, firstfolderiter);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(mmguiapp->window->smslist), path, NULL, TRUE, 0.5, 0.0);
		gtk_tree_path_free (path);
		g_signal_emit_by_name(G_OBJECT(mmguiapp->window->smslist), "cursor-changed", NULL);
		gtk_tree_iter_free(firstfolderiter);
	}
}

static void mmgui_main_sms_get_message_list_hash_destroy_notify(gpointer data)
{
	//Free unique sender name hash table entries
	if (data != NULL) g_free(data);
}

static void mmgui_main_sms_execute_custom_command(mmgui_application_t mmguiapp, mmgui_sms_message_t message)
{
	gint argcp, i, p;
	gchar **argvp;
	gboolean tplfound;
	GString *arg;
	GError *error;
	
	if ((mmguiapp == NULL) || (message == NULL)) return;
	
	/*Custom command*/
	if (g_strcmp0(mmguiapp->options->smscustomcommand, "") != 0) {
		if (g_shell_parse_argv(mmguiapp->options->smscustomcommand, &argcp, &argvp, NULL)) {
			for (i = 0; i < argcp; i++) {
				/*First search for known templates*/
				tplfound = FALSE;
				for (p = 0; argvp[i][p] != '\0'; p++) {
					if (argvp[i][p] == '%') {
						if ((argvp[i][p + 1] == 'n') || (argvp[i][p + 1] == 't')) {
							tplfound = TRUE;
							break;
						}
					}
				}
				/*Then replace templates if found*/
				if (tplfound) {
					arg = g_string_new(NULL);
					for (p = 0; argvp[i][p] != '\0'; p++) {
						if (argvp[i][p] == '%') {
							if (argvp[i][p + 1] == 'n') {
								g_string_append(arg, mmgui_smsdb_message_get_number(message));
							} else if (argvp[i][p + 1] == 't') {
								g_string_append(arg, mmgui_smsdb_message_get_text(message));
							}
							p++;
						} else {
							g_string_append_c(arg, argvp[i][p]);
						}
					}
					g_free(argvp[i]);
					argvp[i] = g_string_free(arg, FALSE);
				}
			}
			/*Finally exucute custom command*/
			error = NULL;
			#if GLIB_CHECK_VERSION(2,34,0)
			if (!g_spawn_async(NULL, argvp, NULL, G_SPAWN_SEARCH_PATH_FROM_ENVP, NULL, NULL, NULL, &error)) {
			#else
			if (!g_spawn_async(NULL, argvp, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
			#endif
				g_debug("Error spawning external command: %s\n", error->message);
				g_error_free(error);
			}
			g_strfreev(argvp);
		}
	}
}

gboolean mmgui_main_sms_get_message_list_from_thread(gpointer data)
{
	mmgui_application_data_t mmguiappdata;
	GSList *messages, *iterator;
	mmgui_sms_message_t message;
	sms_selection_data_t seldata;
	guint nummessages, addedsender;
	gchar *notifycaption, *currentsender;
	GHashTable *sendernames;
	GHashTableIter sendernamesiter;
	gpointer sendernameskey, sendernamesvalue;
	GString *senderunames;
	enum _mmgui_notifications_sound soundmode;
			
	mmguiappdata = (mmgui_application_data_t)data;
	
	if (mmguiappdata == NULL) return FALSE;
		
	messages = mmguicore_sms_enum(mmguiappdata->mmguiapp->core, (gboolean)GPOINTER_TO_UINT(mmguiappdata->data));
	
	if (messages == NULL) return FALSE;
	
	/*No new messages available yet*/
	nummessages = 0;
	/*Hash table for unique sender names*/
	sendernames = g_hash_table_new_full(g_str_hash, g_str_equal, mmgui_main_sms_get_message_list_hash_destroy_notify, NULL);
	
	seldata = NULL;
	
	for (iterator=messages; iterator; iterator=iterator->next) {
		message = iterator->data;
		if (!mmgui_smsdb_message_get_read(message)) {
			/*Add message to database*/
			mmgui_smsdb_add_sms(mmguicore_devices_get_sms_db(mmguiappdata->mmguiapp->core), message);
			/*Add message to list*/
			mmgui_main_sms_add_to_list(mmguiappdata->mmguiapp, message, NULL);
			/*Add unique sender name into hash table*/
			if (g_hash_table_lookup(sendernames, message->number) == NULL) {
				currentsender = g_strdup(mmgui_smsdb_message_get_number(message));
				g_hash_table_insert(sendernames, currentsender, currentsender);
			}
			/*Message selection structure*/
			if (seldata == NULL) {
				seldata = g_new0(struct _sms_selection_data, 1);
				seldata->mmguiapp = mmguiappdata->mmguiapp;
				seldata->messageid = mmgui_smsdb_message_get_db_identifier(message);
			}
			/*New message received*/
			nummessages++;
		}
		/*Execute custom command*/
		mmgui_main_sms_execute_custom_command(mmguiappdata->mmguiapp, message);
		/*Delete message*/
		mmguicore_sms_delete(mmguiappdata->mmguiapp->core, mmgui_smsdb_message_get_identifier(message));
		/*Free message*/
		mmgui_smsdb_message_free(message);
	}
	/*Free list*/
	g_slist_free(messages);
	
	/*Form notification caption based on messages count*/
	if (nummessages > 1) {
		notifycaption = g_strdup_printf(_("Received %u new SMS messages"), nummessages);
	} else if (nummessages == 1) {
		notifycaption = g_strdup(_("Received new SMS message"));
	} else {
		g_hash_table_destroy(sendernames);
		if (seldata != NULL) {
			g_free(seldata);
		}
		g_free(mmguiappdata);
		g_debug("Failed to add messages to database\n");
		return FALSE;
	}
	
	/*Form list of unique senders for message text*/
	senderunames = g_string_new(_("Message senders: "));
	/*Number of unique messages*/
	addedsender = 0;
	
	/*Iterate through hash table*/
	g_hash_table_iter_init(&sendernamesiter, sendernames);
	while (g_hash_table_iter_next(&sendernamesiter, &sendernameskey, &sendernamesvalue)) {
		if (addedsender == 0) {
			g_string_append_printf(senderunames, " %s", (gchar *)sendernameskey);
		} else {
			g_string_append_printf(senderunames, ", %s", (gchar *)sendernameskey);
		}
		addedsender++;
	}
	senderunames = g_string_append_c(senderunames, '.');
	
	/*Show notification/play sound*/
	if (mmguiappdata->mmguiapp->options->usesounds) {
		soundmode = MMGUI_NOTIFICATIONS_SOUND_MESSAGE;
	} else {
		soundmode = MMGUI_NOTIFICATIONS_SOUND_NONE;
	}
	
	/*Ayatana menu*/
	mmgui_ayatana_set_unread_messages_number(mmguiappdata->mmguiapp->ayatana, mmgui_smsdb_get_unread_messages(mmguicore_devices_get_sms_db(mmguiappdata->mmguiapp->core)));
	
	/*Notification*/
	mmgui_notifications_show(mmguiappdata->mmguiapp->notifications, notifycaption, senderunames->str, soundmode, mmgui_main_sms_notification_show_window_callback, seldata);
	
	/*Free resources*/
	g_free(notifycaption);
	g_hash_table_destroy(sendernames);
	g_string_free(senderunames, TRUE);
	g_free(mmguiappdata);
	
	return FALSE;
}

gboolean mmgui_main_sms_get_message_from_thread(gpointer data)
{
	mmgui_application_data_t mmguiappdata;
	guint messageseq;
	mmgui_sms_message_t message;
	gchar *notifycaption, *notifytext;
	enum _mmgui_notifications_sound soundmode;
	sms_selection_data_t seldata;
			
	mmguiappdata = (mmgui_application_data_t)data;
	
	if (mmguiappdata == NULL) return FALSE;
	
	messageseq = GPOINTER_TO_UINT(mmguiappdata->data);
	message = mmguicore_sms_get(mmguiappdata->mmguiapp->core, messageseq);
	
	if (message == NULL) return FALSE;
	
	/*Add message to database*/
	mmgui_smsdb_add_sms(mmguicore_devices_get_sms_db(mmguiappdata->mmguiapp->core), message);
	
	/*Add message to list*/
	mmgui_main_sms_add_to_list(mmguiappdata->mmguiapp, message, NULL);
	
	/*Remove message from device*/
	mmguicore_sms_delete(mmguiappdata->mmguiapp->core, mmgui_smsdb_message_get_identifier(message));
	
	/*Form notification*/
	notifycaption = g_strdup(_("Received new SMS message"));
	notifytext = g_strdup_printf("%s: %s", mmgui_smsdb_message_get_number(message), mmgui_smsdb_message_get_text(message));
		
	/*Show notification/play sound*/
	if (mmguiappdata->mmguiapp->options->usesounds) {
		soundmode = MMGUI_NOTIFICATIONS_SOUND_MESSAGE;
	} else {
		soundmode = MMGUI_NOTIFICATIONS_SOUND_NONE;
	}
	
	/*Message selection structure*/
	seldata = g_new0(struct _sms_selection_data, 1);
	seldata->mmguiapp = mmguiappdata->mmguiapp;
	seldata->messageid = mmgui_smsdb_message_get_db_identifier(message);
	
	/*Ayatana menu*/
	mmgui_ayatana_set_unread_messages_number(mmguiappdata->mmguiapp->ayatana, mmgui_smsdb_get_unread_messages(mmguicore_devices_get_sms_db(mmguiappdata->mmguiapp->core)));
	
	/*Execute custom command*/
	mmgui_main_sms_execute_custom_command(mmguiappdata->mmguiapp, message);
	
	/*Notification*/
	mmgui_notifications_show(mmguiappdata->mmguiapp->notifications, notifycaption, notifytext, soundmode, mmgui_main_sms_notification_show_window_callback, seldata);
	
	/*Free resources*/
	g_free(notifycaption);
	g_free(notifytext);
	g_free(mmguiappdata);
	mmgui_smsdb_message_free(message);
	
	return FALSE;
}

gboolean mmgui_main_sms_handle_new_day_from_thread(gpointer data)
{
	mmgui_application_t mmguiapp;
	GtkTreeModel *model;
	GtkTreeIter folderiter, msgiter;
	gboolean foldervalid, msgvalid;
	gulong curid;
	guint curfolder;
	gboolean curisfolder;
	mmgui_sms_message_t message;
	smsdb_t smsdb;
	time_t timestamp;
	gchar *markup;
	gchar timestr[200];
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return FALSE;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->smslist));
	smsdb = mmguicore_devices_get_sms_db(mmguiapp->core);
	
	if ((model == NULL) || (smsdb == NULL)) return FALSE;
	
	foldervalid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &folderiter);
	while (foldervalid) {
		/*Folders*/
		gtk_tree_model_get(model, &folderiter, MMGUI_MAIN_SMSLIST_ID, &curid, MMGUI_MAIN_SMSLIST_FOLDER, &curfolder, MMGUI_MAIN_SMSLIST_ISFOLDER, &curisfolder, -1);
		if (gtk_tree_model_iter_has_child(model, &folderiter)) {
			/*Messages*/
			if (gtk_tree_model_iter_children(model, &msgiter, &folderiter)) {
				do {
					gtk_tree_model_get(model, &msgiter, MMGUI_MAIN_SMSLIST_ID, &curid, MMGUI_MAIN_SMSLIST_FOLDER, &curfolder, MMGUI_MAIN_SMSLIST_ISFOLDER, &curisfolder, -1);
					message = mmgui_smsdb_read_sms_message(smsdb, curid);
					if (message != NULL) {
						/*Current time*/
						timestamp = mmgui_smsdb_message_get_timestamp(message);
						/*New markup string*/
						markup = g_strdup_printf(_("<b>%s</b>\n<small>%s</small>"), mmgui_smsdb_message_get_number(message), mmgui_str_format_sms_time(timestamp, timestr, sizeof(timestr)));
						/*Update markup string*/
						gtk_tree_store_set(GTK_TREE_STORE(model), &msgiter, MMGUI_MAIN_SMSLIST_SMS, markup,	-1);
						/*Free resources*/
						g_free(markup);
						mmgui_smsdb_message_free(message);
					}
					msgvalid = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &msgiter);
				} while (msgvalid);
			}
		}
		foldervalid = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &folderiter);
	}
	
	return FALSE;
}

static void mmgui_main_sms_new_dialog_number_changed_signal(GtkEditable *editable, gpointer data)
{
	mmgui_application_data_t appdata;
	const gchar *number;
	GtkTextBuffer *buffer;
	gboolean newnumvalid;
	gint bufferchars;
	gint *smsvalidflags;
	gint newsmsvalidflags;
	
	appdata = (mmgui_application_data_t)data;
	
	if (appdata == NULL) return;
	
	number = gtk_entry_get_text(GTK_ENTRY(appdata->mmguiapp->window->smsnumberentry));
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(appdata->mmguiapp->window->smstextview));
	smsvalidflags = (gint *)appdata->data;
	
	//Validate SMS number
	newnumvalid = mmguicore_sms_validate_number(number);
	if (buffer != NULL) {
		bufferchars = gtk_text_buffer_get_char_count(buffer);
	} else {
		bufferchars = 0;
	}
	
	newsmsvalidflags = MMGUI_MAIN_NEW_SMS_VALIDATION_VALID;
	if (!newnumvalid) newsmsvalidflags &= MMGUI_MAIN_NEW_SMS_VALIDATION_WRONG_NUMBER;
	if (bufferchars == 0) newsmsvalidflags &= MMGUI_MAIN_NEW_SMS_VALIDATION_WRONG_TEXT;
	
	if (((!newnumvalid) || (bufferchars == 0)) && ((*smsvalidflags == MMGUI_MAIN_NEW_SMS_VALIDATION_VALID) || (*smsvalidflags != newsmsvalidflags))) {
		#if GTK_CHECK_VERSION(3,10,0)
			gtk_entry_set_icon_from_icon_name(GTK_ENTRY(appdata->mmguiapp->window->smsnumberentry), GTK_ENTRY_ICON_SECONDARY, "dialog-warning");
		#else
			gtk_entry_set_icon_from_stock(GTK_ENTRY(appdata->mmguiapp->window->smsnumberentry), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CAPS_LOCK_WARNING);
		#endif
		if (!newnumvalid) {
			gtk_entry_set_icon_tooltip_markup(GTK_ENTRY(appdata->mmguiapp->window->smsnumberentry), GTK_ENTRY_ICON_SECONDARY, _("<b>SMS number is not valid</b>\n<small>Only numbers from 2 to 20 digits without\nletters and symbols can be used</small>"));
		} else if (bufferchars == 0) {
			gtk_entry_set_icon_tooltip_markup(GTK_ENTRY(appdata->mmguiapp->window->smsnumberentry), GTK_ENTRY_ICON_SECONDARY, _("<b>SMS text is not valid</b>\n<small>Please write some text to send</small>"));
		}
		gtk_widget_set_sensitive(appdata->mmguiapp->window->newsmssendtb, FALSE);
		gtk_widget_set_sensitive(appdata->mmguiapp->window->newsmssavetb, FALSE);
		*smsvalidflags = newsmsvalidflags;
	} else if ((newnumvalid) && (bufferchars > 0)) {
		#if GTK_CHECK_VERSION(3,10,0)
			gtk_entry_set_icon_from_icon_name(GTK_ENTRY(appdata->mmguiapp->window->smsnumberentry), GTK_ENTRY_ICON_SECONDARY, NULL);
		#else
			gtk_entry_set_icon_from_stock(GTK_ENTRY(appdata->mmguiapp->window->smsnumberentry), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CAPS_LOCK_WARNING);
		#endif
		gtk_entry_set_icon_tooltip_markup(GTK_ENTRY(appdata->mmguiapp->window->smsnumberentry), GTK_ENTRY_ICON_SECONDARY, NULL);
		gtk_widget_set_sensitive(appdata->mmguiapp->window->newsmssendtb, TRUE);
		gtk_widget_set_sensitive(appdata->mmguiapp->window->newsmssavetb, TRUE);
		*smsvalidflags = newsmsvalidflags;
	}
}

static enum _mmgui_main_new_sms_dialog_result mmgui_main_sms_new_dialog(mmgui_application_t mmguiapp, const gchar *number, const gchar *text)
{
	struct _mmgui_application_data appdata;
	GtkTextBuffer *buffer;
	gint response;
	gulong editnumsignal, edittextsignal;
	gint smsvalidflags;
	enum _mmgui_main_new_sms_dialog_result result;
	
	if (mmguiapp == NULL) return MMGUI_MAIN_NEW_SMS_DIALOG_CLOSE;
	
	smsvalidflags = MMGUI_MAIN_NEW_SMS_VALIDATION_VALID;
	
	appdata.mmguiapp = mmguiapp;
	appdata.data = &smsvalidflags;
	
	editnumsignal = g_signal_connect(G_OBJECT(mmguiapp->window->smsnumberentry), "changed", G_CALLBACK(mmgui_main_sms_new_dialog_number_changed_signal), &appdata);
	
	if (number != NULL) {
		gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->smsnumberentry), number);
		g_signal_emit_by_name(G_OBJECT(mmguiapp->window->smsnumberentry), "changed");
	}
	
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mmguiapp->window->smstextview));
	if (buffer != NULL) {
		edittextsignal = g_signal_connect(G_OBJECT(buffer), "changed", G_CALLBACK(mmgui_main_sms_new_dialog_number_changed_signal), &appdata);
		if (text != NULL) {
			gtk_text_buffer_set_text(buffer, text, -1);
			g_signal_emit_by_name(G_OBJECT(buffer), "changed");
		}
	}
	
	response = gtk_dialog_run(GTK_DIALOG(mmguiapp->window->newsmsdialog));
	
	g_signal_handler_disconnect(G_OBJECT(mmguiapp->window->smsnumberentry), editnumsignal);
	if (buffer != NULL) {
		g_signal_handler_disconnect(G_OBJECT(buffer), edittextsignal);
	}
	
	gtk_widget_hide(mmguiapp->window->newsmsdialog);
	
	switch (response) {
		case 0: 
			result = MMGUI_MAIN_NEW_SMS_DIALOG_CLOSE;
			break;
		case 1:
			result = MMGUI_MAIN_NEW_SMS_DIALOG_SEND;
			break;
		case 2:
			result = MMGUI_MAIN_NEW_SMS_DIALOG_SAVE;
			break;
		default:
			result = MMGUI_MAIN_NEW_SMS_DIALOG_CLOSE;
			break;
	}
	
	return result;
}

void mmgui_main_sms_new_send_signal(GtkToolButton *toolbutton, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	gtk_dialog_response(GTK_DIALOG(mmguiapp->window->newsmsdialog), MMGUI_MAIN_NEW_SMS_DIALOG_SEND);
}
               
void mmgui_main_sms_new_save_signal(GtkToolButton *toolbutton, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	gtk_dialog_response(GTK_DIALOG(mmguiapp->window->newsmsdialog), MMGUI_MAIN_NEW_SMS_DIALOG_SAVE);
}

gboolean mmgui_main_sms_send(mmgui_application_t mmguiapp, const gchar *number, const gchar *text)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *resnumber, *restext;
	enum _mmgui_main_new_sms_dialog_result result;
	mmgui_sms_message_t message;
	gchar *statusmsg;
	
	if ((mmguiapp == NULL) || (number == NULL) || (text == NULL)) return FALSE;
	
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mmguiapp->window->smstextview));
	
	if (buffer == NULL) return FALSE;
	
	/*Open dialog for user interaction*/
	result = mmgui_main_sms_new_dialog(mmguiapp, number, text);
	
	if ((result == MMGUI_MAIN_NEW_SMS_DIALOG_SEND) || (result == MMGUI_MAIN_NEW_SMS_DIALOG_SAVE)) {
		/*Get final message number and text*/
		resnumber = (gchar *)gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->smsnumberentry));
		gtk_text_buffer_get_bounds(buffer, &start, &end);
		restext = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
		/*Send message*/
		if (result == MMGUI_MAIN_NEW_SMS_DIALOG_SEND) {
			/*Start progress menubar*/
			statusmsg = g_strdup_printf(_("Sending SMS to number \"%s\"..."), resnumber);
			mmgui_ui_infobar_show(mmguiapp, statusmsg, MMGUI_MAIN_INFOBAR_TYPE_PROGRESS, NULL, NULL);
			g_free(statusmsg);
			/*Send message*/
			if (mmguicore_sms_send(mmguiapp->core, resnumber, restext, mmguiapp->options->smsvalidityperiod, mmguiapp->options->smsdeliveryreport)) {
				/*Form message*/
				message = mmgui_smsdb_message_create();
				mmgui_smsdb_message_set_number(message, resnumber);
				mmgui_smsdb_message_set_text(message, restext, FALSE);
				mmgui_smsdb_message_set_read(message, TRUE);
				mmgui_smsdb_message_set_folder(message, MMGUI_SMSDB_SMS_FOLDER_SENT);
				/*Add message to database*/
				mmgui_smsdb_add_sms(mmguicore_devices_get_sms_db(mmguiapp->core), message);
				/*Add message to list*/
				mmgui_main_sms_add_to_list(mmguiapp, message, NULL);
				/*Free message*/
				mmgui_smsdb_message_free(message);
				/*Save last number*/
				mmgui_main_sms_add_number_to_history(mmguiapp, resnumber);
				return TRUE;
			} else {
				/*Show menubar with error message*/
				mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_FAIL, _("Wrong number or device not ready"));
				return FALSE;
			}
		/*Save message*/
		} else if (result == MMGUI_MAIN_NEW_SMS_DIALOG_SAVE) {
			/*Start progress menubar*/
			mmgui_ui_infobar_show(mmguiapp, _("Saving SMS..."), MMGUI_MAIN_INFOBAR_TYPE_PROGRESS, NULL, NULL);
			/*Form message*/
			message = mmgui_smsdb_message_create();
			mmgui_smsdb_message_set_number(message, resnumber);
			mmgui_smsdb_message_set_text(message, restext, FALSE);
			mmgui_smsdb_message_set_read(message, TRUE);
			mmgui_smsdb_message_set_folder(message, MMGUI_SMSDB_SMS_FOLDER_DRAFTS);
			/*Add message to database*/
			mmgui_smsdb_add_sms(mmguicore_devices_get_sms_db(mmguiapp->core), message);
			/*Add message to list*/
			mmgui_main_sms_add_to_list(mmguiapp, message, NULL);
			/*Free message*/
			mmgui_smsdb_message_free(message);
			/*Save last number*/
			mmgui_main_sms_add_number_to_history(mmguiapp, resnumber);
			/*Show result*/
			mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_SUCCESS, NULL);
			return TRUE;
		}
	}
	
	return FALSE;
}

void mmgui_main_sms_remove(mmgui_application_t mmguiapp)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GList *selected;
	GList *listiter;
	gchar *pathstr;
	GtkTreeIter iter;
	GtkTreeRowReference *rowref;
	GtkTreePath *treepath;
	GList *rowreflist;
	guint rowcount;
	guint rmcount;
	gchar *questionstr;
	gchar *errorstr;
	gulong id;
	gboolean isfolder;
	
	if (mmguiapp == NULL) return;
		
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->smslist));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->smslist));
	
	if ((model != NULL) && (selection != NULL)) {
		selected = gtk_tree_selection_get_selected_rows(selection, &model);
		if (selected != NULL) {
			/*Get selected iter and find messages*/
			rowreflist = NULL;
			rowcount = 0;
			selected = g_list_reverse(selected);
			for (listiter = selected; listiter; listiter = listiter->next) {
				treepath = listiter->data;
				pathstr = gtk_tree_path_to_string(treepath);
				if (pathstr != NULL) {
					if (gtk_tree_model_get_iter_from_string(model, &iter, (const gchar *)pathstr)) {
						gtk_tree_model_get(model, &iter, MMGUI_MAIN_SMSLIST_ISFOLDER, &isfolder, -1);
						if (!isfolder) {
							rowref = gtk_tree_row_reference_new(model, treepath);
							rowreflist = g_list_prepend(rowreflist, rowref);
							rowcount++;
						}
					}
					g_free(pathstr);
				}
			}
			/*Work with collected row references*/
			if ((rowcount > 0) && (rowreflist != NULL)) {
				questionstr = g_strdup_printf(_("Really want to remove messages (%u) ?"), rowcount);
				if (mmgui_main_ui_question_dialog_open(mmguiapp, _("<b>Remove messages</b>"), questionstr)) {
					rmcount = 0;
					for (listiter = rowreflist; listiter; listiter = listiter->next) {
						treepath = gtk_tree_row_reference_get_path((GtkTreeRowReference *)listiter->data);
						if (treepath != NULL) {
							if (gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, treepath)) {
								gtk_tree_model_get(model, &iter, MMGUI_MAIN_SMSLIST_ID, &id, MMGUI_MAIN_SMSLIST_ISFOLDER, &isfolder, -1);
								if (!isfolder) {
									if (mmgui_smsdb_remove_sms_message(mmguicore_devices_get_sms_db(mmguiapp->core), id)) {
										/*Remove entry from list*/
										gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
										/*Ayatana menu*/
										mmgui_ayatana_set_unread_messages_number(mmguiapp->ayatana, mmgui_smsdb_get_unread_messages(mmguicore_devices_get_sms_db(mmguiapp->core)));
										/*Count message*/
										rmcount++;
									}
								}
							}
						}
					}
					/*Show error message if some messages weren't removed*/
					if (rmcount < rowcount) {
						errorstr = g_strdup_printf(_("Some messages weren\'t removed (%u)"), rowcount - rmcount);
						mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_FAIL, errorstr);
						g_free(errorstr);						
					}
				}
				g_list_foreach(rowreflist, (GFunc)gtk_tree_row_reference_free, NULL);
				g_list_free(rowreflist);
				g_free(questionstr);
			}
		}
	}
}

void mmgui_main_sms_remove_button_clicked_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_sms_remove(mmguiapp);
}

void mmgui_main_sms_new(mmgui_application_t mmguiapp)
{
	gchar *lastnumber;
	guint smscaps;
	
	if (mmguiapp == NULL) return;
	if (mmguicore_devices_get_current(mmguiapp->core) == NULL) return;
	
	smscaps = mmguicore_sms_get_capabilities(mmguiapp->core);
	
	if (smscaps & MMGUI_SMS_CAPS_SEND) {
		/*Restore last number*/
		lastnumber = mmgui_main_sms_get_first_number_from_history(mmguiapp, "8888");
		/*Open 'New message' dialog*/
		mmgui_main_sms_send(mmguiapp, lastnumber, "");
	}
}

void mmgui_main_sms_new_button_clicked_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_sms_new(mmguiapp);
}

void mmgui_main_sms_answer(mmgui_application_t mmguiapp)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GList *selected;
	GList *listiter;
	GtkTreePath *treepath;
	gchar *pathstr;
	GtkTreeIter iter;
	guint smscaps;
	gulong id;
	guint folder;
	gboolean isfolder;
	mmgui_sms_message_t message;
	
	if (mmguiapp == NULL) return;
		
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->smslist));
	
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->smslist));
	
	smscaps = mmguicore_sms_get_capabilities(mmguiapp->core);
	
	if ((model != NULL) && (selection != NULL) && (smscaps & MMGUI_SMS_CAPS_SEND)) {
		/*Get selected messages and select first one*/
		selected = gtk_tree_selection_get_selected_rows(selection, &model);
		if (selected != NULL) {
			selected = g_list_reverse(selected);
			for (listiter = selected; listiter; listiter = listiter->next) {
				treepath = listiter->data;
				pathstr = gtk_tree_path_to_string(treepath);
				if (pathstr != NULL) {
					if (gtk_tree_model_get_iter_from_string(model, &iter, (const gchar *)pathstr)) {
						gtk_tree_model_get(model, &iter, MMGUI_MAIN_SMSLIST_ID, &id, MMGUI_MAIN_SMSLIST_FOLDER, &folder, MMGUI_MAIN_SMSLIST_ISFOLDER, &isfolder, -1);
						if (!isfolder) {
							message = mmgui_smsdb_read_sms_message(mmguicore_devices_get_sms_db(mmguiapp->core), id);
							if (message != NULL) {
								/*Open dialog*/
								if (mmguicore_sms_validate_number(mmgui_smsdb_message_get_number(message))) {
									if (folder == MMGUI_SMSDB_SMS_FOLDER_DRAFTS) {
										mmgui_main_sms_send(mmguiapp, mmgui_smsdb_message_get_number(message), mmgui_smsdb_message_get_text(message));
									} else {
										mmgui_main_sms_send(mmguiapp, mmgui_smsdb_message_get_number(message), "");
									}
									break;
								}
								/*Free message*/
								mmgui_smsdb_message_free(message);
							}
						}
					}
					g_free(pathstr);
				}
			}
		}
	}
}

void mmgui_main_sms_answer_button_clicked_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
		
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
		
	mmgui_main_sms_answer(mmguiapp);
}

static void mmgui_main_sms_list_selection_changed_signal(GtkTreeSelection *selection, gpointer data)
{
	mmgui_application_t mmguiapp;
	mmgui_sms_message_t sms;
	guint smscaps;
	GtkTreeModel *model;
	GList *selected;
	GList *listiter;
	gchar *pathstr;
	GtkTextIter starttextiter, endtextiter;
	GtkTextBuffer *textbuffer;
	time_t timestamp;
	gchar timestr[200];
	GtkTreeIter iter;
	GtkTreeRowReference *rowref;
	GtkTreePath *treepath;
	GList *msgreflist, *folderreflist;
	guint msgcount, foldercount, folder;
	gulong id, lastid;
	gboolean isfolder, numchecked, canremove, cananswer;
	gchar *msgnumber;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->smslist));
	
	textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mmguiapp->window->smstext));
	
	smscaps = mmguicore_sms_get_capabilities(mmguiapp->core);
	
	if (textbuffer != NULL) {
		/*Clear text*/
		gtk_text_buffer_get_bounds(textbuffer, &starttextiter, &endtextiter);
		gtk_text_buffer_delete(textbuffer, &starttextiter, &endtextiter);
	}
	
	if ((model != NULL) && (selection != NULL) && (textbuffer != NULL)) {
		selected = gtk_tree_selection_get_selected_rows(selection, &model);
		if (selected != NULL) {
			/*Get selected iter and find messages and folders*/
			msgreflist = NULL;
			msgcount = 0;
			folderreflist = NULL;
			foldercount = 0;
			canremove = FALSE;
			cananswer = FALSE;
			selected = g_list_reverse(selected);
			for (listiter = selected; listiter; listiter = listiter->next) {
				treepath = listiter->data;
				pathstr = gtk_tree_path_to_string(treepath);
				if (pathstr != NULL) {
					if (gtk_tree_model_get_iter_from_string(model, &iter, (const gchar *)pathstr)) {
						gtk_tree_model_get(model, &iter, MMGUI_MAIN_SMSLIST_ISFOLDER, &isfolder, -1);
						rowref = gtk_tree_row_reference_new(model, treepath);
						if (isfolder) {
							folderreflist = g_list_prepend(folderreflist, rowref);
							foldercount++;
						} else {
							msgreflist = g_list_prepend(msgreflist, rowref);
							msgcount++;							
						}
					}
					g_free(pathstr);
				}
			}
			
			lastid = 0;
			
			/*Show messages using collected row references*/
			if ((msgcount > 0) && (msgreflist != NULL)) {
				numchecked = FALSE;
				msgnumber = NULL;
				for (listiter = msgreflist; listiter; listiter = listiter->next) {
					treepath = gtk_tree_row_reference_get_path((GtkTreeRowReference *)listiter->data);
					if (treepath != NULL) {
						if (gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, treepath)) {
							gtk_tree_model_get(model, &iter, MMGUI_MAIN_SMSLIST_ID, &id, MMGUI_MAIN_SMSLIST_ISFOLDER, &isfolder, -1);
							if (!isfolder) {
								sms = mmgui_smsdb_read_sms_message(mmguicore_devices_get_sms_db(mmguiapp->core), id);
								if (sms != NULL) {
									/*Message heading*/
									gtk_text_buffer_get_end_iter(textbuffer, &endtextiter);
									gtk_text_buffer_insert_with_tags(textbuffer, &endtextiter, mmgui_smsdb_message_get_number(sms), -1, mmguiapp->window->smsheadingtag, NULL);
									/*Timestamp*/
									timestamp = mmgui_smsdb_message_get_timestamp(sms);
									gtk_text_buffer_get_end_iter(textbuffer, &endtextiter);
									gtk_text_buffer_insert_with_tags(textbuffer, &endtextiter, mmgui_str_format_sms_time(timestamp, timestr, sizeof(timestr)), -1, mmguiapp->window->smsdatetag, NULL);
									gtk_text_buffer_get_end_iter(textbuffer, &endtextiter);
									gtk_text_buffer_insert(textbuffer, &endtextiter, "\n", -1);
									/*Message text*/
									gtk_text_buffer_get_end_iter(textbuffer, &endtextiter);
									gtk_text_buffer_insert(textbuffer, &endtextiter, mmgui_smsdb_message_get_text(sms), -1);
									gtk_text_buffer_get_end_iter(textbuffer, &endtextiter);
									gtk_text_buffer_insert(textbuffer, &endtextiter, "\n\n", -1);
									
									/*Test if sender number is available*/
									if (smscaps & MMGUI_SMS_CAPS_SEND) {
										if (!numchecked) {
											if (mmguicore_sms_validate_number(mmgui_smsdb_message_get_number(sms)) && (smscaps & MMGUI_SMS_CAPS_SEND)) {
												msgnumber = g_strdup(mmgui_smsdb_message_get_number(sms));
												cananswer = TRUE;
											}
											numchecked = TRUE;
										} else {
											if (cananswer) {
												if (mmgui_smsdb_message_get_number(sms) != NULL) {
													if (!g_str_equal(msgnumber, mmgui_smsdb_message_get_number(sms))) {
														cananswer = FALSE;
													}
												} else {
													cananswer = FALSE;
												}
											}
										}
									}
									
									canremove = TRUE;
									
									/*Set read flag if not set before*/
									if (!mmgui_smsdb_message_get_read(sms)) {
										if (mmgui_smsdb_set_message_read_status(mmguicore_devices_get_sms_db(mmguiapp->core), id, TRUE)) {
											gtk_tree_store_set(GTK_TREE_STORE(model), &iter, MMGUI_MAIN_SMSLIST_ICON, mmguiapp->window->smsreadicon, -1);
										}
										/*Ayatana menu*/
										mmgui_ayatana_set_unread_messages_number(mmguiapp->ayatana, mmgui_smsdb_get_unread_messages(mmguicore_devices_get_sms_db(mmguiapp->core)));
									}
									/*Save last message ID*/
									lastid = id;
									/*Free SMS message*/
									mmgui_smsdb_message_free(sms);
								}
							}
						}
					}
				}
				g_list_foreach(msgreflist, (GFunc)gtk_tree_row_reference_free, NULL);
				g_list_free(msgreflist);
				if (msgnumber != NULL) {
					g_free(msgnumber);
				}
				/*Save selected message identifier*/
				if (lastid != 0) {
					mmgui_modem_settings_set_boolean(mmguiapp->modemsettings, "sms_is_folder", FALSE);
					mmgui_modem_settings_set_int64(mmguiapp->modemsettings, "sms_entry_id", lastid);
				}
			}
			
			/*Enable Answer and remove buttons if needed*/
			gtk_widget_set_sensitive(mmguiapp->window->answersmsbutton, cananswer);
			gtk_widget_set_sensitive(mmguiapp->window->removesmsbutton, canremove);
			
			/*Show folders descriptions using collected row references*/
			if ((foldercount > 0) && (folderreflist != NULL)) {
				/*Show tips only if no messages selected*/
				if (lastid == 0) {
					for (listiter = folderreflist; listiter; listiter = listiter->next) {
						treepath = gtk_tree_row_reference_get_path((GtkTreeRowReference *)listiter->data);
						if (treepath != NULL) {
							if (gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, treepath)) {
								gtk_tree_model_get(model, &iter, MMGUI_MAIN_SMSLIST_FOLDER, &folder, MMGUI_MAIN_SMSLIST_ISFOLDER, &isfolder, -1);
								if (isfolder) {
									/*Folder selected*/
									switch (folder) {
										case MMGUI_SMSDB_SMS_FOLDER_INCOMING:
											/*Folder heading*/
											gtk_text_buffer_get_end_iter(textbuffer, &endtextiter);
											gtk_text_buffer_insert_with_tags(textbuffer, &endtextiter, _("Incoming"), -1, mmguiapp->window->smsheadingtag, NULL);
											gtk_text_buffer_get_end_iter(textbuffer, &endtextiter);
											gtk_text_buffer_insert(textbuffer, &endtextiter, "\n", -1);
											/*Folder description*/
											gtk_text_buffer_get_end_iter(textbuffer, &endtextiter);
											gtk_text_buffer_insert(textbuffer, &endtextiter, _("This is folder for your incoming SMS messages.\nYou can answer selected message using 'Answer' button."), -1);
											break;
										case MMGUI_SMSDB_SMS_FOLDER_SENT:
											/*Folder heading*/
											gtk_text_buffer_get_end_iter(textbuffer, &endtextiter);
											gtk_text_buffer_insert_with_tags(textbuffer, &endtextiter, _("Sent"), -1, mmguiapp->window->smsheadingtag, NULL);
											gtk_text_buffer_get_end_iter(textbuffer, &endtextiter);
											gtk_text_buffer_insert(textbuffer, &endtextiter, "\n", -1);
											/*Folder description*/
											gtk_text_buffer_get_end_iter(textbuffer, &endtextiter);
											gtk_text_buffer_insert(textbuffer, &endtextiter, _("This is folder for your sent SMS messages."), -1);
											break;
										case MMGUI_SMSDB_SMS_FOLDER_DRAFTS:
											/*Folder heading*/
											gtk_text_buffer_get_end_iter(textbuffer, &endtextiter);
											gtk_text_buffer_insert_with_tags(textbuffer, &endtextiter, _("Drafts"), -1, mmguiapp->window->smsheadingtag, NULL);
											gtk_text_buffer_get_end_iter(textbuffer, &endtextiter);
											gtk_text_buffer_insert(textbuffer, &endtextiter, "\n", -1);
											/*Folder description*/
											gtk_text_buffer_get_end_iter(textbuffer, &endtextiter);
											gtk_text_buffer_insert(textbuffer, &endtextiter, _("This is folder for your SMS message drafts.\nSelect message and click 'Answer' button to start editing."), -1);
											break;
										default:
											break;
									}
								}
							}
						}
					}
				}
				g_list_foreach(folderreflist, (GFunc)gtk_tree_row_reference_free, NULL);
				g_list_free(folderreflist);
				/*Save selected folder identifier*/
				if (lastid == 0) {
					mmgui_modem_settings_set_boolean(mmguiapp->modemsettings, "sms_is_folder", FALSE);
					mmgui_modem_settings_set_int64(mmguiapp->modemsettings, "sms_entry_id", id);
				}
			}
		}
	}
}

static void mmgui_main_sms_list_row_activated_signal(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_sms_answer(mmguiapp);
}

static void mmgui_main_sms_add_to_list(mmgui_application_t mmguiapp, mmgui_sms_message_t sms, GtkTreeModel *model)
{
	GtkTreeIter iter, child;
	time_t timestamp;
	gchar *markup;
	//struct tm *tmptime;
	gchar timestr[200];
	GdkPixbuf *icon;
	guint folder;
	
	if ((mmguiapp == NULL) || (sms == NULL)) return;
	
	if (model == NULL) {
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->smslist));
	}
	
	timestamp = mmgui_smsdb_message_get_timestamp(sms);
	
	markup = g_strdup_printf(_("<b>%s</b>\n<small>%s</small>"), mmgui_smsdb_message_get_number(sms), mmgui_str_format_sms_time(timestamp, timestr, sizeof(timestr))); 
	
	if (mmgui_smsdb_message_get_read(sms)) {
		icon = mmguiapp->window->smsreadicon;
	} else {
		icon = mmguiapp->window->smsunreadicon;
	}
	
	switch (mmgui_smsdb_message_get_folder(sms)) {
		case MMGUI_SMSDB_SMS_FOLDER_INCOMING:
			folder = MMGUI_SMSDB_SMS_FOLDER_INCOMING;
			gtk_tree_model_get_iter(model, &iter, mmguiapp->window->incomingpath);
			break;
		case MMGUI_SMSDB_SMS_FOLDER_SENT:
			folder = MMGUI_SMSDB_SMS_FOLDER_SENT;
			gtk_tree_model_get_iter(model, &iter, mmguiapp->window->sentpath);
			break;
		case MMGUI_SMSDB_SMS_FOLDER_DRAFTS:
			folder = MMGUI_SMSDB_SMS_FOLDER_DRAFTS;
			gtk_tree_model_get_iter(model, &iter, mmguiapp->window->draftspath);
			break;
		default:
			folder = MMGUI_SMSDB_SMS_FOLDER_INCOMING;
			gtk_tree_model_get_iter(model, &iter, mmguiapp->window->incomingpath);
			break;
	}
	
	//Place new message on top or append to list
	if (mmguiapp->options->smsoldontop) {
		gtk_tree_store_append(GTK_TREE_STORE(model), &child, &iter);
	} else {
		gtk_tree_store_insert(GTK_TREE_STORE(model), &child, &iter, 0);
	}
	
	gtk_tree_store_set(GTK_TREE_STORE(model), &child,
						MMGUI_MAIN_SMSLIST_ICON, icon,
						MMGUI_MAIN_SMSLIST_SMS, markup,
						MMGUI_MAIN_SMSLIST_ID, mmgui_smsdb_message_get_db_identifier(sms),
						MMGUI_MAIN_SMSLIST_FOLDER, folder,
						MMGUI_MAIN_SMSLIST_ISFOLDER, FALSE,
						-1);
	
	g_free(markup);
}

gboolean mmgui_main_sms_list_fill(mmgui_application_t mmguiapp)
{
	GSList *smslist;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint i;
	GSList *iterator;
	mmgui_application_data_t appdata;
		
	gchar *foldercomments[3]       = {_("<b>Incoming</b>\n<small>Incoming messages</small>"), 
										_("<b>Sent</b>\n<small>Sent messages</small>"),
										_("<b>Drafts</b>\n<small>Message drafts</small>")};
	const guint folderids[3]       = {MMGUI_SMSDB_SMS_FOLDER_INCOMING,
										MMGUI_SMSDB_SMS_FOLDER_SENT,
										MMGUI_SMSDB_SMS_FOLDER_DRAFTS};
	GtkTreePath **folderpath[3]    = {&mmguiapp->window->incomingpath,
										&mmguiapp->window->sentpath,
										&mmguiapp->window->draftspath};
	GdkPixbuf **foldericon[3]      = {&mmguiapp->window->smsrecvfoldericon, 
										&mmguiapp->window->smssentfoldericon, 
										&mmguiapp->window->smsdraftsfoldericon};
	
	if (mmguiapp == NULL) return FALSE;
	
	if (mmguicore_devices_get_current(mmguiapp->core) != NULL) {
		smslist = mmgui_smsdb_read_sms_list(mmguicore_devices_get_sms_db(mmguiapp->core));
		
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->smslist));
		if (model != NULL) {
			/*Detach and clear model*/
			g_object_ref(model);
			gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->smslist), NULL);
			gtk_tree_store_clear(GTK_TREE_STORE(model));
			/*Add folders*/
			for (i = 0; i < 3; i++) {
				gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
				gtk_tree_store_set(GTK_TREE_STORE(model), &iter, MMGUI_MAIN_SMSLIST_ICON, *foldericon[i], MMGUI_MAIN_SMSLIST_SMS, foldercomments[i], MMGUI_MAIN_SMSLIST_ID, 0, MMGUI_MAIN_SMSLIST_FOLDER, folderids[i], MMGUI_MAIN_SMSLIST_ISFOLDER, TRUE, -1);
				*(folderpath[i]) = gtk_tree_model_get_path(model, &iter);
			}
			/*Add messages*/
			if (smslist != NULL) {
				for (iterator=smslist; iterator; iterator=iterator->next) {
					mmgui_main_sms_add_to_list(mmguiapp, (mmgui_sms_message_t)iterator->data, model);
				}
			}
			/*Attach model*/
			gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->smslist), model);
			g_object_unref(model);
			/*Expand folders if needed*/
			if (mmguiapp->options->smsexpandfolders) {
				gtk_tree_view_expand_all(GTK_TREE_VIEW(mmguiapp->window->smslist));
			}
		}
		
		/*Free resources*/
		if (smslist != NULL) {
			mmgui_smsdb_message_free_list(smslist);
		}
		
		//Get new messages from modem
		appdata = g_new0(struct _mmgui_application_data, 1);
		appdata->mmguiapp = mmguiapp;
		appdata->data = GUINT_TO_POINTER(mmguiapp->options->concatsms);
		
		g_idle_add(mmgui_main_sms_get_message_list_from_thread, appdata);
	}
	
	return FALSE;
}

void mmgui_main_sms_restore_settings_for_modem(mmgui_application_t mmguiapp)
{
	gulong entryid;
	gboolean entryisfolder;
	
	
	if (mmguiapp == NULL) return;
	if (mmguiapp->modemsettings == NULL) return;
	
	/*Get settings*/
	entryid = (gulong)mmgui_modem_settings_get_int64(mmguiapp->modemsettings, "sms_entry_id", 0);
	entryisfolder = mmgui_modem_settings_get_boolean(mmguiapp->modemsettings, "sms_is_folder", TRUE);

	/*Select last entry*/
	mmgui_main_sms_select_entry_from_list(mmguiapp, entryid, entryisfolder);
	
	/*Add history entries to dropdown list model*/
	mmgui_main_sms_load_numbers_history(mmguiapp);
	
	/*Restore modem contacts if available*/
	mmgui_main_sms_restore_contacts_for_modem(mmguiapp);
}

void mmgui_main_sms_restore_contacts_for_modem(mmgui_application_t mmguiapp)
{
	if (mmguiapp == NULL) return;
	
	/*Add contacts from modem to autocompletion model*/
	mmgui_main_sms_autocompletion_model_fill(mmguiapp, MMGUI_MAIN_CONTACT_MODEM);

	/*Add contacts from modem to dropdown list model*/
	mmgui_main_sms_menu_model_fill(mmguiapp, MMGUI_MAIN_SMS_SOURCE_MODEM);	
}

static void mmgui_main_sms_load_numbers_history(mmgui_application_t mmguiapp)
{
	gchar **numbers;
	GSList *iterator;
	guint numcount, i;
	gchar *number, *fullname;
	GtkTreeIter iter;
	
	if (mmguiapp == NULL) return;
	
	numbers = mmgui_modem_settings_get_string_list(mmguiapp->modemsettings, "sms_numbers_history", NULL);
	
	if (numbers == NULL) return;
	
	mmguiapp->window->smsnumlisthistory = NULL;
	
	i = 0;
	numcount = 0;
	
	while (numbers[i] != NULL) {
		/*Limit the number of recent entries*/
		if (numcount >= 5) {
			break;
		}
		/*Validate every entry*/
		if (mmguicore_sms_validate_number(numbers[i])) {
			mmguiapp->window->smsnumlisthistory = g_slist_prepend(mmguiapp->window->smsnumlisthistory, g_strdup(numbers[i]));
			numcount++;
		}
		i++;
	}
	/*Free string list*/
	g_strfreev(numbers);
	
	if (mmguiapp->window->smsnumlisthistory != NULL) {
		/*Add to dropdown list*/
		for (iterator=mmguiapp->window->smsnumlisthistory; iterator; iterator=iterator->next) {
			number = iterator->data;
			fullname = mmgui_main_sms_get_name_for_number(mmguiapp, number);
			gtk_tree_store_insert(mmguiapp->window->smsnumlistmodel, &iter, NULL, 0);
			gtk_tree_store_set(mmguiapp->window->smsnumlistmodel, &iter, MMGUI_MAIN_SMS_LIST_NAME, fullname,
																		MMGUI_MAIN_SMS_LIST_NUMBER, number,
																		MMGUI_MAIN_SMS_LIST_SOURCE, MMGUI_MAIN_SMS_SOURCE_HISTORY,
																		-1);
			g_free(fullname);
		}
		/*Reverse linked list*/
		mmguiapp->window->smsnumlisthistory = g_slist_reverse(mmguiapp->window->smsnumlisthistory);
	}
}

static void mmgui_main_sms_add_number_to_history(mmgui_application_t mmguiapp, gchar *number)
{
	GtkTreeIter iter;
	guint contactsource;
	gchar *contactnum, *oldnumber, *fullname;
	gboolean valid;
	gchar **numbers;
	GSList *iterator;
	gsize length;
	guint i;
	
	if ((mmguiapp == NULL) || (number == NULL)) return;
		
	if ((mmguiapp->window->smsnumlisthistory == NULL) || ((mmguiapp->window->smsnumlisthistory != NULL) && (g_slist_length(mmguiapp->window->smsnumlisthistory) < 5))) {
		/*Just add string to linked list and dropdown list*/
		/*Add to the beginning of linked list*/
		mmguiapp->window->smsnumlisthistory = g_slist_prepend(mmguiapp->window->smsnumlisthistory, g_strdup(number));
		/*Add to beginning of dropdown list*/
		fullname = mmgui_main_sms_get_name_for_number(mmguiapp, number);
		gtk_tree_store_insert(mmguiapp->window->smsnumlistmodel, &iter, NULL, 0);
		gtk_tree_store_set(mmguiapp->window->smsnumlistmodel, &iter, MMGUI_MAIN_SMS_LIST_NAME, fullname,
																	MMGUI_MAIN_SMS_LIST_NUMBER, number,
																	MMGUI_MAIN_SMS_LIST_SOURCE, MMGUI_MAIN_SMS_SOURCE_HISTORY,
																	-1);
		g_free(fullname);
	} else {
		/*Add new entry and remove old one*/
		iterator = g_slist_last(mmguiapp->window->smsnumlisthistory);
		oldnumber = iterator->data;
		/*Remove from linked list*/
		mmguiapp->window->smsnumlisthistory = g_slist_remove(mmguiapp->window->smsnumlisthistory, oldnumber);
		/*Add to the beginning of linked list*/
		mmguiapp->window->smsnumlisthistory = g_slist_prepend(mmguiapp->window->smsnumlisthistory, g_strdup(number));
		/*Remove from dropdown list*/
		valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter);
		while (valid) {
			gtk_tree_model_get(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter, MMGUI_MAIN_SMS_LIST_SOURCE, &contactsource, MMGUI_MAIN_SMS_LIST_NUMBER, &contactnum, -1);
			if ((contactsource == MMGUI_MAIN_SMS_SOURCE_HISTORY) && (g_str_equal(contactnum, oldnumber))) {
				gtk_tree_store_remove(GTK_TREE_STORE(mmguiapp->window->smsnumlistmodel), &iter);
				g_free(contactnum);
				break;
			}
			g_free(contactnum);
			valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter);
		}
		g_free(oldnumber);
		/*Add to beginning of dropdown list*/
		fullname = mmgui_main_sms_get_name_for_number(mmguiapp, number);
		gtk_tree_store_insert(mmguiapp->window->smsnumlistmodel, &iter, NULL, 0);
		gtk_tree_store_set(mmguiapp->window->smsnumlistmodel, &iter, MMGUI_MAIN_SMS_LIST_NAME, fullname,
																	MMGUI_MAIN_SMS_LIST_NUMBER, number,
																	MMGUI_MAIN_SMS_LIST_SOURCE, MMGUI_MAIN_SMS_SOURCE_HISTORY,
																	-1);
		g_free(fullname);
	}
	
	/*Save changed list*/
	length = g_slist_length(mmguiapp->window->smsnumlisthistory);
	
	numbers = g_malloc0((length * sizeof(gchar *)) + 1);
	
	i = 0;
	for (iterator=mmguiapp->window->smsnumlisthistory; iterator; iterator=iterator->next) {
		contactnum = iterator->data;
		numbers[i] = g_strdup(contactnum);
		i++;		
	}
	
	numbers[length] = NULL;
	
	mmgui_modem_settings_set_string_list(mmguiapp->modemsettings, "sms_numbers_history", numbers);
	
	g_strfreev(numbers);
}

static gchar *mmgui_main_sms_get_first_number_from_history(mmgui_application_t mmguiapp, gchar *defnumber)
{
	if (mmguiapp != NULL) {
		if (mmguiapp->window->smsnumlisthistory != NULL) {
			/*Most recent number from list*/
			return mmguiapp->window->smsnumlisthistory->data;
		} else {
			/*Hisory linked list doesnt contain numbers*/
			return defnumber;
		}
	} else {
		/*Something wrong*/
		return defnumber;
	}
}

static gchar *mmgui_main_sms_get_name_for_number(mmgui_application_t mmguiapp, gchar *number)
{
	gchar *res;
	GSList *contacts[3];
	guint contactscaps, i;
	GSList *iterator;
	mmgui_contact_t contact;
	
	if ((mmguiapp == NULL) || (number == NULL)) return NULL;
	
	contacts[0] = NULL;
	contactscaps = mmguicore_contacts_get_capabilities(mmguiapp->core);
	if (contactscaps & MMGUI_CONTACTS_CAPS_EXPORT) {
		contacts[0] = mmguicore_contacts_list(mmguiapp->core);
	}
	contacts[1] = NULL;
	if (mmgui_addressbooks_get_gnome_contacts_available(mmguiapp->addressbooks)) {
		contacts[1] = mmgui_addressbooks_get_gnome_contacts_list(mmguiapp->addressbooks);
	}
	contacts[2] = NULL;
	if (mmgui_addressbooks_get_kde_contacts_available(mmguiapp->addressbooks)) {
		contacts[2] = mmgui_addressbooks_get_kde_contacts_list(mmguiapp->addressbooks);
	}
	
	i = 0;
	res = NULL;
	
	while ((i < 3) && (res == NULL)) {
		if (contacts[i] != NULL) {
			for (iterator=contacts[i]; iterator; iterator=iterator->next) {
				contact = iterator->data;
				if (contact->number != NULL) {
					if (g_str_equal(number, contact->number)) {
						res = g_strdup_printf("%s (%s)", contact->name, number);
						break;
					}
				}
				if (contact->number2 != NULL) {
					if (g_str_equal(number, contact->number2)) {
						res = g_strdup_printf("%s (%s)", contact->name, number);
						break;
					}
				}
			}
		}
		i++;
	}
	
	if (res == NULL) {
		res = g_strdup(number);
	}
	
	return res;
}

static gboolean mmgui_main_sms_autocompletion_select_entry_signal(GtkEntryCompletion *widget, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	mmgui_application_t mmguiapp;
	gchar *number;
	
	mmguiapp = (mmgui_application_t)user_data;
	
	gtk_tree_model_get(model, iter, MMGUI_MAIN_SMS_COMPLETION_NUMBER, &number, -1);
	if (number != NULL) {
		gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->smsnumberentry), number);
		g_free(number);
	}
	
	return TRUE;	
}

static void mmgui_main_sms_autocompletion_model_fill(mmgui_application_t mmguiapp, guint source)
{
	GSList *contacts;
	guint contactscaps;
	GtkTreeIter iter;
	GSList *iterator;
	mmgui_contact_t contact;
	
	if (mmguiapp == NULL) return;
	if (mmguiapp->addressbooks == NULL) return;
	if (mmguiapp->window->smscompletionmodel == NULL) return;
	
	contacts = NULL;
	
	if (source == MMGUI_MAIN_CONTACT_GNOME) {
		/*Contacts from GNOME addressbook*/
		if (mmgui_addressbooks_get_gnome_contacts_available(mmguiapp->addressbooks)) {
			contacts = mmgui_addressbooks_get_gnome_contacts_list(mmguiapp->addressbooks);
		}
	} else if (source == MMGUI_MAIN_CONTACT_KDE) {
		/*Contacts from KDE addressbook*/
		if (mmgui_addressbooks_get_kde_contacts_available(mmguiapp->addressbooks)) {
			contacts = mmgui_addressbooks_get_kde_contacts_list(mmguiapp->addressbooks);
		}
	} else if (source == MMGUI_MAIN_CONTACT_MODEM) {
		/*Contacts from modem*/
		contactscaps = mmguicore_contacts_get_capabilities(mmguiapp->core);
		if (contactscaps & MMGUI_CONTACTS_CAPS_EXPORT) {
			contacts = mmguicore_contacts_list(mmguiapp->core);
		}
	}
	
	if (contacts != NULL) {
		for (iterator=contacts; iterator; iterator=iterator->next) {
			contact = iterator->data;
			/*Add first number*/
			if (mmguicore_sms_validate_number(contact->number)) {
				gtk_list_store_append(GTK_LIST_STORE(mmguiapp->window->smscompletionmodel), &iter);
				gtk_list_store_set(GTK_LIST_STORE(mmguiapp->window->smscompletionmodel), &iter, MMGUI_MAIN_SMS_COMPLETION_NAME, contact->name,
																								MMGUI_MAIN_SMS_COMPLETION_NUMBER, contact->number,
																								MMGUI_MAIN_SMS_COMPLETION_SOURCE, source,
																								-1);
			}
			/*Add second number if exists*/
			if (mmguicore_sms_validate_number(contact->number2)) {
				gtk_list_store_append(GTK_LIST_STORE(mmguiapp->window->smscompletionmodel), &iter);
				gtk_list_store_set(GTK_LIST_STORE(mmguiapp->window->smscompletionmodel), &iter, MMGUI_MAIN_SMS_COMPLETION_NAME, contact->name,
																								MMGUI_MAIN_SMS_COMPLETION_NUMBER, contact->number2,
																								MMGUI_MAIN_SMS_COMPLETION_SOURCE, source,
																								-1);
			}
		}
	}
}

static void mmgui_main_sms_menu_model_fill(mmgui_application_t mmguiapp, guint source)
{
	GSList *contacts;
	guint contactscaps;
	GtkTreeIter iter, contiter;
	GSList *iterator;
	gchar *fullname;
	mmgui_contact_t contact;
	
	if (mmguiapp == NULL) return;
	if (mmguiapp->addressbooks == NULL) return;
	if (mmguiapp->window->smsnumlistmodel == NULL) return;
	
	contacts = NULL;
	
	if (source == MMGUI_MAIN_SMS_SOURCE_GNOME) {
		/*Contacts from GNOME addressbook*/
		if (mmgui_addressbooks_get_gnome_contacts_available(mmguiapp->addressbooks)) {
			contacts = mmgui_addressbooks_get_gnome_contacts_list(mmguiapp->addressbooks);
			if (contacts != NULL) {
				if (mmguiapp->window->smsnumlistgnomepath == NULL) {
					gtk_tree_store_append(mmguiapp->window->smsnumlistmodel, &iter, NULL);
					gtk_tree_store_set(mmguiapp->window->smsnumlistmodel, &iter, MMGUI_MAIN_SMS_LIST_NAME, _("<b>GNOME contacts</b>"),
																				MMGUI_MAIN_SMS_LIST_NUMBER, NULL,
																				MMGUI_MAIN_SMS_LIST_SOURCE, MMGUI_MAIN_SMS_SOURCE_CAPTION,
																				-1);
					mmguiapp->window->smsnumlistgnomepath = gtk_tree_model_get_path(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter);
				} else {
					gtk_tree_model_get_iter(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter, mmguiapp->window->smsnumlistgnomepath);
				}
			}
		}
	} else if (source == MMGUI_MAIN_SMS_SOURCE_KDE) {
		/*Contacts from KDE addressbook*/
		if (mmgui_addressbooks_get_kde_contacts_available(mmguiapp->addressbooks)) {
			contacts = mmgui_addressbooks_get_kde_contacts_list(mmguiapp->addressbooks);
			if (contacts != NULL) {
				if (mmguiapp->window->smsnumlistkdepath == NULL) {
					gtk_tree_store_append(mmguiapp->window->smsnumlistmodel, &iter, NULL);
					gtk_tree_store_set(mmguiapp->window->smsnumlistmodel, &iter, MMGUI_MAIN_SMS_LIST_NAME, _("<b>KDE contacts</b>"),
																				MMGUI_MAIN_SMS_LIST_NUMBER, NULL,
																				MMGUI_MAIN_SMS_LIST_SOURCE, MMGUI_MAIN_SMS_SOURCE_CAPTION,
																				-1);
					mmguiapp->window->smsnumlistkdepath = gtk_tree_model_get_path(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter);
				} else {
					gtk_tree_model_get_iter(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter, mmguiapp->window->smsnumlistkdepath);
				}
			}
		}
	} else if (source == MMGUI_MAIN_SMS_SOURCE_MODEM) {
		/*Contacts from modem*/
		contactscaps = mmguicore_contacts_get_capabilities(mmguiapp->core);
		if (contactscaps & MMGUI_CONTACTS_CAPS_EXPORT) {
			contacts = mmguicore_contacts_list(mmguiapp->core);
			if (contacts != NULL) {
				if (mmguiapp->window->smsnumlistmodempath == NULL) {
					gtk_tree_store_append(mmguiapp->window->smsnumlistmodel, &iter, NULL);
					gtk_tree_store_set(mmguiapp->window->smsnumlistmodel, &iter, MMGUI_MAIN_SMS_LIST_NAME, _("<b>Modem contacts</b>"),
																				MMGUI_MAIN_SMS_LIST_NUMBER, NULL,
																				MMGUI_MAIN_SMS_LIST_SOURCE, MMGUI_MAIN_SMS_SOURCE_CAPTION,
																				-1);
					mmguiapp->window->smsnumlistmodempath = gtk_tree_model_get_path(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter);
				} else {
					gtk_tree_model_get_iter(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter, mmguiapp->window->smsnumlistmodempath);
				}
			}
		}
	}
	
	if (contacts != NULL) {
		for (iterator=contacts; iterator; iterator=iterator->next) {
			contact = iterator->data;
			/*Add first number*/
			if (mmguicore_sms_validate_number(contact->number)) {
				fullname = g_strdup_printf("%s (%s)", contact->name, contact->number);
				gtk_tree_store_append(GTK_TREE_STORE(mmguiapp->window->smsnumlistmodel), &contiter, &iter);
				gtk_tree_store_set(GTK_TREE_STORE(mmguiapp->window->smsnumlistmodel), &contiter, MMGUI_MAIN_SMS_LIST_NAME, fullname,
																								MMGUI_MAIN_SMS_LIST_NUMBER, contact->number,
																								MMGUI_MAIN_SMS_LIST_SOURCE, source,
																								-1);
				g_free(fullname);
			}
			/*Add second number if exists*/
			if (mmguicore_sms_validate_number(contact->number2)) {
				fullname = g_strdup_printf("%s (%s)", contact->name, contact->number2);
				gtk_tree_store_append(GTK_TREE_STORE(mmguiapp->window->smsnumlistmodel), &contiter, &iter);
				gtk_tree_store_set(GTK_TREE_STORE(mmguiapp->window->smsnumlistmodel), &contiter, MMGUI_MAIN_SMS_LIST_NAME, fullname,
																								MMGUI_MAIN_SMS_LIST_NUMBER, contact->number2,
																								MMGUI_MAIN_SMS_LIST_SOURCE, source,
																								-1);
				g_free(fullname);
			}
		}
	}
}

static void mmgui_main_sms_menu_data_func(GtkCellLayout *cell_layout, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data)
{
	gboolean sensitive;
	
	sensitive = !gtk_tree_model_iter_has_child(tree_model, iter);
	
	g_object_set(cell, "sensitive", sensitive, NULL);
}

static gboolean mmgui_main_sms_menu_separator_func(GtkTreeModel *model, GtkTreeIter  *iter, gpointer data)
{
	gint source;
	
	gtk_tree_model_get(model, iter, MMGUI_MAIN_SMS_LIST_SOURCE, &source, -1);
	
	if (source == MMGUI_MAIN_SMS_SOURCE_SEPARATOR) {
		return TRUE;
	} else {
		return FALSE;
	}
}

static gchar *mmgui_main_sms_list_select_entry_signal(GtkComboBox *combo, const gchar *path, gpointer user_data)
{
	mmgui_application_t mmguiapp;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *number;
		
	mmguiapp = (mmgui_application_t)user_data;
	
	model = gtk_combo_box_get_model(combo);
	gtk_tree_model_get_iter_from_string(model, &iter, path);
	
	gtk_tree_model_get(model, &iter, MMGUI_MAIN_SMS_LIST_NUMBER, &number, -1);
	
	if (number == NULL) {
		number = g_strdup(gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->smsnumberentry)));
	}
		
	return number;
}

#if RESOURCE_SPELLCHECKER_ENABLED
void mmgui_main_sms_new_disable_spellchecker_signal(GtkToolButton *toolbutton, gpointer data)
{
	mmgui_application_t mmguiapp;
	const gchar *langcode;
	gchar *langname;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	if (mmguiapp->window->newsmsspellchecker == NULL) return;
	
	if (gtk_spell_checker_get_from_text_view(GTK_TEXT_VIEW(mmguiapp->window->smstextview)) != NULL) {
		/*Detach spellchecker*/
		g_object_ref(mmguiapp->window->newsmsspellchecker);
		gtk_spell_checker_detach(mmguiapp->window->newsmsspellchecker);
		gmm_settings_set_boolean(mmguiapp->settings, "spell_checker_enabled", FALSE);
		gtk_tool_button_set_label(GTK_TOOL_BUTTON(mmguiapp->window->newsmsspellchecktb), _("Disabled"));
	} else {
		/*Attach spellchecker*/
		gtk_spell_checker_attach(mmguiapp->window->newsmsspellchecker, GTK_TEXT_VIEW(mmguiapp->window->smstextview));
		g_object_unref(mmguiapp->window->newsmsspellchecker);
		gmm_settings_set_boolean(mmguiapp->settings, "spell_checker_enabled", TRUE);
		langcode = gtk_spell_checker_get_language(mmguiapp->window->newsmsspellchecker);
		langname = gtk_spell_checker_decode_language_code(langcode);
		gtk_tool_button_set_label(GTK_TOOL_BUTTON(mmguiapp->window->newsmsspellchecktb), langname);
		g_free(langname);
	}
}

static void mmgui_main_sms_spellcheck_menu_activate_signal(GtkMenuItem *menuitem, gpointer data)
{
	mmgui_application_data_t appdata;
	gchar *langname;
		
	appdata = (mmgui_application_data_t)data;
	
	if (appdata == NULL) return;
	
	if (gtk_spell_checker_get_from_text_view(GTK_TEXT_VIEW(appdata->mmguiapp->window->smstextview)) == NULL) {
		/*First attach spell checker if disabled*/
		gtk_spell_checker_attach(appdata->mmguiapp->window->newsmsspellchecker, GTK_TEXT_VIEW(appdata->mmguiapp->window->smstextview));
		g_object_unref(appdata->mmguiapp->window->newsmsspellchecker);
		gmm_settings_set_boolean(appdata->mmguiapp->settings, "spell_checker_enabled", TRUE);
	}
	
	/*Then set language*/
	langname = gtk_spell_checker_decode_language_code((const gchar *)appdata->data);
	gtk_spell_checker_set_language(appdata->mmguiapp->window->newsmsspellchecker, appdata->data, NULL);
	gmm_settings_set_string(appdata->mmguiapp->settings, "spell_checker_language", appdata->data);
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(appdata->mmguiapp->window->newsmsspellchecktb), langname);
	g_free(langname);
}

gboolean mmgui_main_sms_spellcheck_init(mmgui_application_t mmguiapp)
{
	GList *languages, *iterator;
	mmgui_application_data_t appdata;
	gint i, localenum;
	gchar *locale, *locpart, *userlang, *langname, *langcode;
	GtkWidget *langmenuentry;
	gboolean langset;
	
	/*Create spell checker*/
	mmguiapp->window->newsmsspellchecker = gtk_spell_checker_new();
	/*Fill combo box with languages*/
	languages = gtk_spell_checker_get_language_list();
	if (languages != NULL) {
		/*Create menu widget*/
		mmguiapp->window->newsmslangmenu = gtk_menu_new();
		/*Get current locale*/
		locale = getenv("LANG");
		if (locale != NULL) {
			locale = g_strdup(locale);
			locpart = strchr(locale, '.');
			/*Drop encoding part if any*/
			if (locpart != NULL) {
				locpart[0] = '\0';
			}
		} else {
			locale = g_strdup("en_US");
		}
		/*Get selected language*/
		userlang = gmm_settings_get_string(mmguiapp->settings, "spell_checker_language", locale);
		langset = FALSE;
		localenum = -1;
		i = 0;
		for (iterator = languages; iterator != NULL; iterator = iterator->next) {
			/*Get language code and name*/
			langcode = (gchar *)iterator->data;
			langname = gtk_spell_checker_decode_language_code((const gchar *)langcode);
			/*Create menu entry*/
			langmenuentry = gtk_menu_item_new_with_label(langname);
			gtk_menu_shell_append(GTK_MENU_SHELL(mmguiapp->window->newsmslangmenu), langmenuentry);
			/*Set signal*/
			appdata = g_new(struct _mmgui_application_data, 1);
			appdata->mmguiapp = mmguiapp;
			appdata->data = g_strdup(langcode);
			g_signal_connect(G_OBJECT(langmenuentry), "activate", G_CALLBACK(mmgui_main_sms_spellcheck_menu_activate_signal), appdata);
			mmguiapp->window->newsmscodes = g_slist_prepend(mmguiapp->window->newsmscodes, appdata);
			/*Set default language*/
			if (g_str_equal(langcode, userlang)) {
				gtk_spell_checker_set_language(mmguiapp->window->newsmsspellchecker, langcode, NULL);
				if (gmm_settings_get_boolean(mmguiapp->settings, "spell_checker_enabled", TRUE)) {
					gtk_tool_button_set_label(GTK_TOOL_BUTTON(mmguiapp->window->newsmsspellchecktb), langname);
				}
				gtk_widget_set_sensitive(mmguiapp->window->newsmsspellchecktb, TRUE);
				langset = TRUE;
			}
			/*Try to get locale-specific number*/
			if ((!langset) && (localenum == -1)) {
				if (g_str_equal(langcode, locale)) {
					localenum = i;
				}
			}
			/*Free language name*/
			g_free(langname);
			i++;
		}
		/*Set first language or locale*/
		if (!langset) {
			if (localenum != -1) {
				langcode = (gchar *)g_list_nth_data(languages, localenum);
			} else {
				langcode = (gchar *)g_list_nth_data(languages, 0);
			}
			langname = gtk_spell_checker_decode_language_code((const gchar *)langcode);
			gtk_spell_checker_set_language(mmguiapp->window->newsmsspellchecker, langcode, NULL);
			if (gmm_settings_get_boolean(mmguiapp->settings, "spell_checker_enabled", TRUE)) {
				gtk_tool_button_set_label(GTK_TOOL_BUTTON(mmguiapp->window->newsmsspellchecktb), langname);
			}
			gtk_widget_set_sensitive(mmguiapp->window->newsmsspellchecktb, TRUE);
			g_free(langname);
		}
		/*Show menu*/
		gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(mmguiapp->window->newsmsspellchecktb), mmguiapp->window->newsmslangmenu);
		gtk_widget_show_all(mmguiapp->window->newsmslangmenu);
		/*Activate spell checker*/
		gtk_spell_checker_attach(mmguiapp->window->newsmsspellchecker, GTK_TEXT_VIEW(mmguiapp->window->smstextview));
		if (!gmm_settings_get_boolean(mmguiapp->settings, "spell_checker_enabled", TRUE)) {
			/*Attach and detach spell checker*/
			g_object_ref(mmguiapp->window->newsmsspellchecker);
			gtk_spell_checker_detach(mmguiapp->window->newsmsspellchecker);
		}
		/*Free resources*/
		g_list_foreach(languages, (GFunc)g_free, NULL);
		g_list_free(languages);
		g_free(userlang);
		g_free(locale);
	}
	
	return TRUE;
}
#else
void mmgui_main_sms_new_disable_spellchecker_signal(GtkToolButton *toolbutton, gpointer data)
{
	
}
#endif

void mmgui_main_sms_list_init(mmgui_application_t mmguiapp)
{
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkTreeStore *store;
	GtkTextBuffer *textbuffer;
			
	if (mmguiapp == NULL) return;
	
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("SMS"));
	
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, renderer, "pixbuf", MMGUI_MAIN_SMSLIST_ICON, NULL);
	
	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, "ellipsize-set", TRUE, NULL);
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer, "markup", MMGUI_MAIN_SMSLIST_SMS, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->smslist), column);
	
	store = gtk_tree_store_new(MMGUI_MAIN_SMSLIST_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_ULONG, G_TYPE_UINT, G_TYPE_BOOLEAN);
	gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->smslist), GTK_TREE_MODEL(store));
	g_object_unref(store);
	
	/*Create textview formatting tags*/
	textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mmguiapp->window->smstext));
	
	mmguiapp->window->smsheadingtag = gtk_text_buffer_create_tag(textbuffer, "smsheading", "weight", PANGO_WEIGHT_BOLD, "size", 15 * PANGO_SCALE, NULL);
	mmguiapp->window->smsdatetag = gtk_text_buffer_create_tag(textbuffer, "xx-small", "scale", PANGO_SCALE_XX_SMALL, NULL);
	
	/*Open message signal*/
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->smslist));
	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(mmgui_main_sms_list_selection_changed_signal), mmguiapp);
	
	/*Answer message signal*/
	g_signal_connect(G_OBJECT(mmguiapp->window->smslist), "row-activated", G_CALLBACK(mmgui_main_sms_list_row_activated_signal), mmguiapp);
	
	/*New SMS entry autocompletion*/
	mmguiapp->window->smscompletion = gtk_entry_completion_new();
	/*Search for names*/
	gtk_entry_completion_set_text_column(mmguiapp->window->smscompletion, MMGUI_MAIN_SMS_COMPLETION_NAME);
	/*Attach to entry widget*/
	gtk_entry_set_completion(GTK_ENTRY(mmguiapp->window->smsnumberentry), mmguiapp->window->smscompletion);
	/*Create model for contacts from addressbooks*/
	mmguiapp->window->smscompletionmodel = gtk_list_store_new(MMGUI_MAIN_SMS_COMPLETION_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
	gtk_entry_completion_set_model(mmguiapp->window->smscompletion, GTK_TREE_MODEL(mmguiapp->window->smscompletionmodel));
	/*Select autocompletion entry*/
	g_signal_connect(G_OBJECT(mmguiapp->window->smscompletion), "match-selected", G_CALLBACK(mmgui_main_sms_autocompletion_select_entry_signal), mmguiapp); 
	/*Dropdown list*/
	mmguiapp->window->smsnumlistgnomepath = NULL;
	mmguiapp->window->smsnumlistkdepath = NULL;
	mmguiapp->window->smsnumlistmodempath = NULL;
	/*Dropdown SMS numbers list model*/
	mmguiapp->window->smsnumlistmodel = gtk_tree_store_new(MMGUI_MAIN_SMS_LIST_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
	/*Cell renderer*/
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(mmguiapp->window->smsnumbercombo), renderer, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(mmguiapp->window->smsnumbercombo), renderer, "markup", 0, NULL);
	/*Functions for separator and unsensible entries*/
	gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(mmguiapp->window->smsnumbercombo), renderer, mmgui_main_sms_menu_data_func, mmguiapp, NULL);
	gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(mmguiapp->window->smsnumbercombo), mmgui_main_sms_menu_separator_func, mmguiapp, NULL);
	/*Set combo box model*/
	gtk_combo_box_set_model(GTK_COMBO_BOX(mmguiapp->window->smsnumbercombo), GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel));
	/*Select dropdown entry signal*/
	g_signal_connect(G_OBJECT(mmguiapp->window->smsnumbercombo), "format-entry-text", G_CALLBACK(mmgui_main_sms_list_select_entry_signal), mmguiapp); 
}

void mmgui_main_sms_load_contacts_from_system_addressbooks(mmgui_application_t mmguiapp)
{
	/*Autocompletion*/
	mmgui_main_sms_autocompletion_model_fill(mmguiapp, MMGUI_MAIN_CONTACT_GNOME);
	mmgui_main_sms_autocompletion_model_fill(mmguiapp, MMGUI_MAIN_CONTACT_KDE);
	/*Dropdown list*/
	mmgui_main_sms_menu_model_fill(mmguiapp, MMGUI_MAIN_SMS_SOURCE_GNOME);
	mmgui_main_sms_menu_model_fill(mmguiapp, MMGUI_MAIN_SMS_SOURCE_KDE);
}

void mmgui_main_sms_list_clear(mmgui_application_t mmguiapp)
{
	GtkTreeModel *model;
	GtkTextBuffer *buffer;
	GtkTextIter siter, eiter;
	GtkTreeIter iter, catiter;
	GtkTreePath *refpath;
	GtkTreeRowReference *reference;
	GSList *reflist, *iterator;
	gboolean validcont, validcat;
	guint contactsource;
	gchar *pathstr;
	
	if (mmguiapp == NULL) return;
	
	/*Clear SMS list*/
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->smslist));
	if (model != NULL) {
		gtk_tree_store_clear(GTK_TREE_STORE(model));
	}
	
	/*Clear SMS text field*/
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mmguiapp->window->smstext));
	if (buffer != NULL) {
		gtk_text_buffer_get_bounds(buffer, &siter, &eiter);
		gtk_text_buffer_delete(buffer, &siter, &eiter);
	}
	
	/*Set sensitivity of SMS control buttons*/
	gtk_widget_set_sensitive(mmguiapp->window->removesmsbutton, FALSE);
	gtk_widget_set_sensitive(mmguiapp->window->answersmsbutton, FALSE);
	
	/*Remove modem contacts from autocompletion model*/
	if (mmguiapp->window->smscompletionmodel != NULL) {
		reflist = NULL;
		/*Iterate through model and save references*/
		
		validcont = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(mmguiapp->window->smscompletionmodel), &iter);
		while (validcont) {
			gtk_tree_model_get(GTK_TREE_MODEL(mmguiapp->window->smscompletionmodel), &iter, MMGUI_MAIN_SMS_COMPLETION_SOURCE, &contactsource, -1);
			/*Save references only on modem contacts*/
			if (contactsource == MMGUI_MAIN_SMS_COMPLETION_SOURCE) {
				refpath = gtk_tree_model_get_path(GTK_TREE_MODEL(mmguiapp->window->smscompletionmodel), &iter);
				if (refpath != NULL) {
					reference = gtk_tree_row_reference_new(GTK_TREE_MODEL(mmguiapp->window->smscompletionmodel), refpath);
					if (reference != NULL) {
						reflist = g_slist_prepend(reflist, reference);
					}
				}
			}
			validcont = gtk_tree_model_iter_next(GTK_TREE_MODEL(mmguiapp->window->smscompletionmodel), &iter);
		}
		/*Remove contacts*/
		if (reflist != NULL) {
			for (iterator = reflist;  iterator != NULL;  iterator = iterator->next) {
				refpath = gtk_tree_row_reference_get_path((GtkTreeRowReference *)iterator->data);
				if (refpath) {
					if (gtk_tree_model_get_iter(GTK_TREE_MODEL(mmguiapp->window->smscompletionmodel), &iter, refpath)) {
						gtk_list_store_remove(GTK_LIST_STORE(mmguiapp->window->smscompletionmodel), &iter);
					}
				}
			}
			/*Clear resources allocated for references*/
			g_slist_foreach(reflist, (GFunc)gtk_tree_row_reference_free, NULL);
			g_slist_free(reflist);
		}
	}
	
	/*Clear contacts from dropdown list*/
	if ((mmguiapp->window->smsnumlistmodempath != NULL) && (mmguiapp->window->smsnumlistmodel != NULL)) {
		pathstr = gtk_tree_path_to_string(mmguiapp->window->smsnumlistmodempath);
		if (pathstr != NULL) {
			reflist = NULL;
			/*Iterate through model and save references*/
			validcat = gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &catiter, pathstr);
			while (validcat) {
				if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &catiter)) {
					validcont = gtk_tree_model_iter_children(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter, &catiter);
					while (validcont) {
						gtk_tree_model_get(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter, MMGUI_MAIN_SMS_LIST_SOURCE, &contactsource, -1);
						/*Save references only on modem contacts*/
						if (contactsource == MMGUI_MAIN_SMS_SOURCE_MODEM) {
							refpath = gtk_tree_model_get_path(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter);
							if (refpath != NULL) {
								reference = gtk_tree_row_reference_new(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), refpath);
								if (reference != NULL) {
									reflist = g_slist_prepend(reflist, reference);
								}
							}
						}
						validcont = gtk_tree_model_iter_next(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter);
					}
				}
				
				validcat = gtk_tree_model_iter_next(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &catiter);
			}
			/*Remove contacts*/
			if (reflist != NULL) {
				for (iterator = reflist; iterator != NULL; iterator = iterator->next) {
					refpath = gtk_tree_row_reference_get_path((GtkTreeRowReference *)iterator->data);
					if (refpath) {
						if (gtk_tree_model_get_iter(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter, refpath)) {
							gtk_tree_store_remove(GTK_TREE_STORE(mmguiapp->window->smsnumlistmodel), &iter);
						}
					}
				}
				/*Clear resources allocated for references*/
				g_slist_foreach(reflist, (GFunc)gtk_tree_row_reference_free, NULL);
				g_slist_free(reflist);
			}
			g_free(pathstr);
		}
	}
	
	/*Clear history contacts from dropdown list*/
	if (mmguiapp->window->smsnumlistmodel != NULL) {
		reflist = NULL;
		/*Iterate through model and save references*/
		validcat = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter);
		while (validcat) {
			gtk_tree_model_get(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter, MMGUI_MAIN_SMS_LIST_SOURCE, &contactsource, -1);
			if (contactsource == MMGUI_MAIN_SMS_SOURCE_HISTORY) {
				refpath = gtk_tree_model_get_path(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter);
				if (refpath != NULL) {
					reference = gtk_tree_row_reference_new(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), refpath);
					if (reference != NULL) {
						reflist = g_slist_prepend(reflist, reference);
					}
				}
			}
			validcat = gtk_tree_model_iter_next(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter);
		}
		
		/*Remove contacts*/
		if (reflist != NULL) {
			for (iterator = reflist; iterator != NULL; iterator = iterator->next) {
				refpath = gtk_tree_row_reference_get_path((GtkTreeRowReference *)iterator->data);
				if (refpath) {
					if (gtk_tree_model_get_iter(GTK_TREE_MODEL(mmguiapp->window->smsnumlistmodel), &iter, refpath)) {
						gtk_tree_store_remove(GTK_TREE_STORE(mmguiapp->window->smsnumlistmodel), &iter);
					}
				}
			}
			/*Clear resources allocated for references*/
			g_slist_foreach(reflist, (GFunc)gtk_tree_row_reference_free, NULL);
			g_slist_free(reflist);
		}
	}
	
	/*Free history list*/
	if (mmguiapp->window->smsnumlisthistory != NULL) {
		g_slist_foreach(mmguiapp->window->smsnumlisthistory, (GFunc)g_free, NULL);
		g_slist_free(mmguiapp->window->smsnumlisthistory);
	}
}
