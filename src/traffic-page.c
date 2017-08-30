/*
 *      traffic-page.c
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

#include "settings.h"
#include "notifications.h"
#include "strformat.h"
#include "mmguicore.h"
#include "trafficdb.h"
#include "netlink.h"

#include "traffic-page.h"
#include "main.h"

static void mmgui_main_traffic_limits_notification_show_window_callback(gpointer notification, gchar *action, gpointer userdata);
static void mmgui_main_traffic_limits_dialog_traffic_section_disable_signal(GtkToggleButton *togglebutton, gpointer data);
static gboolean mmgui_main_traffic_limits_dialog_open(mmgui_application_t mmguiapp);

/*TRAFFIC*/
static void mmgui_main_traffic_limits_notification_show_window_callback(gpointer notification, gchar *action, gpointer userdata)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)userdata;
	
	if (mmguiapp == NULL) return;
	
	gtk_window_present(GTK_WINDOW(mmguiapp->window->window));
}

gboolean mmgui_main_traffic_stats_history_update_from_thread(gpointer data)
{
	mmgui_application_t mmguiapp;
	mmgui_trafficdb_t trafficdb;
	struct _mmgui_day_traffic traffic;
	GtkTreeModel *model;
	gboolean valid;
	guint64 curtimestamp;
	GtkTreeIter iter;
	gchar strformat[4][64];
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return FALSE;
	if (mmguiapp->core == NULL) return FALSE;
	
	/*If dialog window is not visible - do not update connections list*/
	if (!gtk_widget_get_visible(mmguiapp->window->trafficstatsdialog)) return FALSE;
	
	trafficdb = (mmgui_trafficdb_t)mmguicore_devices_get_traffic_db(mmguiapp->core);
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->trafficstatstreeview));
	
	if ((trafficdb != NULL) && (model != NULL)) {
		if (mmgui_trafficdb_session_get_day_traffic(trafficdb, &traffic)) {
			valid = gtk_tree_model_get_iter_first(model, &iter);
			while (valid) {
				gtk_tree_model_get(model, &iter, MMGUI_MAIN_TRAFFICSTATSLIST_TIMESATMP, &curtimestamp, -1);
				if (traffic.daytime == curtimestamp) {
					//RX bytes
					mmgui_str_format_bytes(traffic.dayrxbytes + traffic.sessrxbytes, strformat[1], sizeof(strformat[1]), FALSE);
					//TX bytes
					mmgui_str_format_bytes(traffic.daytxbytes + traffic.sesstxbytes, strformat[2], sizeof(strformat[2]), FALSE);
					//Session time
					mmgui_str_format_time(traffic.dayduration + traffic.sessduration, strformat[3], sizeof(strformat[3]), FALSE);
					
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_MAIN_TRAFFICSTATSLIST_RXDATA, strformat[1],
																	MMGUI_MAIN_TRAFFICSTATSLIST_TXDATA, strformat[2],
																	MMGUI_MAIN_TRAFFICSTATSLIST_SESSIONTIME, strformat[3],
																	-1);
					break;
				}
				valid = gtk_tree_model_iter_next(model, &iter);
			}
		}
	}
	
	return FALSE;
}

void mmgui_main_traffic_statistics_dialog_fill_statistics(mmgui_application_t mmguiapp, guint month, guint year)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *statistics, *iterator;
	mmgui_trafficdb_t trafficdb;
	mmgui_day_traffic_t traffic;
	struct tm *timespec;
	gchar strformat[4][64];
			
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->trafficstatstreeview));
	trafficdb = (mmgui_trafficdb_t)mmguicore_devices_get_traffic_db(mmguiapp->core);
		
	if ((model != NULL) && (trafficdb != NULL)) {
		g_object_ref(model);
		gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->trafficstatstreeview), NULL);
		
		gtk_list_store_clear(GTK_LIST_STORE(model));
		
		statistics = mmgui_trafficdb_get_traffic_list_for_month(trafficdb, month, year);
		
		if (statistics != NULL) {
			for (iterator=statistics; iterator; iterator=iterator->next) {
				traffic = iterator->data;
				//Date
				timespec = localtime((const time_t *)&(traffic->daytime));
				if (strftime(strformat[0], sizeof(strformat[0]), "%d %B", timespec) == -1) {
					snprintf(strformat[0], sizeof(strformat[0]), _("Unknown"));
				}
				//RX bytes
				mmgui_str_format_bytes(traffic->dayrxbytes + traffic->sessrxbytes, strformat[1], sizeof(strformat[1]), FALSE);
				//TX bytes
				mmgui_str_format_bytes(traffic->daytxbytes + traffic->sesstxbytes, strformat[2], sizeof(strformat[2]), FALSE);
				//Session time
				mmgui_str_format_time(traffic->dayduration + traffic->sessduration, strformat[3], sizeof(strformat[3]), FALSE);
				
				gtk_list_store_append(GTK_LIST_STORE(model), &iter);
				gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_MAIN_TRAFFICSTATSLIST_DAY, strformat[0],
																MMGUI_MAIN_TRAFFICSTATSLIST_RXDATA, strformat[1],
																MMGUI_MAIN_TRAFFICSTATSLIST_TXDATA, strformat[2],
																MMGUI_MAIN_TRAFFICSTATSLIST_SESSIONTIME, strformat[3],
																MMGUI_MAIN_TRAFFICSTATSLIST_TIMESATMP, traffic->daytime,
																-1);
			}
		}
				
		gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->trafficstatstreeview), model);
		g_object_unref(model);
		
		mmgui_trafficdb_free_traffic_list_for_month(statistics);
	}
	
}

void mmgui_main_traffic_statistics_control_apply_button_clicked_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
	time_t presenttime;
	struct tm *timespec;
	gint monthid, yearid;
	guint year;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	//Local time
	presenttime = time(NULL);
	timespec = localtime(&presenttime);
	year = timespec->tm_year+1900;
	//Selected month and year identifiers
	monthid = gtk_combo_box_get_active(GTK_COMBO_BOX(mmguiapp->window->trafficstatsmonthcb));
	yearid = gtk_combo_box_get_active(GTK_COMBO_BOX(mmguiapp->window->trafficstatsyearcb));
	//Translate year
	if (yearid == 0) {
		year = timespec->tm_year+1900-2;
	} else if (yearid == 1) {
		year = timespec->tm_year+1900-1;
	} else if (yearid == 2) {
		year = timespec->tm_year+1900;
	}
	//Reload list
	mmgui_main_traffic_statistics_dialog_fill_statistics(mmguiapp, monthid, year);
}

