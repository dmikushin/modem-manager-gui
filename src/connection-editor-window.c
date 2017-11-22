/*
 *      connection-editor-window.c
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <langinfo.h>

#include <gtk/gtk.h>
#include <locale.h>
#include <libintl.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gprintf.h>

#include "devices-page.h"
#include "connection-editor-window.h"


void mmgui_main_connection_editor_apn_insert_text_handler(GtkEntry *entry, const gchar *text, gint length, gint *position, gpointer data)
{
	GtkEditable *editable;
	gint exlen, i, count;
	gchar *extext, *result;
	
	/*Text that already exists*/
	extext = (gchar *)gtk_entry_get_text(entry);
	exlen = gtk_entry_get_text_length(entry);
	
	/*Inserted text*/
	result = g_new(gchar, length);
	count = 0;
	
	for (i = 0; i < length; i++) {
		if ((!isalnum(text[i])) && ((text[i] != '.') || ((text[i] == '.') && ((extext[0] == '.') || (extext[exlen-1] == '.'))))) {
			continue;
		}
		result[count++] = text[i];
	}
	
	editable = GTK_EDITABLE(entry);
	
	if (count > 0) {
		g_signal_handlers_block_by_func(G_OBJECT(editable), G_CALLBACK(mmgui_main_connection_editor_apn_insert_text_handler), data);
		gtk_editable_insert_text(editable, result, count, position);
		g_signal_handlers_unblock_by_func(G_OBJECT(editable), G_CALLBACK(mmgui_main_connection_editor_apn_insert_text_handler), data);
	}
	
	g_signal_stop_emission_by_name(G_OBJECT(editable), "insert_text");
	
	g_free(result);
}

void mmgui_main_connection_editor_id_insert_text_handler(GtkEntry *entry, const gchar *text, gint length, gint *position, gpointer data)
{
	GtkEditable *editable;
	gint i, count;
	gchar *result;
	
	/*Inserted text*/
	result = g_new(gchar, length);
	count = 0;
	
	for (i = 0; i < length; i++) {
		if (!isdigit(text[i])) {
			continue;
		}
		result[count++] = text[i];
	}
	
	editable = GTK_EDITABLE(entry);
	
	if (count > 0) {
		g_signal_handlers_block_by_func(G_OBJECT(editable), G_CALLBACK(mmgui_main_connection_editor_id_insert_text_handler), data);
		gtk_editable_insert_text(editable, result, count, position);
		g_signal_handlers_unblock_by_func(G_OBJECT(editable), G_CALLBACK(mmgui_main_connection_editor_id_insert_text_handler), data);
	}
	
	g_signal_stop_emission_by_name(G_OBJECT(editable), "insert_text");
	
	g_free(result);
}

void mmgui_main_connection_editor_number_insert_text_handler(GtkEntry *entry, const gchar *text, gint length, gint *position, gpointer data)
{
	GtkEditable *editable;
	gint i, count;
	gchar *result;
	
	/*Inserted text*/
	result = g_new(gchar, length);
	count = 0;
	
	for (i = 0; i < length; i++) {
		if ((!isdigit(text[i])) && (text[i] != '*') && (text[i] != '#')) {
			continue;
		}
		result[count++] = text[i];
	}
	
	editable = GTK_EDITABLE(entry);
	
	if (count > 0) {
		g_signal_handlers_block_by_func(G_OBJECT(editable), G_CALLBACK(mmgui_main_connection_editor_number_insert_text_handler), data);
		gtk_editable_insert_text(editable, result, count, position);
		g_signal_handlers_unblock_by_func(G_OBJECT(editable), G_CALLBACK(mmgui_main_connection_editor_number_insert_text_handler), data);
	}
	
	g_signal_stop_emission_by_name(G_OBJECT(editable), "insert_text");
	
	g_free(result);
}

void mmgui_main_connection_editor_credentials_insert_text_handler(GtkEntry *entry, const gchar *text, gint length, gint *position, gpointer data)
{
	GtkEditable *editable;
	gint i, count;
	gchar *result;
	
	/*Inserted text*/
	result = g_new(gchar, length);
	count = 0;
	
	for (i = 0; i < length; i++) {
		if (!isalnum(text[i])) {
			continue;
		}
		result[count++] = text[i];
	}
	
	editable = GTK_EDITABLE(entry);
	
	if (count > 0) {
		g_signal_handlers_block_by_func(G_OBJECT(editable), G_CALLBACK(mmgui_main_connection_editor_credentials_insert_text_handler), data);
		gtk_editable_insert_text(editable, result, count, position);
		g_signal_handlers_unblock_by_func(G_OBJECT(editable), G_CALLBACK(mmgui_main_connection_editor_credentials_insert_text_handler), data);
	}
	
	g_signal_stop_emission_by_name(G_OBJECT(editable), "insert_text");
	
	g_free(result);
}

void mmgui_main_connection_editor_dns_insert_text_handler(GtkEntry *entry, const gchar *text, gint length, gint *position, gpointer data)
{
	GtkEditable *editable;
	gint exlen, i, count;
	gchar *extext, *result;
	
	/*Text that already exists*/
	extext = (gchar *)gtk_entry_get_text(entry);
	exlen = gtk_entry_get_text_length(entry);
	
	/*Inserted text*/
	result = g_new(gchar, length);
	count = 0;
	
	for (i = 0; i < length; i++) {
		if ((!isdigit(text[i])) && ((text[i] != '.') || ((text[i] == '.') && ((extext[0] == '.') || (extext[exlen-1] == '.'))))) {
			continue;
		}
		result[count++] = text[i];
	}
	
	editable = GTK_EDITABLE(entry);
	
	if (count > 0) {
		g_signal_handlers_block_by_func(G_OBJECT(editable), G_CALLBACK(mmgui_main_connection_editor_dns_insert_text_handler), data);
		gtk_editable_insert_text(editable, result, count, position);
		g_signal_handlers_unblock_by_func(G_OBJECT(editable), G_CALLBACK(mmgui_main_connection_editor_dns_insert_text_handler), data);
	}
	
	g_signal_stop_emission_by_name(G_OBJECT(editable), "insert_text");
	
	g_free(result);
}

static gboolean mmgui_main_connection_editor_validate_apn(const gchar *apn, gsize len)
{
	gboolean result;
	
	if ((apn == NULL) || (len == 0) || (len > 63)) return FALSE;
	
	result = TRUE;
	
	/*APN must not be started and ended with '.' and must not contain '*'*/
	if ((apn[0] == '.') || (apn[len-1] == '.') || (strchr(apn, '*') != NULL)) {
		result = FALSE;
	}
	/*APN must not be started with words 'rnc', 'rac', 'lac' and 'sgsn'*/
	if ((result) && (len > 3)) {
		if ((strncmp(apn, "rnc", 3) == 0) || (strncmp(apn, "rac", 3) == 0) || (strncmp(apn, "lac", 3) == 0) || (strncmp(apn, "sgsn", 4) == 0)) {
			result = FALSE;
		}
	}
	/*APN must not be ended with word '.gprs'*/
	if ((result) && (len > 5)) {
		if (strncmp(apn + len - 5, ".gprs", 5) == 0) {
			result = FALSE;
		}
	}
	
	return result;
}

