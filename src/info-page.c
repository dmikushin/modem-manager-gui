/*
 *      info-page.c
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
#include "mmguicore.h"
#include "../resources.h"

#include "info-page.h"
#include "main.h"

//INFO
void mmgui_main_info_update_for_modem(mmgui_application_t mmguiapp)
{
	mmguidevice_t device;
	gchar buffer[256];
	guint locationcaps;
	
	if (mmguiapp == NULL) return;
	
	device = mmguicore_devices_get_current(mmguiapp->core);
	locationcaps = mmguicore_location_get_capabilities(mmguiapp->core);
	
	if (device != NULL) {
		/*Device*/
		g_snprintf(buffer, sizeof(buffer), "%s %s (%s)", device->manufacturer, device->model, device->port);
		gtk_label_set_label(GTK_LABEL(mmguiapp->window->devicevlabel), buffer);
		/*Operator name*/
		gtk_label_set_label(GTK_LABEL(mmguiapp->window->operatorvlabel), device->operatorname);
		/*Operator code*/
		gtk_label_set_label(GTK_LABEL(mmguiapp->window->operatorcodevlabel), mmgui_str_format_operator_code(device->operatorcode, device->type, buffer, sizeof(buffer)));
		/*Registration state*/
		gtk_label_set_label(GTK_LABEL(mmguiapp->window->regstatevlabel), mmgui_str_format_reg_status(device->regstatus));
		/*Network mode*/
		gtk_label_set_label(GTK_LABEL(mmguiapp->window->modevlabel), mmgui_str_format_mode_string(device->mode));
		/*IMEI*/
		gtk_label_set_label(GTK_LABEL(mmguiapp->window->imeivlabel), device->imei);
		/*IMSI*/
		gtk_label_set_label(GTK_LABEL(mmguiapp->window->imsivlabel), device->imsi);
		/*Signal level*/
		g_snprintf(buffer, sizeof(buffer), "%u%%", device->siglevel);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(mmguiapp->window->signallevelprogressbar), buffer);
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(mmguiapp->window->signallevelprogressbar), device->siglevel/100.0);
		/*Location*/
		if (locationcaps & MMGUI_LOCATION_CAPS_3GPP) {
			memset(buffer, 0, sizeof(buffer));
			g_snprintf(buffer, sizeof(buffer), "%u/%u/%u/%u/%u/%u", device->loc3gppdata[0], device->loc3gppdata[1], device->loc3gppdata[2], (device->loc3gppdata[3] >> 16) & 0x0000ffff, device->loc3gppdata[3] & 0x0000ffff, device->loc3gppdata[4]);
			gtk_label_set_label(GTK_LABEL(mmguiapp->window->info3gpplocvlabel), buffer);
		} else {
			gtk_label_set_label(GTK_LABEL(mmguiapp->window->info3gpplocvlabel), _("Not supported"));
		}
		if (locationcaps & MMGUI_LOCATION_CAPS_GPS) {
			memset(buffer, 0, sizeof(buffer));
			g_snprintf(buffer, sizeof(buffer), "%3.2f/%3.2f/%3.2f", device->locgpsdata[0], device->locgpsdata[1], device->locgpsdata[2]);
			gtk_label_set_label(GTK_LABEL(mmguiapp->window->infogpslocvlabel), buffer);
		} else {
			gtk_label_set_label(GTK_LABEL(mmguiapp->window->infogpslocvlabel), _("Not supported"));
		}
	}
}

void mmgui_main_info_handle_signal_level_change(mmgui_application_t mmguiapp, mmguidevice_t device)
{
	gchar strbuf[256];
	
	if ((mmguiapp == NULL) || (device == NULL)) return;
	
	g_snprintf(strbuf, sizeof(strbuf), "%u%%", device->siglevel);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(mmguiapp->window->signallevelprogressbar), strbuf);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(mmguiapp->window->signallevelprogressbar), device->siglevel/100.0);
}

void mmgui_main_info_handle_network_mode_change(mmgui_application_t mmguiapp, mmguidevice_t device)
{
	if ((mmguiapp == NULL) || (device == NULL)) return;
	
	gtk_label_set_label(GTK_LABEL(mmguiapp->window->modevlabel), mmgui_str_format_mode_string(device->mode));
}

void mmgui_main_info_handle_network_registration_change(mmgui_application_t mmguiapp, mmguidevice_t device)
{
	guint setpage;
	gchar buffer[256];
	
	if ((mmguiapp == NULL) || (device == NULL)) return;
	
	gtk_label_set_label(GTK_LABEL(mmguiapp->window->operatorvlabel), device->operatorname);
	gtk_label_set_label(GTK_LABEL(mmguiapp->window->operatorcodevlabel), mmgui_str_format_operator_code(device->operatorcode, device->type, buffer, sizeof(buffer)));
	gtk_label_set_label(GTK_LABEL(mmguiapp->window->regstatevlabel), mmgui_str_format_reg_status(device->regstatus));
	
	/*Update current page state*/
	setpage = gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiapp->window->notebook));
	mmgui_main_ui_test_device_state(mmguiapp, setpage);
}

void mmgui_main_info_handle_location_change(mmgui_application_t mmguiapp, mmguidevice_t device)
{
	gchar buffer[256];
	guint locationcaps;
	
	if ((mmguiapp == NULL) || (device == NULL)) return;
	
	locationcaps = mmguicore_location_get_capabilities(mmguiapp->core);
	
	if (locationcaps & MMGUI_LOCATION_CAPS_3GPP) {
		memset(buffer, 0, sizeof(buffer));
		g_snprintf(buffer, sizeof(buffer), "%u/%u/%u/%u/%u", device->loc3gppdata[0], device->loc3gppdata[1], device->loc3gppdata[2], (device->loc3gppdata[3] >> 16) & 0x0000ffff, device->loc3gppdata[3] & 0x0000ffff);
		gtk_label_set_label(GTK_LABEL(mmguiapp->window->info3gpplocvlabel), buffer);
	} else {
		gtk_label_set_label(GTK_LABEL(mmguiapp->window->info3gpplocvlabel), _("Not supported"));
	}
	
	if (locationcaps & MMGUI_LOCATION_CAPS_GPS) {
		memset(buffer, 0, sizeof(buffer));
		g_snprintf(buffer, sizeof(buffer), "%2.3f/%2.3f/%2.3f", device->locgpsdata[0], device->locgpsdata[1], device->locgpsdata[2]);
		gtk_label_set_label(GTK_LABEL(mmguiapp->window->infogpslocvlabel), buffer);
	} else {
		gtk_label_set_label(GTK_LABEL(mmguiapp->window->infogpslocvlabel), _("Not supported"));
	}
}

void mmgui_main_info_state_clear(mmgui_application_t mmguiapp)
{
	/*Clear 'Info' page fields*/
	gtk_label_set_text(GTK_LABEL(mmguiapp->window->devicevlabel), "");
	gtk_label_set_text(GTK_LABEL(mmguiapp->window->operatorvlabel), "");
	gtk_label_set_text(GTK_LABEL(mmguiapp->window->modevlabel), "");
	gtk_label_set_text(GTK_LABEL(mmguiapp->window->imeivlabel), "");
	gtk_label_set_text(GTK_LABEL(mmguiapp->window->imsivlabel), "");
	gtk_label_set_text(GTK_LABEL(mmguiapp->window->info3gpplocvlabel), "");
	gtk_label_set_text(GTK_LABEL(mmguiapp->window->infogpslocvlabel), "");
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(mmguiapp->window->signallevelprogressbar), "");
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(mmguiapp->window->signallevelprogressbar), 0.0);
}