void mmgui_main_traffic_statistics_dialog(mmgui_application_t mmguiapp)
{
	/*gint response;*/
	time_t presenttime;
	struct tm *timespec;
	gchar strformat[64];
	
	if (mmguiapp == NULL) return;
	
	//Local time
	presenttime = time(NULL);
	timespec = localtime(&presenttime);
	//Clear years
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(mmguiapp->window->trafficstatsyearcb));
	//Years
	snprintf(strformat, sizeof(strformat), "%u", timespec->tm_year+1900-2);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mmguiapp->window->trafficstatsyearcb), strformat);
	snprintf(strformat, sizeof(strformat), "%u", timespec->tm_year+1900-1);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mmguiapp->window->trafficstatsyearcb), strformat);
	snprintf(strformat, sizeof(strformat), "%u", timespec->tm_year+1900);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mmguiapp->window->trafficstatsyearcb), strformat);
	//Select current year
	gtk_combo_box_set_active(GTK_COMBO_BOX(mmguiapp->window->trafficstatsyearcb), 2);
	//Select current month
	gtk_combo_box_set_active(GTK_COMBO_BOX(mmguiapp->window->trafficstatsmonthcb), timespec->tm_mon);
	//Fill list
	mmgui_main_traffic_statistics_dialog_fill_statistics(mmguiapp, timespec->tm_mon, timespec->tm_year+1900);
	
	/*response = */gtk_dialog_run(GTK_DIALOG(mmguiapp->window->trafficstatsdialog));
	
	gtk_widget_hide(mmguiapp->window->trafficstatsdialog);
}

void mmgui_main_traffic_statistics_dialog_button_clicked_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
		
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_traffic_statistics_dialog(mmguiapp);
}

void mmgui_main_traffic_traffic_statistics_list_init(mmgui_application_t mmguiapp)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *store;
	/*GtkTreeIter iter;*/
	
	if (mmguiapp == NULL) return;
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Day"), renderer, "markup", MMGUI_MAIN_TRAFFICSTATSLIST_DAY, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->trafficstatstreeview), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Received data"), renderer, "markup", MMGUI_MAIN_TRAFFICSTATSLIST_RXDATA, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->trafficstatstreeview), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Transmitted data"), renderer, "markup", MMGUI_MAIN_TRAFFICSTATSLIST_TXDATA, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->trafficstatstreeview), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Session time"), renderer, "markup", MMGUI_MAIN_TRAFFICSTATSLIST_SESSIONTIME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->trafficstatstreeview), column);
	
	store = gtk_list_store_new(MMGUI_MAIN_TRAFFICSTATSLIST_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT64);
	
	gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->trafficstatstreeview), GTK_TREE_MODEL(store));
	g_object_unref(store);
}

gboolean mmgui_main_traffic_connections_update_from_thread(gpointer data)
{
	mmgui_application_t mmguiapp;
	mmgui_netlink_connection_change_t fullchange;
	GSList *changes, *iterator;
	gboolean add, remove, modify;
	GHashTable *modconns;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;
	guint inode;
	GtkTreePath *path;
	GtkTreeRowReference *reference;
	GList *rmlist, *rmnode;
	gchar strbuf[32];
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return FALSE;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->conntreeview));
	
	if (model == NULL) return FALSE;
	
	changes = mmguicore_get_connections_changes(mmguiapp->core);
	
	if (changes == NULL) return FALSE;
	
	modconns = g_hash_table_new(g_int_hash, g_int_equal);
	
	add = FALSE; remove = FALSE; modify = FALSE;
	
	for (iterator=changes; iterator; iterator=iterator->next) {
		fullchange = (mmgui_netlink_connection_change_t)iterator->data;
		if (fullchange != NULL) {
			if (fullchange->event == MMGUI_NETLINK_CONNECTION_EVENT_ADD) {
				add = TRUE;
			} else if (fullchange->event == MMGUI_NETLINK_CONNECTION_EVENT_REMOVE) {
				remove = TRUE;
				g_hash_table_insert(modconns, &(fullchange->inode), fullchange);
			} else if (fullchange->event == MMGUI_NETLINK_CONNECTION_EVENT_MODIFY) {
				modify = TRUE;
				g_hash_table_insert(modconns, &(fullchange->inode), fullchange);
			}
		}
	}
	
	gtk_widget_freeze_child_notify(mmguiapp->window->conntreeview);
	
	if ((remove) || (modify)) {
		rmlist = NULL;
		valid = gtk_tree_model_get_iter_first(model, &iter);
		while (valid) {
			gtk_tree_model_get(model, &iter, MMGUI_MAIN_CONNECTIONLIST_INODE, &inode, -1);
			fullchange = (mmgui_netlink_connection_change_t)g_hash_table_lookup(modconns, (gconstpointer)&inode);
			if (fullchange != NULL) {
				if (fullchange->event == MMGUI_NETLINK_CONNECTION_EVENT_MODIFY) {
					/*update connection*/
					mmgui_str_format_bytes((guint64)fullchange->data.params->dqueue, strbuf, sizeof(strbuf), FALSE);
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_MAIN_CONNECTIONLIST_STATE, mmgui_netlink_socket_state(fullchange->data.params->state),
																	MMGUI_MAIN_CONNECTIONLIST_BUFFER, strbuf,
																	-1); 
				} else if (fullchange->event == MMGUI_NETLINK_CONNECTION_EVENT_REMOVE) {
					/*save reference to remove*/
					path = gtk_tree_model_get_path(model, &iter);
					reference = gtk_tree_row_reference_new(model, path);
					rmlist = g_list_prepend(rmlist, reference);
					gtk_tree_path_free(path);
				}
			}
			valid = gtk_tree_model_iter_next(model, &iter);
		}
		/*remove closed connections*/
		if (rmlist != NULL) {
			for (rmnode = rmlist; rmnode != NULL; rmnode = rmnode->next) {
				path = gtk_tree_row_reference_get_path((GtkTreeRowReference *)rmnode->data);
				if (path != NULL) {
					if (gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path)) {
						gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
					}
				}
			}
			g_list_foreach(rmlist, (GFunc)gtk_tree_row_reference_free, NULL);
			g_list_free(rmlist);
		}
	}
	
	if (add) {
		for (iterator=changes; iterator; iterator=iterator->next) {
			fullchange = (mmgui_netlink_connection_change_t)iterator->data;
			if (fullchange != NULL) {
				if (fullchange->event == MMGUI_NETLINK_CONNECTION_EVENT_ADD) {
					mmgui_str_format_bytes((guint64)fullchange->data.connection->dqueue, strbuf, sizeof(strbuf), FALSE);
					gtk_list_store_append(GTK_LIST_STORE(model), &iter);
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_MAIN_CONNECTIONLIST_APPLICATION, fullchange->data.connection->appname,
																	MMGUI_MAIN_CONNECTIONLIST_PID, fullchange->data.connection->apppid,
																	MMGUI_MAIN_CONNECTIONLIST_PROTOCOL, "TCP",
																	MMGUI_MAIN_CONNECTIONLIST_STATE, mmgui_netlink_socket_state(fullchange->data.connection->state),
																	MMGUI_MAIN_CONNECTIONLIST_BUFFER, strbuf,
																	MMGUI_MAIN_CONNECTIONLIST_LOCALADDR, fullchange->data.connection->srcport,
																	MMGUI_MAIN_CONNECTIONLIST_DESTADDR, fullchange->data.connection->dsthostname,
																	MMGUI_MAIN_CONNECTIONLIST_INODE, fullchange->data.connection->inode,
																	-1);
				}
			}
		}
	}
	
	gtk_widget_thaw_child_notify(mmguiapp->window->conntreeview);
	
	/*Free resources*/
	g_slist_foreach(changes, (GFunc)mmgui_netlink_free_connection_change, NULL);
	g_slist_free(changes);
	
	return FALSE;
}

