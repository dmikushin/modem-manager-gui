/*
 *      scan-page.c
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

#include "strformat.h"
#include "../resources.h"

#include "scan-page.h"
#include "main.h"

static void mmgui_main_scan_list_fill_foreach(gpointer data, gpointer user_data);

/*SCAN*/
void mmgui_main_scan_start(mmgui_application_t mmguiapp)
{
	if (mmguiapp == NULL) return;
	
	if (mmguicore_networks_scan(mmguiapp->core)) {
		mmgui_ui_infobar_show(mmguiapp, _("Scanning networks..."), MMGUI_MAIN_INFOBAR_TYPE_PROGRESS, TRUE);
	} else {
		mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_FAIL, _("Device error"));
	}
}

void mmgui_main_scan_create_connection_button_clicked_signal(GObject *object, gpointer data)
{
		
}

void mmgui_main_scan_start_button_clicked_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_scan_start(mmguiapp);
}

static void mmgui_main_scan_list_cursor_changed_signal(GtkTreeView *tree_view, gpointer data)
{
	mmgui_application_t mmguiapp;
		
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
}

static void mmgui_main_scan_list_row_activated_signal(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
}

static void mmgui_main_scan_list_fill_foreach(gpointer data, gpointer user_data)
{
	mmgui_scanned_network_t network;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *markup;
	gboolean netavailability;
		
	network = (mmgui_scanned_network_t)data;
	model = (GtkTreeModel *)user_data;
	
	if ((network == NULL) || (model == NULL)) return;
	
	markup = g_strdup_printf(_("<b>%s</b>\n<small>%s ID: %u Availability: %s Access tech: %s</small>"), 
							network->operator_long,
							network->operator_short,
							network->operator_num,
							mmgui_str_format_na_status_string(network->status),
							mmgui_str_format_access_tech_string(network->access_tech));
	
	netavailability = ((network->status == MMGUI_NA_AVAILABLE) || (network->status == MMGUI_NA_CURRENT));
	
	gtk_list_store_append(GTK_LIST_STORE(model), &iter);
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_MAIN_SCANLIST_OPERATOR, markup, MMGUI_MAIN_SCANLIST_NAME, network->operator_long, MMGUI_MAIN_SCANLIST_IDENIFIER, network->operator_num, MMGUI_MAIN_SCANLIST_AVAILABILITY, netavailability, -1);
	
	g_free(markup);
}

void mmgui_main_scan_list_fill(mmgui_application_t mmguiapp, mmguicore_t mmguicore, GSList *netlist)
{
	GtkTreeModel *model;
	
	if ((mmguiapp == NULL) || (mmguicore == NULL)) return;
	
	if (netlist != NULL) {
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->scanlist));
		if (model != NULL) {
			//Detach model
			g_object_ref(model);
			gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->scanlist), NULL);
			//Clear model
			gtk_list_store_clear(GTK_LIST_STORE(model));
			//Fill model
			g_slist_foreach(netlist, mmgui_main_scan_list_fill_foreach, model);
			//Attach model
			gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->scanlist), model);
			g_object_unref(model);
		}
		//Free networks list
		mmguicore_networks_scan_free(netlist);
	} else {
		 mmgui_main_ui_error_dialog_open(mmguiapp, _("<b>Error scanning networks</b>"), mmguicore_get_last_error(mmguiapp->core));
	}
}

void mmgui_main_scan_list_init(mmgui_application_t mmguiapp)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *store;
	
	if (mmguiapp == NULL) return;
	
	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, "ellipsize-set", TRUE, NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Operator"), renderer, "markup", MMGUI_MAIN_SCANLIST_OPERATOR, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->scanlist), column);
	
	store = gtk_list_store_new(MMGUI_MAIN_SCANLIST_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_BOOLEAN);
	gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->scanlist), GTK_TREE_MODEL(store));
	g_object_unref(store);
	
	/*Select network signal*/
	g_signal_connect(G_OBJECT(mmguiapp->window->scanlist), "cursor-changed", G_CALLBACK(mmgui_main_scan_list_cursor_changed_signal), mmguiapp);
	/*Create connection signal*/
	g_signal_connect(G_OBJECT(mmguiapp->window->scanlist), "row-activated", G_CALLBACK(mmgui_main_scan_list_row_activated_signal), mmguiapp);
}

void mmgui_main_scan_state_clear(mmgui_application_t mmguiapp)
{
	GtkTreeModel *model;
	
	if (mmguiapp == NULL) return;
	
	/*Clear scanned networks list*/
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->scanlist));
	if (model != NULL) {
		gtk_list_store_clear(GTK_LIST_STORE(model));
	}
	
	/*Disable 'Create connection' button*/
	gtk_widget_set_sensitive(GTK_WIDGET(mmguiapp->window->scancreateconnectionbutton), FALSE);
}
