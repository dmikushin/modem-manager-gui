/*
 *      devices-page.c
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

#include "settings.h"
#include "encoding.h"
#include "notifications.h"
#include "ayatana.h"
#include "mmguicore.h"

#include "devices-page.h"
#include "info-page.h"
#include "main.h"

static gboolean mmgui_main_device_list_unselect_foreach(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static void mmgui_main_device_list_select_signal(GtkCellRendererToggle *cell_renderer, gchar *path, gpointer data);
static void mmgui_main_device_remove_from_list(mmgui_application_t mmguiapp, guint devid);
static void mmgui_main_device_add_to_list(mmgui_application_t mmguiapp, mmguidevice_t device, GtkTreeModel *model);

/*Devices*/
gboolean mmgui_main_device_handle_added_from_thread(gpointer data)
{
	mmgui_application_data_t mmguiappdata;
	mmguidevice_t device;
	
	mmguiappdata = (mmgui_application_data_t)data;
	
	if (mmguiappdata == NULL) return FALSE;
	
	device = (mmguidevice_t)mmguiappdata->data;
	//Add device to list
	mmgui_main_device_add_to_list(mmguiappdata->mmguiapp, device, NULL);
	//If no device opened, open that one
	if (mmguicore_devices_get_current(mmguiappdata->mmguiapp->core) == NULL) {
		mmgui_main_device_select_from_list(mmguiappdata->mmguiapp, device->persistentid);
	}
	//Unlock control buttons
	mmgui_main_ui_control_buttons_disable(mmguiappdata->mmguiapp, FALSE);
	
	g_free(mmguiappdata);
	
	return FALSE;
}

gboolean mmgui_main_device_handle_removed_from_thread(gpointer data)
{
	mmgui_application_data_t mmguiappdata;
	guint id;
	
	mmguiappdata = (mmgui_application_data_t)data;
	
	if (mmguiappdata == NULL) return FALSE;
	
	id = GPOINTER_TO_UINT(mmguiappdata->data);
	//Remove device from list
	mmgui_main_device_remove_from_list(mmguiappdata->mmguiapp, id);
	//Look for available devices
	if (mmguicore_devices_get_list(mmguiappdata->mmguiapp->core) == NULL) {
		//No devices available, lock control buttons
		mmgui_main_ui_control_buttons_disable(mmguiappdata->mmguiapp, TRUE);
	} else if (mmguicore_devices_get_current(mmguiappdata->mmguiapp->core) == NULL) {
		//If no device opened, open default one
		mmgui_main_device_select_from_list(mmguiappdata->mmguiapp, NULL);
	}
	
	g_free(mmguiappdata);
		
	return FALSE;
}

void mmgui_main_device_handle_enabled_local_status(mmgui_application_t mmguiapp)
{
	guint setpage;
	gboolean enabled;
	
	if (mmguiapp == NULL) return;
	
	enabled = mmguicore_devices_get_enabled(mmguiapp->core);
	if (enabled) {
		/*Update device information*/
		mmgui_main_info_update_for_modem(mmguiapp);
	}
	/*Update current page state*/
	setpage = gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiapp->window->notebook));
	mmgui_main_ui_test_device_state(mmguiapp, setpage);
}

gboolean mmgui_main_device_handle_enabled_status_from_thread(gpointer data)
{
	mmgui_application_data_t mmguiappdata;
	gboolean enabledstatus;
	guint setpage;
	
	mmguiappdata = (mmgui_application_data_t)data;
	
	if (mmguiappdata == NULL) return FALSE;
	
	enabledstatus = (gboolean)GPOINTER_TO_UINT(mmguiappdata->data);
	
	if (mmguiappdata->mmguiapp == NULL) return FALSE;
	
	if (enabledstatus) {
		//Update device information
		mmgui_main_info_update_for_modem(mmguiappdata->mmguiapp);
	}
	
	/*Update current page state*/
	setpage = gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiappdata->mmguiapp->window->notebook));
	mmgui_main_ui_test_device_state(mmguiappdata->mmguiapp, setpage);
	
	g_free(mmguiappdata);
	
	return FALSE;
}

gboolean mmgui_main_device_handle_blocked_status_from_thread(gpointer data)
{
	mmgui_application_data_t mmguiappdata;
	gboolean blockedstatus;
	guint setpage;
	
	mmguiappdata = (mmgui_application_data_t)data;
	
	if (mmguiappdata == NULL) return FALSE;
	
	blockedstatus = (gboolean)GPOINTER_TO_UINT(mmguiappdata->data);
	
	if (mmguiappdata->mmguiapp == NULL) return FALSE;
	
	/*Update current page state*/
	setpage = gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiappdata->mmguiapp->window->notebook));
	mmgui_main_ui_test_device_state(mmguiappdata->mmguiapp, setpage);
	
	g_free(mmguiappdata);
	
	g_debug("Device blocked status: %u\n", blockedstatus);
	
	return FALSE;
}