void mmgui_main_traffic_connections_terminate_button_clicked_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	guint pid;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
		
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->conntreeview));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mmguiapp->window->conntreeview));
	
	if (model != NULL) {
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_tree_model_get(model, &iter, MMGUI_MAIN_CONNECTIONLIST_PID, &pid, -1);
			mmgui_netlink_terminate_application((pid_t)pid);
		}
	}
}

void mmgui_main_traffic_connections_dialog(mmgui_application_t mmguiapp)
{
	GtkTreeModel *model;
	GSList *connections, *iterator;
	mmgui_netlink_connection_t connection;
	GtkTreeIter iter;
	gchar strbuf[32];
			
	if (mmguiapp == NULL) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->conntreeview));
	
	if (model != NULL) {
		gtk_list_store_clear(GTK_LIST_STORE(model));
		/*Fill list with initial connections*/
		connections =  mmguicore_open_connections_list(mmguiapp->core);
		if (connections != NULL) {
			for (iterator=connections; iterator; iterator=iterator->next) {
				connection = (mmgui_netlink_connection_t)iterator->data;
				if (connection != NULL) {
					mmgui_str_format_bytes((guint64)connection->dqueue, strbuf, sizeof(strbuf), FALSE);
					gtk_list_store_append(GTK_LIST_STORE(model), &iter);
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_MAIN_CONNECTIONLIST_APPLICATION, connection->appname,
																	MMGUI_MAIN_CONNECTIONLIST_PID, connection->apppid,
																	MMGUI_MAIN_CONNECTIONLIST_PROTOCOL, "TCP",
																	MMGUI_MAIN_CONNECTIONLIST_STATE, mmgui_netlink_socket_state(connection->state),
																	MMGUI_MAIN_CONNECTIONLIST_BUFFER, strbuf,
																	MMGUI_MAIN_CONNECTIONLIST_LOCALADDR, connection->srcport,
																	MMGUI_MAIN_CONNECTIONLIST_DESTADDR, connection->dsthostname,
																	MMGUI_MAIN_CONNECTIONLIST_INODE, connection->inode,
																	-1);
				}
			}
			/*Free resources*/
			g_slist_foreach(connections, (GFunc)mmgui_netlink_free_connection, NULL);
			g_slist_free(connections);
		}
		
		gtk_dialog_run(GTK_DIALOG(mmguiapp->window->conndialog));
		
		mmguicore_close_connections_list(mmguiapp->core);
		
		gtk_widget_hide(mmguiapp->window->conndialog);
	}
}

void mmgui_main_traffic_connections_dialog_button_clicked_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_traffic_connections_dialog(mmguiapp);
}

void mmgui_main_traffic_accelerators_init(mmgui_application_t mmguiapp)
{
	/*Accelerators*/
	mmguiapp->window->connaccelgroup = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(mmguiapp->window->conndialog), mmguiapp->window->connaccelgroup);
	gtk_widget_add_accelerator(mmguiapp->window->conntermtoolbutton, "clicked", mmguiapp->window->connaccelgroup, GDK_KEY_t, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
}

void mmgui_main_traffic_connections_list_init(mmgui_application_t mmguiapp)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *store;
		
	if (mmguiapp == NULL) return;
	
	/*List*/
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Application"), renderer, "markup", MMGUI_MAIN_CONNECTIONLIST_APPLICATION, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->conntreeview), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("PID"), renderer, "markup", MMGUI_MAIN_CONNECTIONLIST_PID, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->conntreeview), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Protocol"), renderer, "markup", MMGUI_MAIN_CONNECTIONLIST_PROTOCOL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->conntreeview), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("State"), renderer, "markup", MMGUI_MAIN_CONNECTIONLIST_STATE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->conntreeview), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Buffer"), renderer, "markup", MMGUI_MAIN_CONNECTIONLIST_BUFFER, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->conntreeview), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Port"), renderer, "markup", MMGUI_MAIN_CONNECTIONLIST_LOCALADDR, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->conntreeview), column);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Destination"), renderer, "markup", MMGUI_MAIN_CONNECTIONLIST_DESTADDR, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->conntreeview), column);
	
	store = gtk_list_store_new(MMGUI_MAIN_CONNECTIONLIST_COLUMNS, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT64);
	
	gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->conntreeview), GTK_TREE_MODEL(store));
	g_object_unref(store);
}

gboolean mmgui_main_traffic_limits_show_message_from_thread(gpointer data)
{
	mmgui_application_data_t mmguiappdata;
	guint eventid;
	gchar *notifycaption, *notifytext;
	enum _mmgui_notifications_sound soundmode;
	
	mmguiappdata = (mmgui_application_data_t)data;
	
	if (mmguiappdata == NULL) return FALSE;
	
	eventid = GPOINTER_TO_INT(mmguiappdata->data);
	
	if (mmguiappdata->mmguiapp->coreoptions != NULL) {
		//Various limits
		switch (eventid) {
			case MMGUI_EVENT_TRAFFIC_LIMIT:
				notifycaption = _("Traffic limit exceeded");
				notifytext = mmguiappdata->mmguiapp->coreoptions->trafficmessage;
				break;
			case MMGUI_EVENT_TIME_LIMIT:
				notifycaption = _("Time limit exceeded");
				notifytext = mmguiappdata->mmguiapp->coreoptions->timemessage;
				break;
			default:
				g_debug("Unknown limit identifier");
				return FALSE;
		}
		
		//Show notification/play sound
		if (mmguiappdata->mmguiapp->options->usesounds) {
			soundmode = MMGUI_NOTIFICATIONS_SOUND_MESSAGE;
		} else {
			soundmode = MMGUI_NOTIFICATIONS_SOUND_NONE;
		}
		
		mmgui_notifications_show(mmguiappdata->mmguiapp->notifications, notifycaption, notifytext, soundmode, mmgui_main_traffic_limits_notification_show_window_callback, mmguiappdata);
	}
	
	g_free(mmguiappdata);
	
	return FALSE;
}

void mmgui_main_traffic_limits_dialog_time_section_disable_signal(GtkToggleButton *togglebutton, gpointer data)
{
	mmgui_application_t mmguiapp;
	gboolean sensitive;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	sensitive = gtk_toggle_button_get_active(togglebutton);
	
	gtk_widget_set_sensitive(mmguiapp->window->timeamount, sensitive);
	gtk_widget_set_sensitive(mmguiapp->window->timeunits, sensitive);
	gtk_widget_set_sensitive(mmguiapp->window->timemessage, sensitive);
	gtk_widget_set_sensitive(mmguiapp->window->timeaction, sensitive);
}

static void mmgui_main_traffic_limits_dialog_traffic_section_disable_signal(GtkToggleButton *togglebutton, gpointer data)
{
	mmgui_application_t mmguiapp;
	gboolean sensitive;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	sensitive = gtk_toggle_button_get_active(togglebutton);
	
	gtk_widget_set_sensitive(mmguiapp->window->trafficamount, sensitive);
	gtk_widget_set_sensitive(mmguiapp->window->trafficunits, sensitive);
	gtk_widget_set_sensitive(mmguiapp->window->trafficmessage, sensitive);
	gtk_widget_set_sensitive(mmguiapp->window->trafficaction, sensitive);
}