static gboolean mmgui_main_connection_editor_validate_ip(const gchar *ip, gsize len)
{
	GSocketAddress *address;
	gboolean result;
	
	if ((ip == NULL) || (len == 0)) return FALSE;
	
	result = FALSE;
	
	/*Address validated with Glib function*/
	address = g_inet_socket_address_new_from_string(ip, 1);
	if (address != NULL) {
		result = TRUE;
		g_object_unref(address);
	}
	
	return result;
}

static void mmgui_main_connection_editor_handle_simple_parameter_change(mmgui_application_t mmguiapp, GtkEntry *entry, guint columnid)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	gboolean new, changed, removed;
	gchar *text, *name, *caption;
	
	if ((mmguiapp == NULL) || (entry == NULL)) return;
	
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	
	if (selection != NULL) {
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_tree_model_get(model, &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, &name,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, &new,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, &changed,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, &removed,
											-1);
			if (!removed) {
				/*Get current parameter value*/
				text = (gchar *)gtk_entry_get_text(entry);
				/*Update parameter value*/
				if ((new) || (changed)) {
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, columnid, text,
																	-1);
				} else {
					caption = g_strdup_printf("<i>%s</i>", name);
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, caption,
																	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, TRUE,
																	columnid, text,
																	-1);
					g_free(caption);
				}
			}
			/*Free resources*/
			if (name != NULL) {
				g_free(name);
			}		
		}
	}
}

static void mmgui_main_connection_editor_handle_validated_parameter_change(mmgui_application_t mmguiapp, GtkEntry *entry, guint columnid, const gchar *paramname, gboolean (*validator)(const gchar *parameter, gsize len))
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	gboolean new, changed, removed;
	gint len;
	gchar *text, *name, *caption, *message;
	
	if ((mmguiapp == NULL) || (entry == NULL) || (validator == NULL)) return;
	
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	
	if (selection != NULL) {
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_tree_model_get(model, &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, &name,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, &new,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, &changed,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, &removed,
											-1);
			if (!removed) {
				text = (gchar *)gtk_entry_get_text(entry);
				len = gtk_entry_get_text_length(entry);
				
				if (len > 0) {
					if (validator((const gchar *)text, len)) {
						/*Valid parameter*/
						if ((new) || (changed)) {
							gtk_list_store_set(GTK_LIST_STORE(model), &iter, columnid, text,
																			-1);
						} else {
							caption = g_strdup_printf("<i>%s</i>", name);
							gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, caption,
																			MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, TRUE,
																			columnid, text,
																			-1);
							g_free(caption);
						}
						/*Hide warning*/
						#if GTK_CHECK_VERSION(3,10,0)
							gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, NULL);
						#else
							gtk_entry_set_icon_from_stock(entry, GTK_ENTRY_ICON_SECONDARY, NULL);
						#endif
						gtk_entry_set_icon_tooltip_markup(entry, GTK_ENTRY_ICON_SECONDARY, NULL);
					} else {
						/*Show warning*/
						#if GTK_CHECK_VERSION(3,10,0)
							gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, "dialog-warning");
						#else
							gtk_entry_set_icon_from_stock(entry, GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CAPS_LOCK_WARNING);
						#endif
						message = g_strdup_printf(_("<b>%s is not valid</b>\n<small>It won't be saved and used on connection initialization</small>"), paramname);
						gtk_entry_set_icon_tooltip_markup(entry, GTK_ENTRY_ICON_SECONDARY, message);
						g_free(message);
					}
				} else {
					/*Empty IP address*/
					if ((new) || (changed)) {
						gtk_list_store_set(GTK_LIST_STORE(model), &iter, columnid, text,
																		-1);
					} else {
						caption = g_strdup_printf("<i>%s</i>", name);
						gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, caption,
																		MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, TRUE,
																		columnid, text,
																		-1);
						g_free(caption);
					}
					/*Pretend it is valid*/
					#if GTK_CHECK_VERSION(3,10,0)
						gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, NULL);
					#else
						gtk_entry_set_icon_from_stock(entry, GTK_ENTRY_ICON_SECONDARY, NULL);
					#endif
					gtk_entry_set_icon_tooltip_markup(entry, GTK_ENTRY_ICON_SECONDARY, NULL);
				}
				/*Free resources*/
				if (name != NULL) {
					g_free(name);
				}
			}
		}
	}
}

void mmgui_main_connection_editor_name_changed_signal(GtkEditable *editable, gpointer data)
{
	mmgui_application_t mmguiapp;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	gboolean new, changed, removed;
	gint len;
	gchar *text, *name, *caption;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	
	if (selection != NULL) {
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_tree_model_get(model, &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, &name,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, &new,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, &changed,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, &removed,
											-1);
			if (!removed) {
				text = (gchar *)gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->connnameentry));
				len = gtk_entry_get_text_length(GTK_ENTRY(mmguiapp->window->connnameentry));
				if (new) {
					if (len > 0) {
						caption = g_strdup_printf("<b>%s</b>", text);
					} else {
						caption = g_strdup_printf("<b>%s</b>", _("Unnamed connection"));
						text = NULL;
					}
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, caption,
																	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, text,
																	-1);
				} else {
					if (len > 0) {
						caption = g_strdup_printf("<i>%s</i>", text);
					} else {
						caption = g_strdup_printf("<i>%s</i>", _("Unnamed connection"));
						text = NULL;
					}
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, caption,
																	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, text,
																	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, TRUE,
																	-1);
				}
				
				if (caption != NULL) {
					g_free(caption);
				}
				if (name != NULL) {
					g_free(name);
				}
			}
		}
	}
}

void mmgui_main_connection_editor_apn_changed_signal(GtkEditable *editable, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_connection_editor_handle_validated_parameter_change(mmguiapp, GTK_ENTRY(mmguiapp->window->connnameapnentry), MMGUI_CONNECTION_EDITOR_WINDOW_LIST_APN, _("APN name"), mmgui_main_connection_editor_validate_apn);
}