gboolean mmgui_main_device_handle_prepared_status_from_thread(gpointer data)
{
	mmgui_application_data_t mmguiappdata;
	gboolean preparedstatus;
	guint setpage;
	
	mmguiappdata = (mmgui_application_data_t)data;
	
	if (mmguiappdata == NULL) return FALSE;
	
	preparedstatus = (gboolean)GPOINTER_TO_UINT(mmguiappdata->data);
	
	if (mmguiappdata->mmguiapp == NULL) return FALSE;
	
	/*Update current page state*/
	setpage = gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiappdata->mmguiapp->window->notebook));
	mmgui_main_ui_test_device_state(mmguiappdata->mmguiapp, setpage);
	
	g_free(mmguiappdata);
	
	g_debug("Device prepared status: %u\n", preparedstatus);
	
	return FALSE;
}

gboolean mmgui_main_device_handle_connection_status_from_thread(gpointer data)
{
	mmgui_application_data_t mmguiappdata;
	gboolean connstatus;
	guint setpage;
	
	mmguiappdata = (mmgui_application_data_t)data;
	
	if (mmguiappdata == NULL) return FALSE;
	
	connstatus = (gboolean)GPOINTER_TO_UINT(mmguiappdata->data);
	
	if (mmguiappdata->mmguiapp == NULL) return FALSE;
	
	/*Update current page state*/
	setpage = gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiappdata->mmguiapp->window->notebook));
	mmgui_main_ui_test_device_state(mmguiappdata->mmguiapp, setpage);
	
	g_free(mmguiappdata);
	
	g_debug("Device connection status: %u\n", connstatus);
	
	return FALSE;
}

static gboolean mmgui_main_device_list_unselect_foreach(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	mmgui_application_t mmguiapp;
	gboolean enabled;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return TRUE;
	
	gtk_tree_model_get(model, iter, MMGUI_MAIN_DEVLIST_ENABLED, &enabled, -1);
	if (enabled) {
		gtk_list_store_set(GTK_LIST_STORE(model), iter, MMGUI_MAIN_DEVLIST_ENABLED, FALSE, -1);
		return TRUE;
	}
	
	return FALSE;
}

static void mmgui_main_device_list_select_signal(GtkCellRendererToggle *cell_renderer, gchar *path, gpointer data)
{
	mmgui_application_t mmguiapp;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean enabled;
	guint id;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->devlist));
	if (gtk_tree_model_get_iter_from_string(model, &iter, path)) {
		gtk_tree_model_get(model, &iter, MMGUI_MAIN_DEVLIST_ENABLED, &enabled, MMGUI_MAIN_DEVLIST_ID, &id, -1);
		if (!enabled) {
			gtk_tree_model_foreach(model, mmgui_main_device_list_unselect_foreach, mmguiapp);
			gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_MAIN_DEVLIST_ENABLED, TRUE, -1);
			if (!mmguicore_devices_open(mmguiapp->core, id, TRUE)) {
				mmgui_main_ui_error_dialog_open(mmguiapp, _("<b>Error opening device</b>"), mmguicore_get_last_error(mmguiapp->core));
			}
		}
	}
}

gboolean mmgui_main_device_select_from_list(mmgui_application_t mmguiapp, gchar *identifier)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;
	gboolean selected;
	guint curid;
	gchar *curidentifier;
		
	if (mmguiapp == NULL) return FALSE;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->devlist));
	
	if (model == NULL) return FALSE;
	
	selected = FALSE;
	
	//Select requested device
	if (identifier != NULL) {
		for (valid = gtk_tree_model_get_iter_first(model, &iter); valid; valid = gtk_tree_model_iter_next(model, &iter)) {
			gtk_tree_model_get(model, &iter, MMGUI_MAIN_DEVLIST_ID, &curid, MMGUI_MAIN_DEVLIST_IDENTIFIER, &curidentifier, -1);
			if (g_str_equal(identifier, curidentifier)) {
				gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_MAIN_DEVLIST_ENABLED, TRUE, -1);
				selected = TRUE;
				break;
			}
		}
	}
	
	//If needed device not found, select first one
	if (!selected) {
		for (valid = gtk_tree_model_get_iter_first(model, &iter); valid; valid = gtk_tree_model_iter_next(model, &iter)) {
			gtk_tree_model_get(model, &iter, MMGUI_MAIN_DEVLIST_ID, &curid, MMGUI_MAIN_DEVLIST_IDENTIFIER, &curidentifier, -1);
			gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_MAIN_DEVLIST_ENABLED, TRUE, -1);
			selected = TRUE;
			break;
		}
	}
	
	if (selected) {
		if (mmguicore_devices_open(mmguiapp->core, curid, TRUE)) {
			mmgui_main_ui_control_buttons_disable(mmguiapp, FALSE);
			gmm_settings_set_string(mmguiapp->settings, "device_identifier", identifier);
			return TRUE;
		} else {
			mmgui_main_ui_control_buttons_disable(mmguiapp, TRUE);
			mmgui_main_ui_error_dialog_open(mmguiapp, _("<b>Error opening device</b>"), mmguicore_get_last_error(mmguiapp->core));
		}
	} else {
		g_debug("No devices to select\n");
		mmgui_main_ui_control_buttons_disable(mmguiapp, TRUE);
	}
	
	return FALSE;
}