static gboolean mmgui_main_traffic_limits_dialog_open(mmgui_application_t mmguiapp)
{
	gint response;
	gulong trafficboxsignal, timeboxsignal;
	
	if (mmguiapp == NULL) return FALSE;
	
	trafficboxsignal = g_signal_connect(G_OBJECT(mmguiapp->window->trafficlimitcheckbutton), "toggled", G_CALLBACK(mmgui_main_traffic_limits_dialog_traffic_section_disable_signal), mmguiapp);
	timeboxsignal = g_signal_connect(G_OBJECT(mmguiapp->window->timelimitcheckbutton), "toggled", G_CALLBACK(mmgui_main_traffic_limits_dialog_time_section_disable_signal), mmguiapp);
	
	g_signal_emit_by_name(G_OBJECT(mmguiapp->window->trafficlimitcheckbutton), "toggled", NULL);
	g_signal_emit_by_name(G_OBJECT(mmguiapp->window->timelimitcheckbutton), "toggled", NULL);
	
	response = gtk_dialog_run(GTK_DIALOG(mmguiapp->window->trafficlimitsdialog));
	
	g_signal_handler_disconnect(G_OBJECT(mmguiapp->window->trafficlimitcheckbutton), trafficboxsignal);
	g_signal_handler_disconnect(G_OBJECT(mmguiapp->window->timelimitcheckbutton), timeboxsignal);
	
	gtk_widget_hide(mmguiapp->window->trafficlimitsdialog);
	
	return (response > 0);
}

void mmgui_main_traffic_limits_dialog(mmgui_application_t mmguiapp)
{
	mmguidevice_t device;
	gchar realtrafficbuf[64], settrafficbuf[64], realtimebuf[64], settimebuf[64];
	gchar *message;
	
	if (mmguiapp == NULL) return;
	if ((mmguiapp->coreoptions == NULL) || (mmguiapp->core == NULL) || (mmguiapp->modemsettings == NULL)) return;
	
	device = mmguicore_devices_get_current(mmguiapp->core);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->trafficlimitcheckbutton), mmguiapp->coreoptions->trafficenabled);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(mmguiapp->window->trafficamount), (gdouble)mmguiapp->coreoptions->trafficamount);
	gtk_combo_box_set_active(GTK_COMBO_BOX(mmguiapp->window->trafficunits), mmguiapp->coreoptions->trafficunits);
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->trafficmessage), mmguiapp->coreoptions->trafficmessage);
	gtk_combo_box_set_active(GTK_COMBO_BOX(mmguiapp->window->trafficaction), mmguiapp->coreoptions->trafficaction);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->timelimitcheckbutton), mmguiapp->coreoptions->timeenabled);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(mmguiapp->window->timeamount), (gdouble)mmguiapp->coreoptions->timeamount);
	gtk_combo_box_set_active(GTK_COMBO_BOX(mmguiapp->window->timeunits), mmguiapp->coreoptions->timeunits);
	gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->timemessage), mmguiapp->coreoptions->timemessage);
	gtk_combo_box_set_active(GTK_COMBO_BOX(mmguiapp->window->timeaction), mmguiapp->coreoptions->timeaction);
	
	if (mmgui_main_traffic_limits_dialog_open(mmguiapp)) {
		if (mmguiapp->coreoptions != NULL) {
			/*Traffic*/
			if (mmguiapp->coreoptions->trafficmessage != NULL) {
				g_free(mmguiapp->coreoptions->trafficmessage);
				mmguiapp->coreoptions->trafficmessage = NULL;
			}
			mmguiapp->coreoptions->trafficenabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->trafficlimitcheckbutton));
			mmguiapp->coreoptions->trafficamount = (guint)gtk_spin_button_get_value(GTK_SPIN_BUTTON(mmguiapp->window->trafficamount));
			mmguiapp->coreoptions->trafficunits = gtk_combo_box_get_active(GTK_COMBO_BOX(mmguiapp->window->trafficunits));
			mmguiapp->coreoptions->trafficmessage = g_strdup(gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->trafficmessage)));
			mmguiapp->coreoptions->trafficaction = gtk_combo_box_get_active(GTK_COMBO_BOX(mmguiapp->window->trafficaction));
			
			switch (mmguiapp->coreoptions->trafficunits) {
				case 0:
					mmguiapp->coreoptions->trafficfull = (guint64)mmguiapp->coreoptions->trafficamount*1024*1024;
					break;
				case 1:
					mmguiapp->coreoptions->trafficfull = (guint64)mmguiapp->coreoptions->trafficamount*1024*1024*1024;
					break;
				case 2:
					mmguiapp->coreoptions->trafficfull = (guint64)mmguiapp->coreoptions->trafficamount*1024*1024*1024*1024;
					break;
				default:
					mmguiapp->coreoptions->trafficfull = (guint64)mmguiapp->coreoptions->trafficamount*1024*1024;
					break;
			}
			
			mmguiapp->coreoptions->trafficexecuted = FALSE;
			
			if (device != NULL) {
				if ((device->connected) && (mmguiapp->coreoptions->trafficenabled) && (mmguiapp->coreoptions->trafficfull < (device->rxbytes + device->txbytes))) {
					mmguiapp->coreoptions->trafficexecuted = TRUE;
				}
			}
			
			/*Time*/
			if (mmguiapp->coreoptions->timemessage != NULL) {
				g_free(mmguiapp->coreoptions->timemessage);
				mmguiapp->coreoptions->timemessage = NULL;
			}
			mmguiapp->coreoptions->timeenabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->timelimitcheckbutton));
			mmguiapp->coreoptions->timeamount = (guint)gtk_spin_button_get_value(GTK_SPIN_BUTTON(mmguiapp->window->timeamount));
			mmguiapp->coreoptions->timeunits = gtk_combo_box_get_active(GTK_COMBO_BOX(mmguiapp->window->timeunits));
			mmguiapp->coreoptions->timemessage = g_strdup(gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->timemessage)));
			mmguiapp->coreoptions->timeaction = gtk_combo_box_get_active(GTK_COMBO_BOX(mmguiapp->window->timeaction));
			
			switch (mmguiapp->coreoptions->timeunits) {
				case 0:
					mmguiapp->coreoptions->timefull = mmguiapp->coreoptions->timeamount*60;
					break;
				case 1:
					mmguiapp->coreoptions->timefull = mmguiapp->coreoptions->timeamount*60*60;
					break;
				default:
					mmguiapp->coreoptions->timefull = mmguiapp->coreoptions->timeamount*60;
					break;
			}
			
			mmguiapp->coreoptions->timeexecuted = FALSE;
			
			if (device != NULL) {
				if ((device->connected) && (mmguiapp->coreoptions->timeenabled) && (mmguiapp->coreoptions->timefull < device->sessiontime)) {
					mmguiapp->coreoptions->timeexecuted = TRUE;
				}
			}
			
			/*Save settings*/
			/*Traffic*/
			mmgui_modem_settings_set_boolean(mmguiapp->modemsettings, "limits_traffic_enabled", mmguiapp->coreoptions->trafficenabled);
			mmgui_modem_settings_set_int(mmguiapp->modemsettings, "limits_traffic_amount", (gint)mmguiapp->coreoptions->trafficamount);
			mmgui_modem_settings_set_int(mmguiapp->modemsettings, "limits_traffic_units", (guint)mmguiapp->coreoptions->trafficunits);
			mmgui_modem_settings_set_string(mmguiapp->modemsettings, "limits_traffic_message", mmguiapp->coreoptions->trafficmessage);
			mmgui_modem_settings_set_int(mmguiapp->modemsettings, "limits_traffic_action", (guint)mmguiapp->coreoptions->trafficaction);
			/*Time*/
			mmgui_modem_settings_set_boolean(mmguiapp->modemsettings, "limits_time_enabled", mmguiapp->coreoptions->timeenabled);
			mmgui_modem_settings_set_int(mmguiapp->modemsettings, "limits_time_amount", (guint)mmguiapp->coreoptions->timeamount);
			mmgui_modem_settings_set_int(mmguiapp->modemsettings, "limits_time_units", (guint)mmguiapp->coreoptions->timeunits);
			mmgui_modem_settings_set_string(mmguiapp->modemsettings, "limits_time_message", mmguiapp->coreoptions->timemessage);
			mmgui_modem_settings_set_int(mmguiapp->modemsettings, "limits_time_action", (guint)mmguiapp->coreoptions->timeaction);
			
			if (device != NULL) {
				if ((mmguiapp->coreoptions->trafficexecuted) || (mmguiapp->coreoptions->timeexecuted)) {
					if ((mmguiapp->coreoptions->trafficexecuted) && (mmguiapp->coreoptions->timeexecuted)) {
						message = g_strdup_printf(_("Traffic: %s, limit set to: %s\nTime: %s, limit set to: %s\nPlease check entered values and try once more"),
													mmgui_str_format_bytes(device->rxbytes + device->txbytes, realtrafficbuf, sizeof(realtrafficbuf), TRUE),
													mmgui_str_format_bytes(mmguiapp->coreoptions->trafficfull, settrafficbuf, sizeof(settrafficbuf), TRUE),
													mmgui_str_format_time(device->sessiontime, realtimebuf, sizeof(realtimebuf), TRUE),
													mmgui_str_format_time(mmguiapp->coreoptions->timefull, settimebuf, sizeof(settimebuf), TRUE));
						mmgui_main_ui_error_dialog_open(mmguiapp, _("Wrong traffic and time limit values"), message);
						g_free(message);
					} else if (mmguiapp->coreoptions->trafficexecuted) {
						message = g_strdup_printf(_("Traffic: %s, limit set to: %s\nPlease check entered values and try once more"),
													mmgui_str_format_bytes(device->rxbytes + device->txbytes, realtrafficbuf, sizeof(realtrafficbuf), TRUE),
													mmgui_str_format_bytes(mmguiapp->coreoptions->trafficfull, settrafficbuf, sizeof(settrafficbuf), TRUE));
						mmgui_main_ui_error_dialog_open(mmguiapp, _("Wrong traffic limit value"), message);
						g_free(message);
					} else if (mmguiapp->coreoptions->timeexecuted) {
						message = g_strdup_printf(_("Time: %s, limit set to: %s\nPlease check entered values and try once more"),
													mmgui_str_format_time(device->sessiontime, realtimebuf, sizeof(realtimebuf), TRUE),
													mmgui_str_format_time(mmguiapp->coreoptions->timefull, settimebuf, sizeof(settimebuf), TRUE));
						mmgui_main_ui_error_dialog_open(mmguiapp, _("Wrong time limit value"), message);
						g_free(message);
					}
				}
			}
		}
	}
}