void mmgui_main_connection_editor_home_only_toggled_signal(GtkToggleButton *togglebutton, gpointer data)
{
	mmgui_application_t mmguiapp;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	gboolean new, changed, removed;
	gboolean value;
	gchar *name, *caption;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	
	if (selection != NULL) {
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_tree_model_get(model, &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, &name,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, &new,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, &changed,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, &removed,
											-1);
			if (!removed) {
				/*Get current parameter value*/
				value = gtk_toggle_button_get_active(togglebutton);
				/*Update parameter value*/
				if ((new) || (changed)) {
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_HOME_ONLY, !value,
																	-1);
				} else {
					caption = g_strdup_printf("<i>%s</i>", name);
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, caption,
																	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, TRUE,
																	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_HOME_ONLY, !value,
																	-1);
					g_free(caption);
				}
			}
			/*Free resources*/
			if (name != NULL) {
				g_free(name);
			}
		}
	}
}

void mmgui_main_connection_editor_network_id_changed_signal(GtkEditable *editable, gpointer data)
{
	mmgui_application_t mmguiapp;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	gboolean new, changed, removed;
	guint value;
	gchar *name, *caption;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	
	if (selection != NULL) {
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_tree_model_get(model, &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, &name,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, &new,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, &changed,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, &removed,
											-1);
			
			if (!removed) {
				/*Get current parameter value*/
				value = (guint)atoi(gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->connnetidspinbutton)));
				/*Update parameter value*/
				if ((new) || (changed)) {
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NETWORK_ID, value,
																	-1);
				} else {
					caption = g_strdup_printf("<i>%s</i>", name);
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, caption,
																	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, TRUE,
																	MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NETWORK_ID, value,
																	-1);
					g_free(caption);
				}
			}
			/*Free resources*/
			if (name != NULL) {
				g_free(name);
			}
		}
	}
}

void mmgui_main_connection_editor_number_changed_signal(GtkEditable *editable, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_connection_editor_handle_simple_parameter_change(mmguiapp, GTK_ENTRY(mmguiapp->window->connauthnumberentry), MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NUMBER);
}

void mmgui_main_connection_editor_username_changed_signal(GtkEditable *editable, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_connection_editor_handle_simple_parameter_change(mmguiapp, GTK_ENTRY(mmguiapp->window->connauthusernameentry), MMGUI_CONNECTION_EDITOR_WINDOW_LIST_USERNAME);
}

void mmgui_main_connection_editor_password_changed_signal(GtkEditable *editable, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_connection_editor_handle_simple_parameter_change(mmguiapp, GTK_ENTRY(mmguiapp->window->connauthpassentry), MMGUI_CONNECTION_EDITOR_WINDOW_LIST_PASSWORD);
}

void mmgui_main_connection_editor_dns1_changed_signal(GtkEditable *editable, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_connection_editor_handle_validated_parameter_change(mmguiapp, GTK_ENTRY(mmguiapp->window->conndns1entry), MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS1, _("First DNS server IP address"), mmgui_main_connection_editor_validate_ip);
}

void mmgui_main_connection_editor_dns2_changed_signal(GtkEditable *editable, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_connection_editor_handle_validated_parameter_change(mmguiapp, GTK_ENTRY(mmguiapp->window->conndns2entry), MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS2, _("Second DNS server IP address"), mmgui_main_connection_editor_validate_ip);
}

void mmgui_main_connection_editor_add_button_clicked_signal(GtkToolButton *toolbutton, gpointer data)
{
	mmgui_application_t mmguiapp;
	mmgui_providers_db_entry_t curentry, recentry;
	GSList *providers, *piterator;
	gchar *caption;
	gint networkid, mcc, mnc, mul;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	guint type;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	
	if (mmguiapp->core->device != NULL) {
		type = mmguiapp->core->device->type;
	} else {
		type = MMGUI_DEVICE_TYPE_GSM;
	}
	
	if (model != NULL) {
		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		if (type == MMGUI_DEVICE_TYPE_GSM) {
			/*Find recommended provider database entry*/
			recentry = NULL;
			if (mmguicore_devices_get_registered(mmguiapp->core)) {
				providers = mmgui_providers_get_list(mmguiapp->providersdb);
				if (providers != NULL) {
					/*Convert operator code to compatible network ID*/
					mcc = (mmguiapp->core->device->operatorcode & 0xffff0000) >> 16;
					mnc = mmguiapp->core->device->operatorcode & 0x0000ffff;
					mul = 1;
					while (mul <= mnc) {
						mul *= 10;
					}
					networkid = mcc * mul + mnc;
					/*Find provider with same network ID*/
					for (piterator = providers; piterator != NULL; piterator = piterator->next) {
						curentry = (mmgui_providers_db_entry_t)piterator->data;
						if ((curentry->tech == MMGUI_DEVICE_TYPE_GSM) && (curentry->usage == MMGUI_PROVIDERS_DB_ENTRY_USAGE_INTERNET)) {
							if (mmgui_providers_provider_get_network_id(curentry) == networkid) {
								recentry = curentry;
								break;
							}
						}
					}
				}
			}
			/*Add connection*/
			if (recentry != NULL) {
				caption = g_strdup_printf("<b>%s</b>", recentry->name);
				gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, caption,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, recentry->name, 
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_UUID, NULL,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NUMBER, "*99#",
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_USERNAME, recentry->username,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_PASSWORD, recentry->password,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_APN, recentry->apn,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NETWORK_ID, mmgui_providers_provider_get_network_id(recentry),
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_TYPE, recentry->tech,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_HOME_ONLY, FALSE,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS1, recentry->dns1,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS2, recentry->dns2,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, TRUE,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, FALSE,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, FALSE,
																-1);
				g_free(caption);
			} else {
				gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, "<b>New connection</b>",
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, "New connection", 
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_UUID, NULL,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NUMBER, "*99#",
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_USERNAME, "internet",
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_PASSWORD, "internet",
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_APN, "internet",
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NETWORK_ID, 10000,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_TYPE, type,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_HOME_ONLY, FALSE,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS1, NULL,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS2, NULL,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, TRUE,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, FALSE,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, FALSE,
																-1);
			}
		} else if (type == MMGUI_DEVICE_TYPE_CDMA) {
			gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, "<b>New connection</b>",
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, "New connection", 
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_UUID, NULL,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NUMBER, "*777#",
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_USERNAME, "internet",
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_PASSWORD, "internet",
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_TYPE, type,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS1, NULL,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS2, NULL,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, TRUE,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, FALSE,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, FALSE,
															-1);
		}
		/*Select it*/
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->contreeview));
		gtk_tree_selection_select_iter(selection, &iter);
		g_signal_emit_by_name(G_OBJECT(mmguiapp->window->contreeview), "cursor-changed", NULL);
	}
	
}
               
