/*
 *      traffic-page.h
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
 
 
#ifndef __TRAFFIC_PAGE_H__
#define __TRAFFIC_PAGE_H__

#include <gtk/gtk.h>

#include "main.h"

enum _mmgui_main_trafficlist_columns {
	MMGUI_MAIN_TRAFFICLIST_PARAMETER = 0,
	MMGUI_MAIN_TRAFFICLIST_VALUE,
	MMGUI_MAIN_TRAFFICLIST_ID,
	MMGUI_MAIN_TRAFFICLIST_COLUMNS
};

enum _mmgui_main_trafficlist_id {
	MMGUI_MAIN_TRAFFICLIST_ID_CAPTION = 0,
	MMGUI_MAIN_TRAFFICLIST_ID_RXDATA,
	MMGUI_MAIN_TRAFFICLIST_ID_TXDATA,
	MMGUI_MAIN_TRAFFICLIST_ID_RXSPEED,
	MMGUI_MAIN_TRAFFICLIST_ID_TXSPEED,
	MMGUI_MAIN_TRAFFICLIST_ID_TIME,
	MMGUI_MAIN_TRAFFICLIST_ID_DATALIMIT,
	MMGUI_MAIN_TRAFFICLIST_ID_TIMELIMIT,
	
	MMGUI_MAIN_TRAFFICLIST_ID_RXDATA_MONTH,
	MMGUI_MAIN_TRAFFICLIST_ID_TXDATA_MONTH,
	MMGUI_MAIN_TRAFFICLIST_ID_TIME_MONTH,
	
	MMGUI_MAIN_TRAFFICLIST_ID_RXDATA_YEAR,
	MMGUI_MAIN_TRAFFICLIST_ID_TXDATA_YEAR,
	MMGUI_MAIN_TRAFFICLIST_ID_TIME_YEAR
};

enum _mmgui_main_connectionlist_columns {
	MMGUI_MAIN_CONNECTIONLIST_APPLICATION = 0,
	MMGUI_MAIN_CONNECTIONLIST_PID,
	MMGUI_MAIN_CONNECTIONLIST_PROTOCOL,
	MMGUI_MAIN_CONNECTIONLIST_STATE,
	MMGUI_MAIN_CONNECTIONLIST_BUFFER,
	MMGUI_MAIN_CONNECTIONLIST_LOCALADDR,
	MMGUI_MAIN_CONNECTIONLIST_DESTADDR,
	MMGUI_MAIN_CONNECTIONLIST_INODE,
	MMGUI_MAIN_CONNECTIONLIST_TIME,
	MMGUI_MAIN_CONNECTIONLIST_COLUMNS
};

enum _mmgui_main_trafficstatslist_columns {
	MMGUI_MAIN_TRAFFICSTATSLIST_DAY = 0,
	MMGUI_MAIN_TRAFFICSTATSLIST_RXDATA,
	MMGUI_MAIN_TRAFFICSTATSLIST_TXDATA,
	MMGUI_MAIN_TRAFFICSTATSLIST_SESSIONTIME,
	MMGUI_MAIN_TRAFFICSTATSLIST_TIMESATMP,
	MMGUI_MAIN_TRAFFICSTATSLIST_COLUMNS
};

enum _mmgui_main_traffic_limits_validation {
	MMGUI_MAIN_LIMIT_TRAFFIC = 0x00,
	MMGUI_MAIN_LIMIT_TIME = 0x01,
	MMGUI_MAIN_LIMIT_BOTH = 0x02
};



//TRAFFIC
gboolean mmgui_main_traffic_stats_history_update_from_thread(gpointer data);
void mmgui_main_traffic_statistics_dialog(mmgui_application_t mmguiapp);
void mmgui_main_traffic_statistics_dialog_button_clicked_signal(GObject *object, gpointer data);
gboolean mmgui_main_traffic_connections_update_from_thread(gpointer data);
void mmgui_main_traffic_connections_terminate_button_clicked_signal(GObject *object, gpointer data);
void mmgui_main_traffic_connections_dialog(mmgui_application_t mmguiapp);
void mmgui_main_traffic_connections_dialog_button_clicked_signal(GObject *object, gpointer data);
void mmgui_main_traffic_traffic_statistics_list_init(mmgui_application_t mmguiapp);
void mmgui_main_traffic_connections_list_init(mmgui_application_t mmguiapp);
gboolean mmgui_main_traffic_limits_show_message_from_thread(gpointer data);
void mmgui_main_traffic_limits_dialog_time_section_disable_signal(GtkToggleButton *togglebutton, gpointer data);
void mmgui_main_traffic_limits_dialog(mmgui_application_t mmguiapp);
void mmgui_main_traffic_limits_dialog_button_clicked_signal(GObject *object, gpointer data);
//gboolean mmgui_main_traffic_update_statusbar_from_thread(gpointer data);
gboolean mmgui_main_traffic_stats_update_from_thread_foreach(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
gboolean mmgui_main_traffic_stats_update_from_thread(gpointer data);
void mmgui_main_traffic_speed_plot_draw(GtkWidget *widget, cairo_t *cr, gpointer data);
void mmgui_main_traffic_accelerators_init(mmgui_application_t mmguiapp);
void mmgui_main_traffic_list_init(mmgui_application_t mmguiapp);
void mmgui_main_traffic_restore_settings_for_modem(mmgui_application_t mmguiapp);

#endif /* __TRAFFIC_PAGE_H__ */