void mmgui_main_traffic_limits_dialog_button_clicked_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
		
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
		
	mmgui_main_traffic_limits_dialog(mmguiapp);
}

gboolean mmgui_main_traffic_stats_update_from_thread(gpointer data)
{
	mmgui_application_t mmguiapp;
	mmgui_trafficdb_t trafficdb;
	mmguidevice_t device;
	GtkTreeModel *model;
	GtkTreeIter sectioniter, elementiter;
	gboolean sectionvalid, elementvalid;
	gint id;
	gchar buffer[64];
	gfloat speed;
	guint64 limitleft;
	GdkWindow *window;
	gboolean visible;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return FALSE;
	if (mmguiapp->core == NULL) return FALSE;
	if (mmguiapp->core->device == NULL) return FALSE;
	
	trafficdb = (mmgui_trafficdb_t)mmguicore_devices_get_traffic_db(mmguiapp->core);
	
	/*Update traffic statistics*/
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->trafficparamslist));
	if (model != NULL) {
		sectionvalid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &sectioniter);
		while (sectionvalid) {
			/*Sections*/
			if (gtk_tree_model_iter_has_child(model, &sectioniter)) {
				/*Elements*/
				if (gtk_tree_model_iter_children(model, &elementiter, &sectioniter)) {
					do {
						device = mmguicore_devices_get_current(mmguiapp->core);
						
						if ((device == NULL) || ((device != NULL) && (!device->connected))) {
							gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, "", -1);
						} else {
							gtk_tree_model_get(model, &elementiter, MMGUI_MAIN_TRAFFICLIST_ID, &id, -1);
							switch (id) {
								case MMGUI_MAIN_TRAFFICLIST_ID_RXDATA:
									gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, mmgui_str_format_bytes(device->rxbytes, buffer, sizeof(buffer), TRUE), -1);
									break;
								case MMGUI_MAIN_TRAFFICLIST_ID_TXDATA:
									gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, mmgui_str_format_bytes(device->txbytes, buffer, sizeof(buffer), TRUE), -1);
									break;
								case MMGUI_MAIN_TRAFFICLIST_ID_RXSPEED:
									if (device->speedindex < MMGUI_SPEED_VALUES_NUMBER) {
										if (device->speedindex == 0) {
											speed = device->speedvalues[0][device->speedindex];
										} else {
											speed = device->speedvalues[0][device->speedindex-1];
										}
									} else {
										speed = device->speedvalues[0][MMGUI_SPEED_VALUES_NUMBER-1];
									}
									gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, mmgui_str_format_speed(speed, buffer, sizeof(buffer), TRUE), -1);
									break;
								case MMGUI_MAIN_TRAFFICLIST_ID_TXSPEED:
									if (device->speedindex < MMGUI_SPEED_VALUES_NUMBER) {
										if (device->speedindex == 0) {
											speed = device->speedvalues[1][device->speedindex];
										} else {
											speed = device->speedvalues[1][device->speedindex-1];
										}
									} else {
										speed = device->speedvalues[1][MMGUI_SPEED_VALUES_NUMBER-1];
									}
									gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, mmgui_str_format_speed(speed, buffer, sizeof(buffer), TRUE), -1);
									break;
								case MMGUI_MAIN_TRAFFICLIST_ID_TIME:
									if (device->connected) {
										gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, mmgui_str_format_time(device->sessiontime, buffer, sizeof(buffer), TRUE), -1);
									} else {
										gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, _("<small><b>Disconnected</b></small>"), -1);
									}
									break;
								case MMGUI_MAIN_TRAFFICLIST_ID_DATALIMIT:
									if (device->connected) {
										if (mmguiapp->coreoptions != NULL) {
											if (!mmguiapp->coreoptions->trafficexecuted) {
												if (mmguiapp->coreoptions->trafficenabled) {
													limitleft = mmguiapp->coreoptions->trafficfull - (device->rxbytes + device->txbytes);
													if (mmguiapp->coreoptions->trafficfull > (device->rxbytes + device->txbytes)) {
														gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, mmgui_str_format_bytes(limitleft, buffer, sizeof(buffer), TRUE), -1);
													} else {
														gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, _("<small><b>Limit</b></small>"), -1);
													}
												} else {
													gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, _("<small><b>Disabled</b></small>"), -1);
												}
											} else {
												gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, _("<small><b>Limit</b></small>"), -1);
											}
										} else {
											gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, _("<small><b>Disabled</b></small>"), -1);
										}
									} else {
										gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, _("<small><b>Disconnected</b></small>"), -1);
									}
									break;
								case MMGUI_MAIN_TRAFFICLIST_ID_TIMELIMIT:
									if (device->connected) {
										if (mmguiapp->coreoptions != NULL) {
											if (!mmguiapp->coreoptions->timeexecuted) {
												if (mmguiapp->coreoptions->timeenabled) {
													limitleft = mmguiapp->coreoptions->timefull - device->sessiontime;
													if (mmguiapp->coreoptions->timefull > device->sessiontime) {
														gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, mmgui_str_format_time(limitleft, buffer, sizeof(buffer), TRUE), -1);
													} else {
														gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, _("<small><b>Limit</b></small>"), -1);
													}
												} else {
													gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, _("<small><b>Disabled</b></small>"), -1);
												}
											} else {
												gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, _("<small><b>Limit</b></small>"), -1);
											}
										} else {
											gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, _("<small><b>Disabled</b></small>"), -1);
										}
									} else {
										gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, _("<small><b>Disconnected</b></small>"), -1);
									}
									break;
								case MMGUI_MAIN_TRAFFICLIST_ID_RXDATA_MONTH:
									gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, mmgui_str_format_bytes(trafficdb->monthrxbytes, buffer, sizeof(buffer), TRUE), -1);
									break;
								case MMGUI_MAIN_TRAFFICLIST_ID_TXDATA_MONTH:
									gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, mmgui_str_format_bytes(trafficdb->monthtxbytes, buffer, sizeof(buffer), TRUE), -1);
									break;
								case MMGUI_MAIN_TRAFFICLIST_ID_TIME_MONTH:
									gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, mmgui_str_format_time(trafficdb->monthduration, buffer, sizeof(buffer), TRUE), -1);
									break;
								case MMGUI_MAIN_TRAFFICLIST_ID_RXDATA_YEAR:
									gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, mmgui_str_format_bytes(trafficdb->yearrxbytes, buffer, sizeof(buffer), TRUE), -1);
									break;
								case MMGUI_MAIN_TRAFFICLIST_ID_TXDATA_YEAR:
									gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, mmgui_str_format_bytes(trafficdb->yeartxbytes, buffer, sizeof(buffer), TRUE), -1);
									break;
								case MMGUI_MAIN_TRAFFICLIST_ID_TIME_YEAR:
									gtk_tree_store_set(GTK_TREE_STORE(model), &elementiter, MMGUI_MAIN_TRAFFICLIST_VALUE, mmgui_str_format_time(trafficdb->yearduration, buffer, sizeof(buffer), TRUE), -1);
									break;
								default:
									break;
							}
						}
						elementvalid = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &elementiter);
					} while (elementvalid);
				}
			}
			sectionvalid = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &sectioniter);
		}
	}
	/*Update traffic graph*/
	window = gtk_widget_get_window(mmguiapp->window->trafficdrawingarea);
	visible = gtk_widget_get_visible(mmguiapp->window->trafficdrawingarea);
	/*Update only if needed*/
	if ((gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiapp->window->notebook)) == MMGUI_MAIN_PAGE_TRAFFIC) && (window != NULL) && (visible)) {
		//TODO: Determine rectangle
		gdk_window_invalidate_rect(window, NULL, FALSE);
	}
		
	return FALSE;
}