void mmgui_main_connection_editor_remove_button_clicked_signal(GtkToolButton *toolbutton, gpointer data)
{
	mmgui_application_t mmguiapp;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *name, *caption;
	gboolean removed;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	if ((selection != NULL) && (model != NULL)) {
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_tree_model_get(model, &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, &name,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, &removed,
											-1);
			if (!removed) {
				/*Update caption*/
				if (name != NULL) {
					caption = g_strdup_printf("<s>%s</s>", name);
					g_free(name);
				} else {
					caption = g_strdup_printf("<s>%s</s>", _("Unnamed connection"));
				}
				gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, caption,
																MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, TRUE,
																-1);
				/*Update control panel*/
				gtk_widget_set_sensitive(mmguiapp->window->connnameentry, FALSE);
				gtk_widget_set_sensitive(mmguiapp->window->connnameapnentry, FALSE);
				gtk_widget_set_sensitive(mmguiapp->window->connnetroamingcheckbutton, FALSE);
				gtk_widget_set_sensitive(mmguiapp->window->connnetidspinbutton, FALSE);
				gtk_widget_set_sensitive(mmguiapp->window->connauthnumberentry, FALSE);
				gtk_widget_set_sensitive(mmguiapp->window->connauthusernameentry, FALSE);
				gtk_widget_set_sensitive(mmguiapp->window->connauthpassentry, FALSE);
				gtk_widget_set_sensitive(mmguiapp->window->conndns1entry, FALSE);
				gtk_widget_set_sensitive(mmguiapp->window->conndns2entry, FALSE);
				gtk_widget_set_sensitive(mmguiapp->window->connremovetoolbutton, FALSE);
				/*Free resources*/
				g_free(caption);
			}
		}
	}
	
}

static void mmgui_main_connection_editor_add_db_entry(GtkMenuItem *menuitem, gpointer data)
{
	mmgui_application_t mmguiapp;
	mmgui_providers_db_entry_t dbentry;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *caption;
	GtkTreeSelection *selection;
	
	mmguiapp = (mmgui_application_t)data;
	dbentry = (mmgui_providers_db_entry_t)g_object_get_data(G_OBJECT(menuitem), "dbentry");
	
	if ((mmguiapp == NULL) || (dbentry == NULL)) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	
	if (model != NULL) {
		/*Create caption*/
		caption = g_strdup_printf("<b>%s</b>", dbentry->name);
		/*Add connection*/
		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		if (dbentry->tech == MMGUI_DEVICE_TYPE_GSM) {
			gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, caption,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, dbentry->name, 
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_UUID, NULL,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NUMBER, "*99#",
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_USERNAME, dbentry->username,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_PASSWORD, dbentry->password,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_APN, dbentry->apn,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NETWORK_ID, mmgui_providers_provider_get_network_id(dbentry),
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_TYPE, dbentry->tech,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_HOME_ONLY, FALSE,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS1, dbentry->dns1,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS2, dbentry->dns2,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, TRUE,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, FALSE,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, FALSE,
															-1);
		} else if (dbentry->tech == MMGUI_DEVICE_TYPE_CDMA) {
			gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, caption,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, dbentry->name, 
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_UUID, NULL,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NUMBER, "*777#",
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_USERNAME, dbentry->username,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_PASSWORD, dbentry->password,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NETWORK_ID, mmgui_providers_provider_get_network_id(dbentry),
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_TYPE, dbentry->tech,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS1, dbentry->dns1,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS2, dbentry->dns2,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, TRUE,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, FALSE,
															MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, FALSE,
															-1);
		}
		/*Select it*/
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->contreeview));
		gtk_tree_selection_select_iter(selection, &iter);
		g_signal_emit_by_name(G_OBJECT(mmguiapp->window->contreeview), "cursor-changed", NULL);
		/*Free resources*/
		g_free(caption);
	}

}

static void mmgui_main_connection_editor_window_forget_changes(mmgui_application_t mmguiapp)
{
	mmguiconn_t connection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *rmlist, *rmiter, *connlist, *conniter;
	GtkTreePath *rmpath;
	GtkTreeRowReference *rmref;
	gboolean new, changed, removed;
	gchar *uuid;
		
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	
	connlist = mmguicore_connections_get_list(mmguiapp->core);
	
	rmlist = NULL;
	
	if (model != NULL) {
		if (gtk_tree_model_get_iter_first(model, &iter)) {
			do {
				/*First get UUID and flags*/
				gtk_tree_model_get(model, &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_UUID, &uuid,
												MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, &new,
												MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, &changed,
												MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, &removed,
												-1);
				
				if (new) {
					/*Add reference to row for deletion*/
					rmpath = gtk_tree_model_get_path(model, &iter);
					rmref = gtk_tree_row_reference_new(model, rmpath);
					rmlist = g_slist_prepend(rmlist, rmref);
					gtk_tree_path_free(rmpath);
				} else if ((changed) || (removed)) {
					if ((uuid != NULL) && (connlist != NULL)) {
						for (conniter = connlist; conniter != NULL; conniter = conniter->next) {
							connection = (mmguiconn_t)conniter->data;
							if (g_strcmp0(connection->uuid, uuid) == 0) {
								gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, connection->name,
																				MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, connection->name, 
																				MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NUMBER, connection->number,
																				MMGUI_CONNECTION_EDITOR_WINDOW_LIST_USERNAME, connection->username,
																				MMGUI_CONNECTION_EDITOR_WINDOW_LIST_PASSWORD, connection->password,
																				MMGUI_CONNECTION_EDITOR_WINDOW_LIST_APN, connection->apn,
																				MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NETWORK_ID, connection->networkid,
																				MMGUI_CONNECTION_EDITOR_WINDOW_LIST_HOME_ONLY, connection->homeonly,
																				MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS1, connection->dns1,
																				MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS2, connection->dns2,
																				MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, FALSE,
																				MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, FALSE,
																				MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, FALSE,
																				-1);
								break;
							}
						}
					}
				}
				/*Free UUID*/
				if (uuid != NULL) {
					g_free(uuid);
				}
			} while (gtk_tree_model_iter_next(model, &iter));
		}
		
		/*Cleanup new rows*/
		if (rmlist != NULL) {
			for (rmiter = rmlist;  rmiter != NULL;  rmiter = rmiter->next) {
				rmpath = gtk_tree_row_reference_get_path((GtkTreeRowReference *)rmiter->data);
				if (rmpath != NULL) {
					if (gtk_tree_model_get_iter(model, &iter, rmpath)) {
						gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
					}
				}
			}
			g_slist_foreach(rmlist, (GFunc)gtk_tree_row_reference_free, NULL);
			g_slist_free(rmlist);
		}
	} 
}