static void mmgui_main_device_remove_from_list(mmgui_application_t mmguiapp, guint devid)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;
	guint currentid;
	
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->devlist));
	
	if (model != NULL) {
		for (valid = gtk_tree_model_get_iter_first(model, &iter); valid; valid = gtk_tree_model_iter_next(model, &iter)) {
			gtk_tree_model_get(model, &iter, MMGUI_MAIN_DEVLIST_ID, &currentid, -1);
			if (currentid == devid) {
				gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
				break;
			}
		}
	}
}

static void mmgui_main_device_add_to_list(mmgui_application_t mmguiapp, mmguidevice_t device, GtkTreeModel *model)
{
	GtkTreeIter iter;
	gchar *markup;
	gchar *devtype;
	gchar *devmanufacturer, *devmodel, *devversion;
	
	if ((mmguiapp == NULL) || (device == NULL)) return;
	if (mmguiapp->window == NULL) return;
	
	if (model == NULL) {
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->devlist));
	}
	
	devtype = NULL;
	
	if (device->type == MMGUI_DEVICE_TYPE_GSM) {
		devtype = "GSM";
	} else if (device->type == MMGUI_DEVICE_TYPE_CDMA) {
		devtype = "CDMA";
	}
	
	devmanufacturer = encoding_clear_special_symbols(g_strdup(device->manufacturer), strlen(device->manufacturer));
	devmodel = encoding_clear_special_symbols(g_strdup(device->model), strlen(device->model));
	devversion = encoding_clear_special_symbols(g_strdup(device->version), strlen(device->version));
	
	markup = g_strdup_printf(_("<b>%s %s</b>\nVersion:%s Port:%s Type:%s"), devmanufacturer, devmodel, devversion, device->port, devtype); 
	
	gtk_list_store_append(GTK_LIST_STORE(model), &iter);
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_MAIN_DEVLIST_ENABLED, FALSE,
													MMGUI_MAIN_DEVLIST_DESCRIPTION, markup,
													MMGUI_MAIN_DEVLIST_ID, device->id,
													MMGUI_MAIN_DEVLIST_IDENTIFIER, device->persistentid,
													-1);
	
	g_free(devmanufacturer);
	g_free(devmodel);
	g_free(devversion);
	g_free(markup);
}

void mmgui_main_device_list_fill(mmgui_application_t mmguiapp)
{
	GtkTreeModel *model;
	GSList *devices;
	GSList *iterator;
	
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->devlist));
	if (model != NULL) {
		g_object_ref(model);
		gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->devlist), NULL);
		gtk_list_store_clear(GTK_LIST_STORE(model));
		devices = mmguicore_devices_get_list(mmguiapp->core);
		if (devices != NULL) {
			for (iterator=devices; iterator; iterator=iterator->next) {
				mmgui_main_device_add_to_list(mmguiapp, (mmguidevice_t)iterator->data, model);
			}
		}
		gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->devlist), model);
		g_object_unref(model);
	}
}

void mmgui_main_device_list_init(mmgui_application_t mmguiapp)
{
	GtkCellRenderer *tbrenderer;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *store;
	
	if (mmguiapp == NULL) return;
	
	tbrenderer = gtk_cell_renderer_toggle_new();
	column = gtk_tree_view_column_new_with_attributes(_("Selected"), tbrenderer, "active", MMGUI_MAIN_DEVLIST_ENABLED, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->devlist), column);
	gtk_cell_renderer_toggle_set_radio(GTK_CELL_RENDERER_TOGGLE(tbrenderer), TRUE);
		
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Device"), renderer, "markup", MMGUI_MAIN_DEVLIST_DESCRIPTION, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->devlist), column);
	
	store = gtk_list_store_new(MMGUI_MAIN_DEVLIST_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->devlist), GTK_TREE_MODEL(store));
	g_object_unref(store);
	
	//Device selection signal
	g_signal_connect(G_OBJECT(tbrenderer), "toggled", G_CALLBACK(mmgui_main_device_list_select_signal), mmguiapp);
}

void mmgui_main_conn_edit_button_clicked_signal(GtkButton *button, gpointer data)
{
	mmgui_application_t mmguiapp;
	gint response;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	/*TEST CURRENT DEVICE*/
	
	response = gtk_dialog_run(GTK_DIALOG(mmguiapp->window->conneditdialog));
	
	if (response > 0) {
		
	}
	
	gtk_widget_hide(mmguiapp->window->conneditdialog);
}

void mmgui_main_conn_ctl_button_clicked_signal(GtkButton *button, gpointer data)
{
	
	
}