void mmgui_main_traffic_speed_plot_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	gint width, height;
	gint i, c, graphlen;
	gfloat maxvalue;
	gchar strbuffer[32];
	const gdouble dashed[1] = {1.0};
	mmgui_application_t mmguiapp;
	mmguidevice_t device;
	gdouble rxr, rxg, rxb, txr, txg, txb;
	
	mmguiapp = (mmgui_application_t)data;
	if (mmguiapp == NULL) return;
	
	device = mmguicore_devices_get_current(mmguiapp->core);
	if (device == NULL) return;
	
	#if GTK_CHECK_VERSION(3,4,0)
		/*RX speed graph color*/
		rxr = mmguiapp->options->rxtrafficcolor.red;
		rxg = mmguiapp->options->rxtrafficcolor.green;
		rxb = mmguiapp->options->rxtrafficcolor.blue;
		/*TX speed graph color*/
		txr = mmguiapp->options->txtrafficcolor.red;
		txg = mmguiapp->options->txtrafficcolor.green;
		txb = mmguiapp->options->txtrafficcolor.blue;
	#else
		/*RX speed graph color*/
		rxr = mmguiapp->options->rxtrafficcolor.red/65535.0;
		rxg = mmguiapp->options->rxtrafficcolor.green/65535.0;
		rxb = mmguiapp->options->rxtrafficcolor.blue/65535.0;
		/*TX speed graph color*/
		txr = mmguiapp->options->txtrafficcolor.red/65535.0;
		txg = mmguiapp->options->txtrafficcolor.green/65535.0;
		txb = mmguiapp->options->txtrafficcolor.blue/65535.0;
	#endif
	
	maxvalue = 100.0;
	
	if ((device->connected) && (device->speedindex > 0)) {
		for (i=device->speedindex-1; i>=0; i--) {
			if (device->speedvalues[0][i] > maxvalue) {
				maxvalue = device->speedvalues[0][i];
			}
			if (device->speedvalues[1][i] > maxvalue) {
				maxvalue = device->speedvalues[1][i];
			}
		}
	}
	
	if (maxvalue < 100.0) maxvalue = 100.0;
	
	width = gtk_widget_get_allocated_width(widget);
	height = gtk_widget_get_allocated_height(widget);
	
	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	cairo_set_line_width(cr, 1.5);
	
	graphlen = 19*(gint)((width-60)/19.0);
	
	cairo_move_to(cr, 30, 30);
	cairo_line_to(cr, 30, height-30);
	cairo_line_to(cr, 30+graphlen, height-30);
	cairo_line_to(cr, 30+graphlen, 30);
	cairo_line_to(cr, 30, 30);
	
	cairo_stroke(cr);
	
	cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 1);
	cairo_set_line_width(cr, 1.0);
	cairo_set_dash(cr, dashed, 1, 0);
	
	for (i=1; i<10; i++) {
		cairo_move_to(cr, 30, height-30-(i*(gint)((height-60)/10.0)));
		cairo_line_to(cr, 30+graphlen, height-30-(i*(gint)((height-60)/10.0)));
	}
	
	for (i=1; i<19; i++) {
		cairo_move_to(cr, 30+(i*(gint)((width-60)/19.0)), 30);
		cairo_line_to(cr, 30+(i*(gint)((width-60)/19.0)), height-30);
	}
	
	cairo_stroke(cr);
	
	cairo_set_dash(cr, dashed, 0, 0);
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 8);
	
	for (i=0; i<=10; i++) {
		cairo_move_to(cr, 0, height-30+3-(i*(gint)((height-60)/10.0)));
		memset(strbuffer, 0, sizeof(strbuffer));
		g_snprintf(strbuffer, sizeof(strbuffer), "%4.0f", i*(maxvalue/10.0));
		cairo_show_text(cr, strbuffer);
	}
	
	cairo_move_to(cr, 0, 15);
	cairo_show_text(cr, _("kbps"));
	
	for (i=0; i<19; i++) {
		cairo_move_to(cr, 30-5+(i*(gint)((width-60)/19.0)), height-8);
		memset(strbuffer, 0, sizeof(strbuffer));
		if (mmguiapp->options->graphrighttoleft) {
			g_snprintf(strbuffer, sizeof(strbuffer), "%i", (19-i+1)*MMGUI_THREAD_SLEEP_PERIOD);
		} else {
			g_snprintf(strbuffer, sizeof(strbuffer), "%i", (i+1)*MMGUI_THREAD_SLEEP_PERIOD);
		}
		cairo_show_text(cr, strbuffer);
	}
	
	cairo_move_to(cr, width-35, height-8);
	cairo_show_text(cr, _("sec"));
	
	cairo_stroke(cr);
	
	if ((device != NULL) && (device->connected) && (device->speedindex > 0)) {
		cairo_set_source_rgba(cr, txr, txg, txb, 1.0);
		cairo_set_line_width(cr, 2.5);
		cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND); 
		if (mmguiapp->options->graphrighttoleft) {
			c = device->speedindex-1;
			for (i=0; i<device->speedindex; i++) {
				if (i == device->speedindex-1) {
					cairo_arc(cr, graphlen+30, height-30-(gint)(device->speedvalues[1][i]*((height-60)/maxvalue)), 2.0, 0*(3.14/180.0), 360*(3.14/180.0));
					cairo_move_to(cr, graphlen+30, height-30-(gint)(device->speedvalues[1][i]*((height-60)/maxvalue)));
				} else {
					cairo_line_to(cr, graphlen+30-(c*(gint)((width-60)/19.0)), height-30-(gint)(device->speedvalues[1][i]*((height-60)/maxvalue)));
					cairo_arc(cr, graphlen+30-(c*(gint)((width-60)/19.0)), height-30-(gint)(device->speedvalues[1][i]*((height-60)/maxvalue)), 2.0, 0*(3.14/180.0), 360*(3.14/180.0));
					cairo_move_to(cr, graphlen+30-(c*(gint)((width-60)/19.0)), height-30-(gint)(device->speedvalues[1][i]*((height-60)/maxvalue)));
					c--;
				}
			}
		} else {
			c = device->speedindex-1;
			for (i=0; i<device->speedindex; i++) {
				if (i == device->speedindex-1) {
					cairo_arc(cr, 30, height-30-(gint)(device->speedvalues[1][i]*((height-60)/maxvalue)), 2.0, 0*(3.14/180.0), 360*(3.14/180.0));
					cairo_move_to(cr, 30, height-30-(gint)(device->speedvalues[1][i]*((height-60)/maxvalue)));
				} else {
					cairo_line_to(cr, 30+(c*(gint)((width-60)/19.0)), height-30-(gint)(device->speedvalues[1][i]*((height-60)/maxvalue)));
					cairo_arc(cr, 30+(c*(gint)((width-60)/19.0)), height-30-(gint)(device->speedvalues[1][i]*((height-60)/maxvalue)), 2.0, 0*(3.14/180.0), 360*(3.14/180.0));
					cairo_move_to(cr, 30+(c*(gint)((width-60)/19.0)), height-30-(gint)(device->speedvalues[1][i]*((height-60)/maxvalue)));
					c--;
				}
			}
		}
		cairo_stroke(cr);
		
		cairo_set_source_rgba(cr, rxr, rxg, rxb, 1.0);
		cairo_set_line_width(cr, 2.5);
		cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND); 
		if (mmguiapp->options->graphrighttoleft) {
			c = device->speedindex-1;
			for (i=0; i<device->speedindex; i++) {
				if (i == device->speedindex-1) {
					cairo_arc(cr, graphlen+30, height-30-(gint)(device->speedvalues[0][i]*((height-60)/maxvalue)), 2.0, 0*(3.14/180.0), 360*(3.14/180.0));
					cairo_move_to(cr, graphlen+30, height-30-(gint)(device->speedvalues[0][i]*((height-60)/maxvalue)));
				} else {
					cairo_line_to(cr, graphlen+30-(c*(gint)((width-60)/19.0)), height-30-(gint)(device->speedvalues[0][i]*((height-60)/maxvalue)));
					cairo_arc(cr, graphlen+30-(c*(gint)((width-60)/19.0)), height-30-(gint)(device->speedvalues[0][i]*((height-60)/maxvalue)), 2.0, 0*(3.14/180.0), 360*(3.14/180.0));
					cairo_move_to(cr, graphlen+30-(c*(gint)((width-60)/19.0)), height-30-(gint)(device->speedvalues[0][i]*((height-60)/maxvalue)));
					c--;
				}
			}
		} else {
			c = device->speedindex-1;
			for (i=0; i<device->speedindex; i++) {
				if (i == device->speedindex-1) {
					cairo_arc(cr, 30, height-30-(gint)(device->speedvalues[0][i]*((height-60)/maxvalue)), 2.0, 0*(3.14/180.0), 360*(3.14/180.0));
					cairo_move_to(cr, 30, height-30-(gint)(device->speedvalues[0][i]*((height-60)/maxvalue)));
				} else {
					cairo_line_to(cr, 30+(c*(gint)((width-60)/19.0)), height-30-(gint)(device->speedvalues[0][i]*((height-60)/maxvalue)));
					cairo_arc(cr, 30+(c*(gint)((width-60)/19.0)), height-30-(gint)(device->speedvalues[0][i]*((height-60)/maxvalue)), 2.0, 0*(3.14/180.0), 360*(3.14/180.0));
					cairo_move_to(cr, 30+(c*(gint)((width-60)/19.0)), height-30-(gint)(device->speedvalues[0][i]*((height-60)/maxvalue)));
					c--;
				}
			}
		}
		cairo_stroke(cr);
	}
	//RX speed
	cairo_set_source_rgba(cr, rxr, rxg, rxb, 1.0);
	cairo_set_line_width(cr, 2.5);
	
	cairo_arc(cr, width-230, 12, 2.0, 0*(3.14/180.0), 360*(3.14/180.0));
	cairo_move_to(cr, width-222, 12);
	cairo_line_to(cr, width-238, 12);
	
	cairo_stroke(cr);
	
	cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 1);
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 10);
	
	cairo_move_to(cr, width-220, 15);
	cairo_show_text(cr, _("RX speed"));
	
	cairo_stroke(cr);
	
	//TX speed
	cairo_set_source_rgba(cr, txr, txg, txb, 1.0);
	cairo_set_line_width(cr, 2.5);
	
	cairo_arc(cr, width-110, 12, 2.0, 0*(3.14/180.0), 360*(3.14/180.0));
	cairo_move_to(cr, width-102, 12);
	cairo_line_to(cr, width-118, 12);
	
	cairo_stroke(cr);
	
	cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 1);
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 10);
	
	cairo_move_to(cr, width-100, 15);
	cairo_show_text(cr, _("TX speed"));
	
	cairo_stroke(cr);
}