static void mmgui_main_connection_editor_window_save_changes(mmgui_application_t mmguiapp)
{
	mmguiconn_t connection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *rmlist, *rmiter;
	GtkTreePath *rmpath;
	GtkTreeRowReference *rmref;
	gboolean new, changed, removed, protected;
	gchar *name, *uuid, *activeuuid, *number, *username, *password, *apn, *dns1, *dns2;
	guint networkid, type;
	gboolean homeonly;
	
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	
	rmlist = NULL;
	
	if (model != NULL) {
		if (gtk_tree_model_get_iter_first(model, &iter)) {
			do {
				/*First get UUID and flags*/
				gtk_tree_model_get(model, &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_UUID, &uuid,
												MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, &new,
												MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, &changed,
												MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, &removed,
												-1);
				/*Protected flag*/
				protected = FALSE;
				if (uuid != NULL) {
					activeuuid = mmguicore_connections_get_active_uuid(mmguiapp->core);
					if (activeuuid != NULL) {
						protected = (g_strcmp0(uuid, activeuuid) == 0);
						g_free(activeuuid);
					}
				}
				/*Available actions*/
				if (new) {
					/*Get connection parameters*/
					gtk_tree_model_get(model, &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, &name,
													MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NUMBER, &number,
													MMGUI_CONNECTION_EDITOR_WINDOW_LIST_USERNAME, &username,
													MMGUI_CONNECTION_EDITOR_WINDOW_LIST_PASSWORD, &password,
													MMGUI_CONNECTION_EDITOR_WINDOW_LIST_APN, &apn,
													MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NETWORK_ID, &networkid,
													MMGUI_CONNECTION_EDITOR_WINDOW_LIST_TYPE, &type,
													MMGUI_CONNECTION_EDITOR_WINDOW_LIST_HOME_ONLY, &homeonly,
													MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS1, &dns1,
													MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS2, &dns2,
													-1);
					if (name == NULL) {
						name = g_strdup(_("Unnamed connection"));
					}
					/*Add new connection (object must be returned)*/
					connection = mmguicore_connections_add(mmguiapp->core, name, number, username, password, apn, networkid, type, homeonly, dns1, dns2);
					if (connection != NULL) {
						/*Add connection to list on devices page*/
						mmgui_main_connection_add_to_list(mmguiapp, connection->name, connection->uuid, connection->type, NULL);
						/*Add UUID, change caption*/
						gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, connection->name,
																		MMGUI_CONNECTION_EDITOR_WINDOW_LIST_UUID, connection->uuid,
																		MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, FALSE,
																		MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, FALSE,
																		MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, FALSE,
																		-1);
					}
					/*Free resources*/
					if (name != NULL) {
						g_free(name);
					}
					if (number != NULL) {
						g_free(number);
					}
					if (username != NULL) {
						g_free(username);
					}
					if (password != NULL) {
						g_free(password);
					}
					if (apn != NULL) {
						g_free(apn);
					}
					if (dns1 != NULL) {
						g_free(dns1);
					}
					if (dns2 != NULL) {
						g_free(dns2);
					}
				} else if (changed) {
					if (!protected) {
						/*Get connection parameters*/
						gtk_tree_model_get(model, &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, &name,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NUMBER, &number,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_USERNAME, &username,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_PASSWORD, &password,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_APN, &apn,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NETWORK_ID, &networkid,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_HOME_ONLY, &homeonly,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS1, &dns1,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS2, &dns2,
														-1);
						if (name == NULL) {
							name = g_strdup(_("Unnamed connection"));
						}
						/*Update connection*/
						mmguicore_connections_update(mmguiapp->core, uuid, name, number, username, password, apn, networkid, homeonly, dns1, dns2);
						/*Update connection name on devices page*/
						mmgui_main_connection_update_name_in_list(mmguiapp, name, uuid);
						/*Change caption and free resources*/
						gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, name,
																		MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, FALSE,
																		MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, FALSE,
																		MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, FALSE,
																		-1);
						if (name != NULL) {
							g_free(name);
						}
						if (number != NULL) {
							g_free(number);
						}
						if (username != NULL) {
							g_free(username);
						}
						if (password != NULL) {
							g_free(password);
						}
						if (apn != NULL) {
							g_free(apn);
						}
						if (dns1 != NULL) {
							g_free(dns1);
						}
						if (dns2 != NULL) {
							g_free(dns2);
						}
					}
				} else if (removed) {
					if (!protected) {
						/*Add reference to row for deletion*/
						rmpath = gtk_tree_model_get_path(model, &iter);
						rmref = gtk_tree_row_reference_new(model, rmpath);
						rmlist = g_slist_prepend(rmlist, rmref);
						gtk_tree_path_free(rmpath);
						/*Delete connection with known UUID*/
						if (uuid != NULL) {
							mmguicore_connections_remove(mmguiapp->core, uuid);
							mmgui_main_connection_remove_from_list(mmguiapp, uuid);
						}
					}					
				}
				/*Free UUID*/
				if (uuid != NULL) {
					g_free(uuid);
				}
			} while (gtk_tree_model_iter_next(model, &iter));
		}
		
		/*Cleanup removed rows*/
		if (rmlist != NULL) {
			for (rmiter = rmlist;  rmiter != NULL;  rmiter = rmiter->next) {
				rmpath = gtk_tree_row_reference_get_path((GtkTreeRowReference *)rmiter->data);
				if (rmpath != NULL) {
					if (gtk_tree_model_get_iter(model, &iter, rmpath)) {
						gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
					}
				}
			}
			g_slist_foreach(rmlist, (GFunc)gtk_tree_row_reference_free, NULL);
			g_slist_free(rmlist);
		}
	} 
}

static gint mmgui_main_connection_editor_provider_menu_compare_entries(gconstpointer a, gconstpointer b)
{
	mmgui_providers_db_entry_t aentry, bentry;
	gchar *acasefold, *bcasefold;
	gint res;
	
	if ((a == NULL) || (b == NULL)) return 0;
	
	aentry = (mmgui_providers_db_entry_t)a;
	bentry = (mmgui_providers_db_entry_t)b;
	
	acasefold = g_utf8_casefold(mmgui_providers_provider_get_country_name(aentry), -1);
	bcasefold = g_utf8_casefold(mmgui_providers_provider_get_country_name(bentry), -1);
	
	res = g_utf8_collate(acasefold, bcasefold);
	
	g_free(acasefold);
	g_free(bcasefold);
	
	return res;
}