void mmgui_main_traffic_list_init(mmgui_application_t mmguiapp)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeStore *store;
	GtkTreeIter iter;
	GtkTreeIter subiter;
	
	if (mmguiapp == NULL) return;
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Parameter"), renderer, "markup", MMGUI_MAIN_TRAFFICLIST_PARAMETER, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->trafficparamslist), column);
	
	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, "ellipsize-set", TRUE, NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Value"), renderer, "markup", MMGUI_MAIN_TRAFFICLIST_VALUE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->trafficparamslist), column);
	
	store = gtk_tree_store_new(MMGUI_MAIN_TRAFFICLIST_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
	
	gtk_tree_store_append(store, &iter, NULL);
	gtk_tree_store_set(store, &iter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Session</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_CAPTION, -1);
	
	gtk_tree_store_append(store, &subiter, &iter);
	gtk_tree_store_set(store, &subiter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Received data</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_RXDATA, -1);
	
	gtk_tree_store_append(store, &subiter, &iter);
	gtk_tree_store_set(store, &subiter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Transmitted data</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_TXDATA, -1);
	
	gtk_tree_store_append(store, &subiter, &iter);
	gtk_tree_store_set(store, &subiter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Receive speed</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_RXSPEED, -1);
	
	gtk_tree_store_append(store, &subiter, &iter);
	gtk_tree_store_set(store, &subiter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Transmit speed</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_TXSPEED, -1);
	
	gtk_tree_store_append(store, &subiter, &iter);
	gtk_tree_store_set(store, &subiter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Session time</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_TIME, -1);
	
	gtk_tree_store_append(store, &subiter, &iter);
	gtk_tree_store_set(store, &subiter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Traffic left</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_DATALIMIT, -1);
	
	gtk_tree_store_append(store, &subiter, &iter);
	gtk_tree_store_set(store, &subiter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Time left</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_TIMELIMIT, -1);
	
	gtk_tree_store_append(store, &iter, NULL);
	gtk_tree_store_set(store, &iter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Month</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_CAPTION, -1);
	
	gtk_tree_store_append(store, &subiter, &iter);
	gtk_tree_store_set(store, &subiter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Received data</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_RXDATA_MONTH, -1);
	
	gtk_tree_store_append(store, &subiter, &iter);
	gtk_tree_store_set(store, &subiter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Transmitted data</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_TXDATA_MONTH, -1);
	
	gtk_tree_store_append(store, &subiter, &iter);
	gtk_tree_store_set(store, &subiter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Total time</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_TIME_MONTH, -1);
	
	gtk_tree_store_append(store, &iter, NULL);
	gtk_tree_store_set(store, &iter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Year</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_CAPTION, -1);
	
	gtk_tree_store_append(store, &subiter, &iter);
	gtk_tree_store_set(store, &subiter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Received data</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_RXDATA_YEAR, -1);
	
	gtk_tree_store_append(store, &subiter, &iter);
	gtk_tree_store_set(store, &subiter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Transmitted data</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_TXDATA_YEAR, -1);
	
	gtk_tree_store_append(store, &subiter, &iter);
	gtk_tree_store_set(store, &subiter, MMGUI_MAIN_TRAFFICLIST_PARAMETER, _("<small><b>Total time</b></small>"), MMGUI_MAIN_TRAFFICLIST_ID, MMGUI_MAIN_TRAFFICLIST_ID_TIME_YEAR, -1);
	
	gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->trafficparamslist), GTK_TREE_MODEL(store));
	
	gtk_tree_view_expand_all(GTK_TREE_VIEW(mmguiapp->window->trafficparamslist));
	
	g_object_unref(store);
}

void mmgui_main_traffic_restore_settings_for_modem(mmgui_application_t mmguiapp)
{
	if (mmguiapp == NULL) return;
	if ((mmguiapp->modemsettings == NULL) || (mmguiapp->coreoptions == NULL)) return;
	
	
	/*Traffic limits*/
	if (mmguiapp->coreoptions->trafficmessage != NULL) {
		g_free(mmguiapp->coreoptions->trafficmessage);
		mmguiapp->coreoptions->trafficmessage = NULL;
	}
	
	mmguiapp->coreoptions->trafficenabled = mmgui_modem_settings_get_boolean(mmguiapp->modemsettings, "limits_traffic_enabled", FALSE);
	mmguiapp->coreoptions->trafficamount = (guint)mmgui_modem_settings_get_int(mmguiapp->modemsettings, "limits_traffic_amount", 150);
	mmguiapp->coreoptions->trafficunits = (guint)mmgui_modem_settings_get_int(mmguiapp->modemsettings, "limits_traffic_units", 0);
	mmguiapp->coreoptions->trafficaction = (guint)mmgui_modem_settings_get_int(mmguiapp->modemsettings, "limits_traffic_action", 0);
	mmguiapp->coreoptions->trafficmessage = mmgui_modem_settings_get_string(mmguiapp->modemsettings, "limits_traffic_message", _("Traffic limit exceeded... It's time to take rest \\(^_^)/"));
		
	switch (mmguiapp->coreoptions->trafficunits) {
		case 0:
			mmguiapp->coreoptions->trafficfull = (guint64)mmguiapp->coreoptions->trafficamount*1024*1024;
			break;
		case 1:
			mmguiapp->coreoptions->trafficfull = (guint64)mmguiapp->coreoptions->trafficamount*1024*1024*1024;
			break;
		case 2:
			mmguiapp->coreoptions->trafficfull = (guint64)mmguiapp->coreoptions->trafficamount*1024*1024*1024*1024;
			break;
		default:
			mmguiapp->coreoptions->trafficfull = (guint64)mmguiapp->coreoptions->trafficamount*1024*1024;
			break;
	}
	
	mmguiapp->coreoptions->trafficexecuted = FALSE;
	
	/*Time limits*/
	if (mmguiapp->coreoptions->timemessage != NULL) {
		g_free(mmguiapp->coreoptions->timemessage);
		mmguiapp->coreoptions->timemessage = NULL;
	}
	
	mmguiapp->coreoptions->timemessage = mmgui_modem_settings_get_string(mmguiapp->modemsettings, "limits_time_message", _("Time limit exceeded... Go sleep and have nice dreams -_-"));
	mmguiapp->coreoptions->timeenabled = mmgui_modem_settings_get_boolean(mmguiapp->modemsettings, "limits_time_enabled", FALSE);
	mmguiapp->coreoptions->timeamount = (guint)mmgui_modem_settings_get_int(mmguiapp->modemsettings, "limits_time_amount", 60);
	mmguiapp->coreoptions->timeunits = (guint)mmgui_modem_settings_get_int(mmguiapp->modemsettings, "limits_time_units", 0);
	mmguiapp->coreoptions->timeaction = (guint)mmgui_modem_settings_get_int(mmguiapp->modemsettings, "limits_time_action", 0);
	
	switch (mmguiapp->coreoptions->timeunits) {
		case 0:
			mmguiapp->coreoptions->timefull = mmguiapp->coreoptions->timeamount*60;
			break;
		case 1:
			mmguiapp->coreoptions->timefull = mmguiapp->coreoptions->timeamount*60*60;
			break;
		default:
			mmguiapp->coreoptions->timefull = mmguiapp->coreoptions->timeamount*60;
			break;
	}
	
	mmguiapp->coreoptions->timeexecuted = FALSE;
}