void mmgui_main_connection_editor_window_open(mmgui_application_t mmguiapp)
{
	gint response;
	gchar *countryid, *langenv;
	GSList *providers, *piterator, *providerlist;
	GList *countries, *citerator;
	mmgui_providers_db_entry_t dbentry;
	GHashTable *countryhash;
	GtkWidget *submenu;
	GtkWidget *cmenuitem, *pmenuitem;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *curuuid, *uuid;
	
	if (mmguiapp == NULL) return;
	if (mmguiapp->core->device == NULL) return;
	
	if (mmguiapp->window->providersmenu == NULL) {
		/*Fill menu with providers names*/
		providers = mmgui_providers_get_list(mmguiapp->providersdb);
		if (providers != NULL) {
			/*Get current country ID if possible*/
			countryid = NULL;
			langenv = getenv("LANG");
			if (langenv != NULL) {
				countryid = nl_langinfo(_NL_ADDRESS_COUNTRY_AB2);
			}
			/*Create menu*/
			mmguiapp->window->providersmenu = gtk_menu_new();
			/*Sort operators by countries*/
			countryhash = g_hash_table_new(g_str_hash, g_str_equal);
			for (piterator = providers; piterator != NULL; piterator = piterator->next) {
				dbentry = (mmgui_providers_db_entry_t)piterator->data;
				if (dbentry->usage == MMGUI_PROVIDERS_DB_ENTRY_USAGE_INTERNET) {
					providerlist = (GSList *)g_hash_table_lookup(countryhash, dbentry->country);
					providerlist = g_slist_prepend(providerlist, dbentry);
					g_hash_table_insert(countryhash, dbentry->country, providerlist);
				}
			}
			/*Sort country names alphabetically*/
			countries = g_list_sort(g_hash_table_get_keys(countryhash), mmgui_main_connection_editor_provider_menu_compare_entries);
			/*Build providers menu*/
			for (citerator = countries; citerator != NULL; citerator = citerator->next) {
				if (g_ascii_strcasecmp((gchar *)citerator->data, countryid) == 0) {
					providerlist = (GSList *)g_hash_table_lookup(countryhash, (gchar *)citerator->data);
					/*Separator between suggested provider entries and countries submenus*/
					pmenuitem = gtk_separator_menu_item_new();
					gtk_menu_shell_prepend(GTK_MENU_SHELL(mmguiapp->window->providersmenu), pmenuitem);
					/*Providers*/
					for (piterator = providerlist; piterator != NULL; piterator = piterator->next) {
						dbentry = (mmgui_providers_db_entry_t)piterator->data;
						pmenuitem = gtk_menu_item_new_with_label(dbentry->name);
						gtk_menu_shell_prepend(GTK_MENU_SHELL(mmguiapp->window->providersmenu), pmenuitem);
						g_object_set_data(G_OBJECT(pmenuitem), "dbentry", dbentry);
						g_signal_connect(G_OBJECT(pmenuitem), "activate", G_CALLBACK(mmgui_main_connection_editor_add_db_entry), mmguiapp);
					}
				} else {
					providerlist = g_slist_reverse((GSList *)g_hash_table_lookup(countryhash, (gchar *)citerator->data));
					cmenuitem = gtk_menu_item_new_with_label(mmgui_providers_provider_get_country_name((mmgui_providers_db_entry_t)providerlist->data));
					gtk_menu_shell_append(GTK_MENU_SHELL(mmguiapp->window->providersmenu), cmenuitem);
					submenu = gtk_menu_new();
					gtk_menu_item_set_submenu(GTK_MENU_ITEM(cmenuitem), submenu);
					for (piterator = providerlist; piterator != NULL; piterator = piterator->next) {
						dbentry = (mmgui_providers_db_entry_t)piterator->data;
						pmenuitem = gtk_menu_item_new_with_label(dbentry->name);
						gtk_menu_shell_append(GTK_MENU_SHELL(submenu), pmenuitem);
						g_object_set_data(G_OBJECT(pmenuitem), "dbentry", dbentry);
						g_signal_connect(G_OBJECT(pmenuitem), "activate", G_CALLBACK(mmgui_main_connection_editor_add_db_entry), mmguiapp);
					}
				}
				/*Free providers list*/
				g_slist_free(providerlist);
			}
			/*Free resources*/
			g_list_free(countries);
			g_hash_table_destroy(countryhash);		
			/*Add menu to toolbar*/
			gtk_widget_show_all(mmguiapp->window->providersmenu);
			gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(mmguiapp->window->connaddtoolbutton), GTK_WIDGET(mmguiapp->window->providersmenu));
		}
	}
	
	/*Prepare controls*/
	g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connnameentry), G_CALLBACK(mmgui_main_connection_editor_name_changed_signal), mmguiapp);
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connnameentry), "");
	gtk_widget_set_sensitive(mmguiapp->window->connnameentry, FALSE);
	g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connnameentry), G_CALLBACK(mmgui_main_connection_editor_name_changed_signal), mmguiapp);
	g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connnameapnentry), G_CALLBACK(mmgui_main_connection_editor_apn_changed_signal), mmguiapp);
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connnameapnentry), "");
	gtk_widget_set_sensitive(mmguiapp->window->connnameapnentry, FALSE);
	g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connnameapnentry), G_CALLBACK(mmgui_main_connection_editor_apn_changed_signal), mmguiapp);
	g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connnetroamingcheckbutton), G_CALLBACK(mmgui_main_connection_editor_home_only_toggled_signal), mmguiapp);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->connnetroamingcheckbutton), FALSE);
	gtk_widget_set_sensitive(mmguiapp->window->connnetroamingcheckbutton, FALSE);
	g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connnetroamingcheckbutton), G_CALLBACK(mmgui_main_connection_editor_home_only_toggled_signal), mmguiapp);
	g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connnetidspinbutton), G_CALLBACK(mmgui_main_connection_editor_network_id_changed_signal), mmguiapp);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(mmguiapp->window->connnetidspinbutton), 0.0);
	gtk_widget_set_sensitive(mmguiapp->window->connnetidspinbutton, FALSE);
	g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connnetidspinbutton), G_CALLBACK(mmgui_main_connection_editor_network_id_changed_signal), mmguiapp);
	g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connauthnumberentry), G_CALLBACK(mmgui_main_connection_editor_number_changed_signal), mmguiapp);
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connauthnumberentry), "");
	gtk_widget_set_sensitive(mmguiapp->window->connauthnumberentry, FALSE);
	g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connauthnumberentry), G_CALLBACK(mmgui_main_connection_editor_number_changed_signal), mmguiapp);
	g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connauthusernameentry), G_CALLBACK(mmgui_main_connection_editor_username_changed_signal), mmguiapp);
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connauthusernameentry), "");
	gtk_widget_set_sensitive(mmguiapp->window->connauthusernameentry, FALSE);
	g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connauthusernameentry), G_CALLBACK(mmgui_main_connection_editor_username_changed_signal), mmguiapp);
	g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connauthpassentry), G_CALLBACK(mmgui_main_connection_editor_password_changed_signal), mmguiapp);
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connauthpassentry), "");
	gtk_widget_set_sensitive(mmguiapp->window->connauthpassentry, FALSE);
	g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connauthpassentry), G_CALLBACK(mmgui_main_connection_editor_password_changed_signal), mmguiapp);
	g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->conndns1entry), G_CALLBACK(mmgui_main_connection_editor_dns1_changed_signal), mmguiapp);
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->conndns1entry), "");
	gtk_widget_set_sensitive(mmguiapp->window->conndns1entry, FALSE);
	g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->conndns1entry), G_CALLBACK(mmgui_main_connection_editor_dns1_changed_signal), mmguiapp);
	g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->conndns2entry), G_CALLBACK(mmgui_main_connection_editor_dns2_changed_signal), mmguiapp);
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->conndns2entry), "");
	gtk_widget_set_sensitive(mmguiapp->window->conndns2entry, FALSE);
	g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->conndns2entry), G_CALLBACK(mmgui_main_connection_editor_dns2_changed_signal), mmguiapp);
	
	/*Select active connection*/
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	uuid = mmgui_main_connection_get_active_uuid(mmguiapp);
	if (uuid != NULL) {
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contreeview));
		if ((selection != NULL) && (model != NULL)) {
			if (gtk_tree_model_get_iter_first(model, &iter)) {
				do {
					gtk_tree_model_get(model, &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_UUID, &curuuid, -1);
					if (curuuid != NULL) {
						if (g_strcmp0(uuid, curuuid) == 0) {
							gtk_tree_selection_select_iter(selection, &iter);
							g_signal_emit_by_name(G_OBJECT(mmguiapp->window->contreeview), "cursor-changed", mmguiapp);
							g_free(curuuid);
							break;
						}
						g_free(curuuid);
					}
				} while (gtk_tree_model_iter_next(model, &iter));
			}
		}
		g_free(uuid);
	}
	
	/*Show dialog window*/
	response = gtk_dialog_run(GTK_DIALOG(mmguiapp->window->conneditdialog));
	
	if (response == GTK_RESPONSE_OK) {
		mmgui_main_connection_editor_window_save_changes(mmguiapp);
	} else {
		mmgui_main_connection_editor_window_forget_changes(mmguiapp);
	}
	
	gtk_widget_hide(mmguiapp->window->conneditdialog);
}

static void mmgui_main_connection_editor_window_list_cursor_changed_signal(GtkTreeView *tree_view, gpointer data)
{
	mmgui_application_t mmguiapp;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *name, *uuid, *activeuuid, *number, *username, *password, *apn, *dns1, *dns2;
	guint networkid, type;
	gboolean homeonly, new, changed, removed, protected;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	if ((selection != NULL) && (model != NULL)) {
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_tree_model_get(model, &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, &name,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_UUID, &uuid,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NUMBER, &number,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_USERNAME, &username,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_PASSWORD, &password,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_APN, &apn,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NETWORK_ID, &networkid,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_TYPE, &type,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_HOME_ONLY, &homeonly,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS1, &dns1,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS2, &dns2,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, &new,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, &changed,
											MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, &removed,
											-1);
			/*Protected flag*/
			protected = FALSE;
			if (uuid != NULL) {
				activeuuid = mmguicore_connections_get_active_uuid(mmguiapp->core);
				if (activeuuid != NULL) {
					protected = (g_strcmp0(uuid, activeuuid) == 0);
					g_free(activeuuid);
				}
				g_free(uuid);
			}
			/*Name*/
			g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connnameentry), G_CALLBACK(mmgui_main_connection_editor_name_changed_signal), data);
			if (name != NULL) {
				gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connnameentry), name);
				g_free(name);
			} else {
				gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connnameentry), "");
			}
			g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connnameentry), G_CALLBACK(mmgui_main_connection_editor_name_changed_signal), data);
			gtk_widget_set_sensitive(mmguiapp->window->connnameentry, !(removed || protected));
			/*APN*/
			if (type == MMGUI_DEVICE_TYPE_GSM) {
				g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connnameapnentry), G_CALLBACK(mmgui_main_connection_editor_apn_changed_signal), data);
				if (apn != NULL) {
					gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connnameapnentry), apn);
					g_free(apn);
				} else {
					gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connnameapnentry), "");
				}
				g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connnameapnentry), G_CALLBACK(mmgui_main_connection_editor_apn_changed_signal), data);
				gtk_widget_set_sensitive(mmguiapp->window->connnameapnentry, !(removed || protected));
				#if GTK_CHECK_VERSION(3,10,0)
					gtk_entry_set_icon_from_icon_name(GTK_ENTRY(mmguiapp->window->connnameapnentry), GTK_ENTRY_ICON_SECONDARY, NULL);
				#else
					gtk_entry_set_icon_from_stock(GTK_ENTRY(mmguiapp->window->connnameapnentry), GTK_ENTRY_ICON_SECONDARY, NULL);
				#endif
			} else if (type == MMGUI_DEVICE_TYPE_CDMA) {
				g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connnameapnentry), G_CALLBACK(mmgui_main_connection_editor_apn_changed_signal), data);
				gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connnameapnentry), "");
				g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connnameapnentry), G_CALLBACK(mmgui_main_connection_editor_apn_changed_signal), data);
				gtk_widget_set_sensitive(mmguiapp->window->connnameapnentry, FALSE);
			}
			/*Home only*/
			if (type == MMGUI_DEVICE_TYPE_GSM) {
				g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connnetroamingcheckbutton), G_CALLBACK(mmgui_main_connection_editor_home_only_toggled_signal), data);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->connnetroamingcheckbutton), !homeonly);
				g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connnetroamingcheckbutton), G_CALLBACK(mmgui_main_connection_editor_home_only_toggled_signal), data);
				gtk_widget_set_sensitive(mmguiapp->window->connnetroamingcheckbutton, !(removed || protected));
			} else if (type == MMGUI_DEVICE_TYPE_CDMA) {
				g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connnetroamingcheckbutton), G_CALLBACK(mmgui_main_connection_editor_home_only_toggled_signal), data);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->connnetroamingcheckbutton), FALSE);
				g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connnetroamingcheckbutton), G_CALLBACK(mmgui_main_connection_editor_home_only_toggled_signal), data);
				gtk_widget_set_sensitive(mmguiapp->window->connnetroamingcheckbutton, FALSE);
			}
			/*Network id*/
			if (type == MMGUI_DEVICE_TYPE_GSM) {
				g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connnetidspinbutton), G_CALLBACK(mmgui_main_connection_editor_network_id_changed_signal), data);
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(mmguiapp->window->connnetidspinbutton), (gdouble)networkid);
				g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connnetidspinbutton), G_CALLBACK(mmgui_main_connection_editor_network_id_changed_signal), data);
				gtk_widget_set_sensitive(mmguiapp->window->connnetidspinbutton, !(removed || protected));
			} else if (type == MMGUI_DEVICE_TYPE_CDMA) {
				g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connnetidspinbutton), G_CALLBACK(mmgui_main_connection_editor_network_id_changed_signal), data);
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(mmguiapp->window->connnetidspinbutton), (gdouble)10000);
				g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connnetidspinbutton), G_CALLBACK(mmgui_main_connection_editor_network_id_changed_signal), data);
				gtk_widget_set_sensitive(mmguiapp->window->connnetidspinbutton, FALSE);
			}
			/*Number*/
			g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connauthnumberentry), G_CALLBACK(mmgui_main_connection_editor_number_changed_signal), data);
			if (number != NULL) {
				gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connauthnumberentry), number);
				g_free(number);
			} else {
				gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connauthnumberentry), "");
			}
			g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connauthnumberentry), G_CALLBACK(mmgui_main_connection_editor_number_changed_signal), data);
			gtk_widget_set_sensitive(mmguiapp->window->connauthnumberentry, !(removed || protected));
			/*Username*/
			g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connauthusernameentry), G_CALLBACK(mmgui_main_connection_editor_username_changed_signal), data);
			if (username != NULL) {
				gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connauthusernameentry), username);
				g_free(username);
			} else {
				gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connauthusernameentry), "");
			}
			g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connauthusernameentry), G_CALLBACK(mmgui_main_connection_editor_username_changed_signal), data);
			gtk_widget_set_sensitive(mmguiapp->window->connauthusernameentry, !(removed || protected));
			/*Password*/
			g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->connauthpassentry), G_CALLBACK(mmgui_main_connection_editor_password_changed_signal), data);
			if (password != NULL) {
				gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connauthpassentry), password);
				g_free(password);
			} else {
				gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->connauthpassentry), "");
			}
			g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->connauthpassentry), G_CALLBACK(mmgui_main_connection_editor_password_changed_signal), data);
			gtk_widget_set_sensitive(mmguiapp->window->connauthpassentry, !(removed || protected));
			/*DNS 1*/
			g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->conndns1entry), G_CALLBACK(mmgui_main_connection_editor_dns1_changed_signal), data);
			if (dns1 != NULL) {
				gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->conndns1entry), dns1);
				g_free(dns1);
			} else {
				gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->conndns1entry), "");
			}
			g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->conndns1entry), G_CALLBACK(mmgui_main_connection_editor_dns1_changed_signal), data);
			#if GTK_CHECK_VERSION(3,10,0)
				gtk_entry_set_icon_from_icon_name(GTK_ENTRY(mmguiapp->window->conndns1entry), GTK_ENTRY_ICON_SECONDARY, NULL);
			#else
				gtk_entry_set_icon_from_stock(GTK_ENTRY(mmguiapp->window->conndns1entry), GTK_ENTRY_ICON_SECONDARY, NULL);
			#endif
			gtk_entry_set_icon_tooltip_markup(GTK_ENTRY(mmguiapp->window->conndns1entry), GTK_ENTRY_ICON_SECONDARY, NULL);
			gtk_widget_set_sensitive(mmguiapp->window->conndns1entry, !(removed || protected));
			/*DNS 2*/
			g_signal_handlers_block_by_func(G_OBJECT(mmguiapp->window->conndns2entry), G_CALLBACK(mmgui_main_connection_editor_dns2_changed_signal), data);
			if (dns2 != NULL) {
				gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->conndns2entry), dns2);
				g_free(dns2);
			} else {
				gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->conndns2entry), "");
			}
			g_signal_handlers_unblock_by_func(G_OBJECT(mmguiapp->window->conndns2entry), G_CALLBACK(mmgui_main_connection_editor_dns2_changed_signal), data);
			#if GTK_CHECK_VERSION(3,10,0)
				gtk_entry_set_icon_from_icon_name(GTK_ENTRY(mmguiapp->window->conndns2entry), GTK_ENTRY_ICON_SECONDARY, NULL);
			#else
				gtk_entry_set_icon_from_stock(GTK_ENTRY(mmguiapp->window->conndns2entry), GTK_ENTRY_ICON_SECONDARY, NULL);
			#endif
			gtk_entry_set_icon_tooltip_markup(GTK_ENTRY(mmguiapp->window->conndns2entry), GTK_ENTRY_ICON_SECONDARY, NULL);
			gtk_widget_set_sensitive(mmguiapp->window->conndns2entry, !(removed || protected));
			/*Remove tool button*/
			gtk_widget_set_sensitive(mmguiapp->window->connremovetoolbutton, !(removed || protected));
		}
	}
}

void mmgui_main_connection_editor_window_list_init(mmgui_application_t mmguiapp)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *store;
		
	if (mmguiapp == NULL) return;
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Connection"), renderer, "markup", MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->contreeview), column);
	
	store = gtk_list_store_new(MMGUI_CONNECTION_EDITOR_WINDOW_LIST_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->contreeview), GTK_TREE_MODEL(store));
	g_object_unref(store);
	
	g_signal_connect(G_OBJECT(mmguiapp->window->contreeview), "cursor-changed", G_CALLBACK(mmgui_main_connection_editor_window_list_cursor_changed_signal), mmguiapp);
	
	mmguiapp->window->providersmenu = NULL;
}

static void mmgui_main_connection_editor_window_add_to_list(mmgui_application_t mmguiapp, mmguiconn_t connection, GtkTreeModel *model)
{
	GtkTreeIter iter;
		
	if ((mmguiapp == NULL) || (connection == NULL)) return;
	if (mmguiapp->window == NULL) return;
	
	if (model == NULL) {
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	}
	
	gtk_list_store_append(GTK_LIST_STORE(model), &iter);
	if (connection->type == MMGUI_DEVICE_TYPE_GSM) {
		gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, connection->name,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, connection->name, 
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_UUID, connection->uuid,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NUMBER, connection->number,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_USERNAME, connection->username,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_PASSWORD, connection->password,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_APN, connection->apn,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NETWORK_ID, connection->networkid,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_TYPE, connection->type,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_HOME_ONLY, connection->homeonly,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS1, connection->dns1,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS2, connection->dns2,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, FALSE,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, FALSE,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, FALSE,
														-1);
	} else if (connection->type == MMGUI_DEVICE_TYPE_CDMA) {
		gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CAPTION, connection->name,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NAME, connection->name, 
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_UUID, connection->uuid,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NUMBER, connection->number,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_USERNAME, connection->username,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_PASSWORD, connection->password,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NETWORK_ID, connection->networkid,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_TYPE, connection->type,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS1, connection->dns1,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_DNS2, connection->dns2,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_NEW, FALSE,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_CHANGED, FALSE,
														MMGUI_CONNECTION_EDITOR_WINDOW_LIST_REMOVED, FALSE,
														-1);
	}
}

void mmgui_main_connection_editor_window_list_fill(mmgui_application_t mmguiapp)
{
	GtkTreeModel *model;
	GSList *connections;
	GSList *iterator;
	
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->contreeview));
	if (model != NULL) {
		g_object_ref(model);
		gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->contreeview), NULL);
		gtk_list_store_clear(GTK_LIST_STORE(model));
		connections = mmguicore_connections_get_list(mmguiapp->core);
		if (connections != NULL) {
			/*Add connection entries to list*/
			for (iterator=connections; iterator; iterator=iterator->next) {
				mmgui_main_connection_editor_window_add_to_list(mmguiapp, (mmguiconn_t)iterator->data, model);
			}
		}
		gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->contreeview), model);
		g_object_unref(model);
	}
}
