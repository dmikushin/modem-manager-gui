/*
 *      main.c
 *      
 *      Copyright 2012-2015 Alex <alex@linuxonly.ru>
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
#include <unistd.h>
#include <sys/stat.h>
#ifdef __GLIBC__
	#include <execinfo.h>
	#define __USE_GNU
	#include <ucontext.h>
	#include <signal.h>
#endif
#include <gtk/gtk.h>
#include <locale.h>
#include <libintl.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gprintf.h>

#include "settings.h"
#include "ussdlist.h"
#include "encoding.h"
#include "libpaths.h"
#include "notifications.h"
#include "addressbooks.h"
#include "ayatana.h"
#include "strformat.h"
#include "mmguicore.h"
#include "smsdb.h"
#include "trafficdb.h"
#include "netlink.h"
#include "resources.h"

#include "contacts-page.h"
#include "traffic-page.h"
#include "scan-page.h"
#include "info-page.h"
#include "ussd-page.h"
#include "sms-page.h"
#include "devices-page.h"
#include "preferences-window.h"
#include "welcome-window.h"

#include "main.h"

#if GTK_CHECK_VERSION(3,12,0)
	#define mmgui_add_accelerator(application, accelerator, action) \
				gtk_application_set_accels_for_action(application, action, (const gchar*[]) {accelerator, NULL});
	#define mmgui_add_accelerator_with_parameter(application, accelerator, action, parameter) \
				gtk_application_set_accels_for_action(application, action"::"parameter, (const gchar*[]) {accelerator, NULL});
#else
	#define mmgui_add_accelerator(application, accelerator, action) \
				gtk_application_add_accelerator(application, accelerator, action, NULL);
	#define mmgui_add_accelerator_with_parameter(application, accelerator, action, parameter) \
				gtk_application_add_accelerator(application, accelerator, action, g_variant_new_string(parameter));
#endif

#define MMGUI_MAIN_DEFAULT_RX_GRAPH_RGBA_COLOR  "rgba(7,139,45,1.0)"
#define MMGUI_MAIN_DEFAULT_TX_GRAPH_RGBA_COLOR  "rgba(153,17,77,1.0)"
#define MMGUI_MAIN_DEFAULT_RX_GRAPH_RGB_COLOR   "#078b2d"
#define MMGUI_MAIN_DEFAULT_TX_GRAPH_RGB_COLOR   "#99114d"

enum _mmgui_main_control_shortcuts {
	MMGUI_MAIN_CONTROL_SHORTCUT_SMS_NEW = 0,
	MMGUI_MAIN_CONTROL_SHORTCUT_SMS_REMOVE,
	MMGUI_MAIN_CONTROL_SHORTCUT_SMS_ANSWER,
	MMGUI_MAIN_CONTROL_SHORTCUT_USSD_EDITOR,
	MMGUI_MAIN_CONTROL_SHORTCUT_USSD_SEND,
	MMGUI_MAIN_CONTROL_SHORTCUT_SCAN_START,
	MMGUI_MAIN_CONTROL_SHORTCUT_TRAFFIC_LIMIT,
	MMGUI_MAIN_CONTROL_SHORTCUT_TRAFFIC_CONNECTIONS,
	MMGUI_MAIN_CONTROL_SHORTCUT_TRAFFIC_STATS,
	MMGUI_MAIN_CONTROL_SHORTCUT_CONTACTS_NEW,
	MMGUI_MAIN_CONTROL_SHORTCUT_CONTACTS_REMOVE,
	MMGUI_MAIN_CONTROL_SHORTCUT_CONTACTS_SMS,
	MMGUI_MAIN_CONTROL_SHORTCUT_NUMBER
};

/*Exit dialog response codes*/
enum _mmgui_main_exit_dialog_result {
	MMGUI_MAIN_EXIT_DIALOG_CANCEL = -1,
	MMGUI_MAIN_EXIT_DIALOG_EXIT   =  0,
	MMGUI_MAIN_EXIT_DIALOG_HIDE   =  1
};

/*Widget list used during UI creation process*/
struct _mmgui_main_widgetset {
	gchar *name;
	GtkWidget **widget;
};
/*Pixbuf list used during UI creation process*/
struct _mmgui_main_pixbufset {
	gchar *name;
	GdkPixbuf **pixbuf;
};
/*Closure list used during UI creation process*/
struct _mmgui_main_closureset {
	guint identifier;
	GClosure **closure;
};

/*EVENTS*/
static void mmgui_main_event_callback(enum _mmgui_event event, gpointer mmguicore, gpointer data, gpointer userdata);
static gboolean mmgui_main_handle_extend_capabilities(mmgui_application_t mmguiapp, gint id);
/*UI*/
static void mmgui_main_ui_page_control_disable(mmgui_application_t mmguiapp, guint page, gboolean disable, gboolean onlylimited);
static void mmgui_main_ui_page_setup_shortcuts(mmgui_application_t mmguiapp, guint setpage);
static void mmgui_main_ui_page_use_shortcuts_signal(gpointer data);
static void mmgui_main_ui_open_page(mmgui_application_t mmguiapp, guint page);
static void mmgui_main_ui_application_menu_set_page(mmgui_application_t mmguiapp, guint page);
static void mmgui_main_ui_application_menu_set_state(mmgui_application_t mmguiapp, gboolean enabled);
static enum _mmgui_main_exit_dialog_result mmgui_main_ui_window_hide_dialog(mmgui_application_t mmguiapp);
static void mmgui_main_ui_window_save_geometry(mmgui_application_t mmguiapp);
static void mmgui_main_ui_exit_menu_item_activate_signal(GSimpleAction *action, GVariant *parameter, gpointer data);
static void mmgui_main_ui_help_menu_item_activate_signal(GSimpleAction *action, GVariant *parameter, gpointer data);
static void mmgui_main_ui_about_menu_item_activate_signal(GSimpleAction *action, GVariant *parameter, gpointer data);
static void mmgui_main_ui_section_menu_item_activate_signal(GSimpleAction *action, GVariant *parameter, gpointer data);
static void mmgui_main_tray_icon_activation_signal(GtkStatusIcon *status_icon, gpointer data);
/*Tray*/
static void mmgui_main_tray_icon_window_show_signal(GtkCheckMenuItem *checkmenuitem, gpointer data);
static void mmgui_main_tray_icon_new_sms_signal(GtkMenuItem *menuitem, gpointer data);
static void mmgui_main_tray_icon_exit_signal(GtkMenuItem *menuitem, gpointer data);
static void mmgui_main_tray_popup_menu_show_signal(GtkStatusIcon *status_icon, guint button, guint activate_time, gpointer data);
static gboolean mmgui_main_tray_tooltip_show_signal(GtkStatusIcon *status_icon, gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, gpointer data);
static void mmgui_main_tray_icon_init(mmgui_application_t mmguiapp);
/*Ayatana*/
static void mmgui_main_ayatana_event_callback(enum _mmgui_ayatana_event event, gpointer ayatana, gpointer data, gpointer userdata);
/*Initialization*/
static void mmgui_main_application_unresolved_error(mmgui_application_t mmguiapp, gchar *caption, gchar *text);
static gboolean mmgui_main_contacts_load_from_thread(gpointer data);
static gboolean mmgui_main_settings_ui_load(mmgui_application_t mmguiapp);
static gboolean mmgui_main_settings_load(mmgui_application_t mmguiapp);
static gboolean mmgui_main_application_build_user_interface(mmgui_application_t mmguiapp);
static void mmgui_main_application_terminate(mmgui_application_t mmguiapp);
static void mmgui_main_application_startup_signal(GtkApplication *application, gpointer data);
static void mmgui_main_continue_initialization(mmgui_application_t mmguiapp, mmguicore_t mmguicore);
static void mmgui_main_application_activate_signal(GtkApplication *application, gpointer data);
static void mmgui_main_application_shutdown_signal(GtkApplication *application, gpointer data);
static void mmgui_main_modules_list(mmgui_application_t mmguiapp);
#ifdef __GLIBC__
static void mmgui_main_application_backtrace_signal_handler(int sig, siginfo_t *info, ucontext_t *ucontext);
#endif
static void mmgui_main_application_termination_signal_handler(int sig, siginfo_t *info, ucontext_t *ucontext);

//EVENTS
static void mmgui_main_event_callback(enum _mmgui_event event, gpointer mmguicore, gpointer data, gpointer userdata)
{
	mmguidevice_t device;
	mmgui_application_t mmguiapp;
	mmgui_application_data_t appdata;
	guint id;
			
	mmguiapp = (mmgui_application_t)userdata;
		
	switch (event) {
		case MMGUI_EVENT_DEVICE_ADDED:
			appdata = g_new0(struct _mmgui_application_data, 1);
			appdata->mmguiapp = mmguiapp;
			appdata->data = data;
			g_idle_add(mmgui_main_device_handle_added_from_thread, appdata);
			break;
		case MMGUI_EVENT_DEVICE_REMOVED:
			appdata = g_new0(struct _mmgui_application_data, 1);
			appdata->mmguiapp = mmguiapp;
			appdata->data = data;
			g_idle_add(mmgui_main_device_handle_removed_from_thread, appdata);
			break;
		case MMGUI_EVENT_DEVICE_OPENED:
			mmguiapp->modemsettings = mmgui_modem_settings_open(mmguiapp->core->device->persistentid);
			/*SMS*/
			mmgui_main_sms_list_clear(mmguiapp);
			mmgui_main_sms_list_fill(mmguiapp);
			mmgui_main_sms_restore_settings_for_modem(mmguiapp);
			/*USSD*/
			mmgui_main_ussd_state_clear(mmguiapp);
			mmgui_main_ussd_restore_settings_for_modem(mmguiapp);
			/*Info*/
			mmgui_main_info_state_clear(mmguiapp);
			mmgui_main_info_update_for_modem(mmguiapp);
			/*Scan*/
			mmgui_main_scan_state_clear(mmguiapp);
			/*Traffic*/
			mmgui_main_traffic_restore_settings_for_modem(mmguiapp);
			/*Contacts*/
			mmgui_main_contacts_state_clear(mmguiapp);
			mmgui_main_contacts_list_fill(mmguiapp);
			/*Show network name*/
			g_idle_add(mmgui_main_ui_update_statusbar_from_thread, mmguiapp);
			break;
		case MMGUI_EVENT_DEVICE_CLOSING:
			mmgui_modem_settings_close(mmguiapp->modemsettings);
			break;
		case MMGUI_EVENT_DEVICE_ENABLED_STATUS:
			appdata = g_new0(struct _mmgui_application_data, 1);
			appdata->mmguiapp = mmguiapp;
			appdata->data = data;
			g_idle_add(mmgui_main_device_handle_enabled_status_from_thread, appdata);
			break;
		case MMGUI_EVENT_DEVICE_BLOCKED_STATUS:
			appdata = g_new0(struct _mmgui_application_data, 1);
			appdata->mmguiapp = mmguiapp;
			appdata->data = data;
			g_idle_add(mmgui_main_device_handle_blocked_status_from_thread, appdata);
			break;
		case MMGUI_EVENT_DEVICE_PREPARED_STATUS:
			appdata = g_new0(struct _mmgui_application_data, 1);
			appdata->mmguiapp = mmguiapp;
			appdata->data = data;
			g_idle_add(mmgui_main_device_handle_prepared_status_from_thread, appdata);
			break;
		case MMGUI_EVENT_DEVICE_CONNECTION_STATUS:
			appdata = g_new0(struct _mmgui_application_data, 1);
			appdata->mmguiapp = mmguiapp;
			appdata->data = data;
			g_idle_add(mmgui_main_device_handle_connection_status_from_thread, appdata);
			break;
		case MMGUI_EVENT_SIGNAL_LEVEL_CHANGE:
			device = (mmguidevice_t)data;
			mmgui_main_info_handle_signal_level_change(mmguiapp, device);
			/*Update level bars*/
			g_idle_add(mmgui_main_ui_update_statusbar_from_thread, mmguiapp);
			break;
		case MMGUI_EVENT_NETWORK_MODE_CHANGE:
			device = (mmguidevice_t)data;
			mmgui_main_info_handle_network_mode_change(mmguiapp, device);
			break;
		case MMGUI_EVENT_NETWORK_REGISTRATION_CHANGE:
			device = (mmguidevice_t)data;
			mmgui_main_info_handle_network_registration_change(mmguiapp, device);
			/*Show new network name*/
			g_idle_add(mmgui_main_ui_update_statusbar_from_thread, mmguiapp);
			break;
		case MMGUI_EVENT_LOCATION_CHANGE:
			device = (mmguidevice_t)data;
			mmgui_main_info_handle_location_change(mmguiapp, device);
			break;
		case MMGUI_EVENT_MODEM_ENABLE_RESULT:
			if (GPOINTER_TO_UINT(data)) {
				/*Update device partameters*/
				mmgui_main_device_handle_enabled_local_status(mmguiapp);
				mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_SUCCESS, NULL);
			} else {
				mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_FAIL, mmguicore_get_last_error(mmguiapp->core));
			}
			break;
		case MMGUI_EVENT_SCAN_RESULT:
			if (data != NULL) {
				/*Show found networks*/
				mmgui_main_scan_list_fill(mmguiapp, (mmguicore_t)mmguicore, (GSList *)data);
				mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_SUCCESS, NULL);
			} else {
				mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_FAIL, mmguicore_get_last_error(mmguiapp->core));
			}
			break;
		case MMGUI_EVENT_USSD_RESULT:
			if (data != NULL) {
				/*Show USSD answer*/
				mmgui_main_ussd_request_send_end(mmguiapp, (mmguicore_t)mmguicore, (const gchar *)data);
				mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_SUCCESS, NULL);
			} else {
				mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_FAIL, mmguicore_get_last_error(mmguiapp->core));
			}
			break;
		case MMGUI_EVENT_SMS_COMPLETED:
			appdata = g_new0(struct _mmgui_application_data, 1);
			appdata->mmguiapp = mmguiapp;
			appdata->data = data;
			g_idle_add(mmgui_main_sms_get_message_from_thread, appdata);
			break;
		case MMGUI_EVENT_SMS_LIST_READY:
			appdata = g_new0(struct _mmgui_application_data, 1);
			appdata->mmguiapp = mmguiapp;
			appdata->data = data;
			g_idle_add(mmgui_main_sms_get_message_list_from_thread, appdata);
			break;
		case MMGUI_EVENT_SMS_SENT:
			if (GPOINTER_TO_UINT(data)) {
				/*Better to save message here*/
				mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_SUCCESS, NULL);
			} else {
				mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_FAIL, mmguicore_get_last_error(mmguiapp->core));
			}
			break;
		case MMGUI_EVENT_SMS_NEW_DAY:
			g_idle_add(mmgui_main_sms_handle_new_day_from_thread, mmguiapp);
			break;
		case MMGUI_EVENT_NET_STATUS:
			g_idle_add(mmgui_main_ui_update_statusbar_from_thread, mmguiapp);
			g_idle_add(mmgui_main_traffic_stats_update_from_thread, mmguiapp);
			if (gtk_widget_get_visible(mmguiapp->window->trafficstatsdialog)) {
				g_idle_add(mmgui_main_traffic_stats_history_update_from_thread, mmguiapp);
			}
			break;
		case MMGUI_EVENT_TRAFFIC_LIMIT:
		case MMGUI_EVENT_TIME_LIMIT:
			appdata = g_new0(struct _mmgui_application_data, 1);
			appdata->mmguiapp = mmguiapp;
			appdata->data = GINT_TO_POINTER(event);
			g_idle_add(mmgui_main_traffic_limits_show_message_from_thread, appdata);
			break;
		case MMGUI_EVENT_UPDATE_CONNECTIONS_LIST:
			g_idle_add(mmgui_main_traffic_connections_update_from_thread, mmguiapp);
			break;
		case MMGUI_EVENT_EXTEND_CAPABILITIES:
			id = GPOINTER_TO_INT(data);
			mmgui_main_handle_extend_capabilities(mmguiapp, id);
			break;
		case MMGUI_EVENT_SERVICE_ACTIVATION_STARTED:
			mmgui_welcome_window_activation_page_open(mmguiapp);
			break;
		case MMGUI_EVENT_SERVICE_ACTIVATION_SERVICE_CHANGED:
			mmgui_welcome_window_activation_page_add_service(mmguiapp, (gchar *)data);
			break;
		case MMGUI_EVENT_SERVICE_ACTIVATION_SERVICE_ACTIVATED:
			mmgui_welcome_window_activation_page_set_state(mmguiapp, NULL);
			break;
		case MMGUI_EVENT_SERVICE_ACTIVATION_SERVICE_ERROR:
			mmgui_welcome_window_activation_page_set_state(mmguiapp, (gchar *)data);
			break;
		case MMGUI_EVENT_SERVICE_ACTIVATION_FINISHED:
			mmgui_welcome_window_close(mmguiapp);
			mmgui_main_continue_initialization(mmguiapp, mmguicore);
			break;
		case MMGUI_EVENT_SERVICE_ACTIVATION_AUTH_ERROR:
			mmgui_main_application_unresolved_error(mmguiapp, _("Error while initialization"), _("Unable to start needed system services without correct credentials"));
			break;
		case MMGUI_EVENT_SERVICE_ACTIVATION_STARTUP_ERROR:
			mmgui_main_application_unresolved_error(mmguiapp, _("Error while initialization"), _("Unable to communicate with available system services"));
			break;
		case MMGUI_EVENT_SERVICE_ACTIVATION_OTHER_ERROR:
			mmgui_main_application_unresolved_error(mmguiapp, _("Error while initialization"), (gchar *)data);
			break;
		case MMGUI_EVENT_ADDRESS_BOOKS_EXPORT_FINISHED:
			g_idle_add(mmgui_main_contacts_load_from_thread, userdata);
			break;
		default:
			g_debug("Unknown event (%u) got from core\n", event);
			break;
	}
	
}

static gboolean mmgui_main_handle_extend_capabilities(mmgui_application_t mmguiapp, gint id)
{
	if (mmguiapp == NULL) return FALSE;
	
	switch (id) {
		case MMGUI_CAPS_SMS:
			break;
		case MMGUI_CAPS_USSD:
			break;
		case MMGUI_CAPS_LOCATION:
			break;
		case MMGUI_CAPS_SCAN:
			break;
		case MMGUI_CAPS_CONTACTS:
			mmgui_main_contacts_list_fill(mmguiapp);
			mmgui_main_sms_restore_contacts_for_modem(mmguiapp);
			break;
		case MMGUI_CAPS_NONE:
		default:
			break;
	}
	
	return TRUE;
}

//UI
gboolean mmgui_main_ui_question_dialog_open(mmgui_application_t mmguiapp, gchar *caption, gchar *text)
{
	gint response;
	
	if ((mmguiapp == NULL) || (caption == NULL) || (text == NULL)) return FALSE;
	
	gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(mmguiapp->window->questiondialog), caption);
	gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(mmguiapp->window->questiondialog), "%s", text);
	
	response = gtk_dialog_run(GTK_DIALOG(mmguiapp->window->questiondialog));
	
	gtk_widget_hide(mmguiapp->window->questiondialog);
	
	return (response == GTK_RESPONSE_YES);
}

gboolean mmgui_main_ui_error_dialog_open(mmgui_application_t mmguiapp, gchar *caption, gchar *text)
{
	gint response;
	
	if ((mmguiapp == NULL) || (caption == NULL) || (text == NULL)) return FALSE;
	
	gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(mmguiapp->window->errordialog), caption);
	gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(mmguiapp->window->errordialog), "%s", text);
	
	response = gtk_dialog_run(GTK_DIALOG(mmguiapp->window->errordialog));
	
	gtk_widget_hide(mmguiapp->window->errordialog);
	
	return (response == GTK_RESPONSE_CLOSE);
}

static gboolean mmgui_ui_infobar_show_result_timer(gpointer data)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return G_SOURCE_REMOVE;
	
	/*Mark timer removed*/
	mmguiapp->window->infobartimeout = 0;
	
	/*Hide infobar*/
	gtk_widget_hide(mmguiapp->window->infobar);
	
	return G_SOURCE_REMOVE;
}

void mmgui_ui_infobar_show_result(mmgui_application_t mmguiapp, gint result, gchar *message)
{
	gchar *iconname, *defmessage, *resmessage;
	guint page;
	
	if (mmguiapp == NULL) return;
	
	switch (result) {
		case MMGUI_MAIN_INFOBAR_RESULT_SUCCESS:
			iconname = "emblem-default";
			defmessage = _("Success");
			mmguiapp->window->infobartimeout = g_timeout_add_seconds(2, mmgui_ui_infobar_show_result_timer, mmguiapp);
			break;
		case MMGUI_MAIN_INFOBAR_RESULT_FAIL:
			iconname = "emblem-important";
			defmessage = _("Failed");
			mmguiapp->window->infobartimeout = g_timeout_add_seconds(2, mmgui_ui_infobar_show_result_timer, mmguiapp);
			break;
		case MMGUI_MAIN_INFOBAR_RESULT_TIMEOUT:
			iconname = "appointment-missed";
			defmessage = _("Timeout");
			mmguiapp->window->infobartimeout = g_timeout_add_seconds(2, mmgui_ui_infobar_show_result_timer, mmguiapp);
			break;
		case MMGUI_MAIN_INFOBAR_RESULT_INTERRUPT:
		default:
			/*Hide infobar instantly*/
			gtk_widget_hide(mmguiapp->window->infobar);
			return;
	}
	
	/*Show result in infobar*/
	gtk_image_set_from_icon_name(GTK_IMAGE(mmguiapp->window->infobarimage), iconname, GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_visible(mmguiapp->window->infobarimage, TRUE);
	gtk_widget_set_visible(mmguiapp->window->infobarspinner, FALSE);
	gtk_spinner_stop(GTK_SPINNER(mmguiapp->window->infobarspinner));
	if (message != NULL) {
		resmessage = g_strdup_printf("%s (%s)", gtk_label_get_label(GTK_LABEL(mmguiapp->window->infobarlabel)), message);
	} else {
		resmessage = g_strdup_printf("%s (%s)", gtk_label_get_label(GTK_LABEL(mmguiapp->window->infobarlabel)), defmessage);
	}
	gtk_label_set_label(GTK_LABEL(mmguiapp->window->infobarlabel), resmessage);
	g_free(resmessage);
	gtk_widget_hide(mmguiapp->window->infobarstopbutton);
	
	/*Show infobar if not visible*/
	if (!gtk_widget_get_visible(mmguiapp->window->infobar)) {
		gtk_widget_show(mmguiapp->window->infobar);
	}
	
	/*Unblock controls*/
	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiapp->window->notebook));
	mmgui_main_ui_test_device_state(mmguiapp, page);
	/*Toolbar*/
	gtk_widget_set_sensitive(mmguiapp->window->devbutton, TRUE);
	gtk_widget_set_sensitive(mmguiapp->window->smsbutton, TRUE);
	gtk_widget_set_sensitive(mmguiapp->window->ussdbutton, TRUE);
	gtk_widget_set_sensitive(mmguiapp->window->infobutton, TRUE);
	gtk_widget_set_sensitive(mmguiapp->window->scanbutton, TRUE);
	gtk_widget_set_sensitive(mmguiapp->window->trafficbutton, TRUE);
	gtk_widget_set_sensitive(mmguiapp->window->contactsbutton, TRUE);
	/*Application menu*/
	mmgui_main_ui_application_menu_set_state(mmguiapp, TRUE);
}

static gboolean mmgui_ui_infobar_timeout_timer(gpointer data)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return G_SOURCE_REMOVE;
	
	if (mmguicore_interrupt_operation(mmguiapp->core)) {
		mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_TIMEOUT, NULL);
	}
	
	/*Mark timer removed*/
	mmguiapp->window->infobartimeout = 0;
	
	return G_SOURCE_REMOVE;
}

void mmgui_ui_infobar_show(mmgui_application_t mmguiapp, gchar *message, gint type)
{
	gchar *iconname;
	GtkMessageType msgtype;
	guint page;
	
	if ((mmguiapp == NULL) || (message == NULL)) return;
	
	/*First of all remove timeout timer (if any)*/
	if (mmguiapp->window->infobartimeout > 0) {
		g_source_remove(mmguiapp->window->infobartimeout);
		mmguiapp->window->infobartimeout = 0;
	}
	
	/*Pick infobar parameters*/
	switch (type) {
		case MMGUI_MAIN_INFOBAR_TYPE_INFO:
			iconname = "dialog-information";
			msgtype = GTK_MESSAGE_INFO;
			break;
		case MMGUI_MAIN_INFOBAR_TYPE_WARNING:
			iconname = "dialog-warning";
			msgtype = GTK_MESSAGE_WARNING;
			break;
		case MMGUI_MAIN_INFOBAR_TYPE_ERROR:
			iconname = "dialog-error";
			msgtype = GTK_MESSAGE_ERROR;
			break;
		default:
			iconname = NULL;
			msgtype = GTK_MESSAGE_INFO;
			break;
	}
	
	/*Show needed widgets*/
	if (type == MMGUI_MAIN_INFOBAR_TYPE_PROGRESS) {
		/*Block controls*/
		page = gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiapp->window->notebook));
		mmgui_main_ui_page_control_disable(mmguiapp, page, TRUE, FALSE);
		/*Toolbar*/
		gtk_widget_set_sensitive(mmguiapp->window->devbutton, FALSE);
		gtk_widget_set_sensitive(mmguiapp->window->smsbutton, FALSE);
		gtk_widget_set_sensitive(mmguiapp->window->ussdbutton, FALSE);
		gtk_widget_set_sensitive(mmguiapp->window->infobutton, FALSE);
		gtk_widget_set_sensitive(mmguiapp->window->scanbutton, FALSE);
		gtk_widget_set_sensitive(mmguiapp->window->trafficbutton, FALSE);
		gtk_widget_set_sensitive(mmguiapp->window->contactsbutton, FALSE);
		/*Application menu*/
		mmgui_main_ui_application_menu_set_state(mmguiapp, FALSE);
		/*Prepare infobar*/
		gtk_info_bar_set_message_type(GTK_INFO_BAR(mmguiapp->window->infobar), msgtype);
		gtk_widget_set_visible(mmguiapp->window->infobarimage, FALSE);
		gtk_widget_set_visible(mmguiapp->window->infobarspinner, TRUE);
		gtk_spinner_start(GTK_SPINNER(mmguiapp->window->infobarspinner));
		gtk_widget_show(mmguiapp->window->infobarstopbutton);
		/*Set new timeout timer */
		mmguiapp->window->infobartimeout = g_timeout_add_seconds(MMGUI_MAIN_OPERATION_TIMEOUT, mmgui_ui_infobar_timeout_timer, mmguiapp);
	} else {
		gtk_image_set_from_icon_name(GTK_IMAGE(mmguiapp->window->infobarimage), iconname, GTK_ICON_SIZE_BUTTON);
		gtk_info_bar_set_message_type(GTK_INFO_BAR(mmguiapp->window->infobar), msgtype);
		gtk_widget_set_visible(mmguiapp->window->infobarimage, TRUE);
		gtk_widget_set_visible(mmguiapp->window->infobarspinner, FALSE);
		gtk_spinner_stop(GTK_SPINNER(mmguiapp->window->infobarspinner));
		gtk_widget_hide(mmguiapp->window->infobarstopbutton);
	}
	
	/*Set infobar label text*/
	gtk_widget_set_visible(mmguiapp->window->infobarlabel, TRUE);
	gtk_label_set_text(GTK_LABEL(mmguiapp->window->infobarlabel), message);
	
	/*Show infobar*/
	gtk_widget_show(mmguiapp->window->infobar);
}

void mmgui_ui_infobar_process_stop_signal(GtkInfoBar *info_bar, gint response_id, gpointer data)
{
	mmgui_application_t mmguiapp;
	guint page;
			
	mmguiapp = (mmgui_application_t)data;
	
	if ((mmguiapp == NULL) || (response_id != GTK_RESPONSE_CLOSE)) return;
	
	/*Remove timeout timer*/
	if (mmguiapp->window->infobartimeout > 0) {
		g_source_remove(mmguiapp->window->infobartimeout);
		mmguiapp->window->infobartimeout = 0;
	}
	
	/*Interrupt operation and hide infobar*/
	if (mmguicore_interrupt_operation(mmguiapp->core)) {
		mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_INTERRUPT, NULL);
		/*Unblock controls*/
		page = gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiapp->window->notebook));
		mmgui_main_ui_test_device_state(mmguiapp, page);
		/*Toolbar*/
		gtk_widget_set_sensitive(mmguiapp->window->devbutton, TRUE);
		gtk_widget_set_sensitive(mmguiapp->window->smsbutton, TRUE);
		gtk_widget_set_sensitive(mmguiapp->window->ussdbutton, TRUE);
		gtk_widget_set_sensitive(mmguiapp->window->infobutton, TRUE);
		gtk_widget_set_sensitive(mmguiapp->window->scanbutton, TRUE);
		gtk_widget_set_sensitive(mmguiapp->window->trafficbutton, TRUE);
		gtk_widget_set_sensitive(mmguiapp->window->contactsbutton, TRUE);
		/*Application menu*/
		mmgui_main_ui_application_menu_set_state(mmguiapp, TRUE);
	}
}

static void mmgui_main_ui_page_control_disable(mmgui_application_t mmguiapp, guint page, gboolean disable, gboolean onlylimited)
{
	if ((mmguiapp == NULL) || (page > MMGUI_MAIN_PAGE_CONTACTS)) return;
	
	switch (page) {
		case MMGUI_MAIN_PAGE_DEVICES:
			break;
		case MMGUI_MAIN_PAGE_SMS:
			gtk_widget_set_sensitive(mmguiapp->window->newsmsbutton, !disable);
			break;
		case MMGUI_MAIN_PAGE_USSD:
			gtk_widget_set_sensitive(mmguiapp->window->ussdentry, !disable);
			gtk_widget_set_sensitive(mmguiapp->window->ussdcombobox, !disable);
			gtk_widget_set_sensitive(mmguiapp->window->ussdeditor, !disable);
			gtk_widget_set_sensitive(mmguiapp->window->ussdsend, !disable);
			if (!disable) {
				g_signal_emit_by_name(G_OBJECT(mmguiapp->window->ussdentry), "changed", mmguiapp);
			}
			break;
		case MMGUI_MAIN_PAGE_INFO:
			break;
		case MMGUI_MAIN_PAGE_SCAN:
			gtk_widget_set_sensitive(mmguiapp->window->startscanbutton, !disable);
			break;
		case MMGUI_MAIN_PAGE_TRAFFIC:
			break;
		case MMGUI_MAIN_PAGE_CONTACTS:
			gtk_widget_set_sensitive(mmguiapp->window->newcontactbutton, !disable);
			break;
		default:
			break;
	}
}

static void mmgui_main_ui_page_setup_shortcuts(mmgui_application_t mmguiapp, guint setpage)
{
	GSList *iterator;
	GClosure *closure;
	
	if (mmguiapp == NULL) return;
	
	if (mmguiapp->window->pageshortcuts != NULL) {
		for (iterator=mmguiapp->window->pageshortcuts; iterator; iterator=iterator->next) {
			closure = (GClosure *)iterator->data;
			if (closure != NULL) {
				g_closure_ref(closure);
				gtk_accel_group_disconnect(mmguiapp->window->accelgroup, closure);
			}
		}
		g_slist_free(mmguiapp->window->pageshortcuts);
		mmguiapp->window->pageshortcuts = NULL;
	}
	
	switch (setpage) {
		case MMGUI_MAIN_PAGE_DEVICES:
			break;
		case MMGUI_MAIN_PAGE_SMS:
			/*send sms message*/
			gtk_accel_group_connect(mmguiapp->window->accelgroup, GDK_KEY_N, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE, mmguiapp->window->newsmsclosure);
			mmguiapp->window->pageshortcuts = g_slist_prepend(mmguiapp->window->pageshortcuts, mmguiapp->window->newsmsclosure);
			/*remove sms message*/
			gtk_accel_group_connect(mmguiapp->window->accelgroup, GDK_KEY_D, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE, mmguiapp->window->removesmsclosure);
			mmguiapp->window->pageshortcuts = g_slist_prepend(mmguiapp->window->pageshortcuts, mmguiapp->window->removesmsclosure);
			/*answer sms message*/
			gtk_accel_group_connect(mmguiapp->window->accelgroup, GDK_KEY_A, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE, mmguiapp->window->answersmsclosure);
			mmguiapp->window->pageshortcuts = g_slist_prepend(mmguiapp->window->pageshortcuts, mmguiapp->window->answersmsclosure);
			break;
		case MMGUI_MAIN_PAGE_USSD:
			/*edit ussd commands*/
			gtk_accel_group_connect(mmguiapp->window->accelgroup, GDK_KEY_E, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE, mmguiapp->window->ussdeditorclosure);
			mmguiapp->window->pageshortcuts = g_slist_prepend(mmguiapp->window->pageshortcuts, mmguiapp->window->ussdeditorclosure);
			/*send ussd request*/
			gtk_accel_group_connect(mmguiapp->window->accelgroup, GDK_KEY_S, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE, mmguiapp->window->ussdsendclosure);
			mmguiapp->window->pageshortcuts = g_slist_prepend(mmguiapp->window->pageshortcuts, mmguiapp->window->ussdsendclosure);
			break;
		case MMGUI_MAIN_PAGE_INFO:
			break;
		case MMGUI_MAIN_PAGE_SCAN:
			/*scan networks*/
			gtk_accel_group_connect(mmguiapp->window->accelgroup, GDK_KEY_S, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE, mmguiapp->window->startscanclosure);
			mmguiapp->window->pageshortcuts = g_slist_prepend(mmguiapp->window->pageshortcuts, mmguiapp->window->startscanclosure);
			break;
		case MMGUI_MAIN_PAGE_TRAFFIC:
			/*limits*/
			gtk_accel_group_connect(mmguiapp->window->accelgroup, GDK_KEY_L, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE, mmguiapp->window->trafficlimitclosure);
			mmguiapp->window->pageshortcuts = g_slist_prepend(mmguiapp->window->pageshortcuts, mmguiapp->window->trafficlimitclosure);
			/*connections*/
			gtk_accel_group_connect(mmguiapp->window->accelgroup, GDK_KEY_C, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE, mmguiapp->window->trafficconnclosure);
			mmguiapp->window->pageshortcuts = g_slist_prepend(mmguiapp->window->pageshortcuts, mmguiapp->window->trafficconnclosure);
			/*statistics*/
			gtk_accel_group_connect(mmguiapp->window->accelgroup, GDK_KEY_S, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE, mmguiapp->window->trafficstatsclosure);
			mmguiapp->window->pageshortcuts = g_slist_prepend(mmguiapp->window->pageshortcuts, mmguiapp->window->trafficstatsclosure);
			break;
		case MMGUI_MAIN_PAGE_CONTACTS:
			/*add contact*/
			gtk_accel_group_connect(mmguiapp->window->accelgroup, GDK_KEY_N, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE, mmguiapp->window->newcontactclosure);
			mmguiapp->window->pageshortcuts = g_slist_prepend(mmguiapp->window->pageshortcuts, mmguiapp->window->newcontactclosure);
			/*remove contact*/
			gtk_accel_group_connect(mmguiapp->window->accelgroup, GDK_KEY_D, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE, mmguiapp->window->removecontactclosure);
			mmguiapp->window->pageshortcuts = g_slist_prepend(mmguiapp->window->pageshortcuts, mmguiapp->window->removecontactclosure);
			/*send sms*/
			gtk_accel_group_connect(mmguiapp->window->accelgroup, GDK_KEY_S, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE, mmguiapp->window->smstocontactclosure);
			mmguiapp->window->pageshortcuts = g_slist_prepend(mmguiapp->window->pageshortcuts, mmguiapp->window->smstocontactclosure);
			break;
		default:
			break;
	}
}

static void mmgui_main_ui_page_use_shortcuts_signal(gpointer data)
{
	mmgui_application_data_t appdata;
	guint shortcut, operation, setpage, pagecaps, suppcaps;
	gboolean enabled, blocked, connected;
	
	appdata = (mmgui_application_data_t)data;
	
	if (appdata == NULL) return;
	
	shortcut = GPOINTER_TO_UINT(appdata->data);
	operation = mmguicore_devices_get_current_operation(appdata->mmguiapp->core);
	
	blocked = mmguicore_devices_get_locked(appdata->mmguiapp->core);
	enabled = mmguicore_devices_get_enabled(appdata->mmguiapp->core);
	connected = mmguicore_devices_get_connected(appdata->mmguiapp->core); 
	
	setpage = gtk_notebook_get_current_page(GTK_NOTEBOOK(appdata->mmguiapp->window->notebook));
	
	switch (setpage) {
		case MMGUI_MAIN_PAGE_DEVICES:
			break;
		case MMGUI_MAIN_PAGE_SMS:
			if ((enabled) && (!blocked)) {
				pagecaps = mmguicore_sms_get_capabilities(appdata->mmguiapp->core);
				/*send sms message*/
				if ((pagecaps & MMGUI_SMS_CAPS_SEND) && (operation == MMGUI_DEVICE_OPERATION_IDLE)) {
					if (shortcut == MMGUI_MAIN_CONTROL_SHORTCUT_SMS_NEW) {
						mmgui_main_sms_new(appdata->mmguiapp);
					}
					/*answer sms message*/
					if (shortcut == MMGUI_MAIN_CONTROL_SHORTCUT_SMS_ANSWER) {
						mmgui_main_sms_answer(appdata->mmguiapp);
					}
				}
				/*remove sms message*/
				if (shortcut == MMGUI_MAIN_CONTROL_SHORTCUT_SMS_REMOVE) {
					mmgui_main_sms_remove(appdata->mmguiapp);
				}
			}
			break;
		case MMGUI_MAIN_PAGE_USSD:
			if ((enabled) && (!blocked)) {
				pagecaps = mmguicore_ussd_get_capabilities(appdata->mmguiapp->core);
				if ((pagecaps & MMGUI_USSD_CAPS_SEND) && (operation == MMGUI_DEVICE_OPERATION_IDLE)) {
					/*send ussd request*/
					if (shortcut == MMGUI_MAIN_CONTROL_SHORTCUT_USSD_SEND) {
						mmgui_main_ussd_request_send(appdata->mmguiapp);
					}
					/*edit ussd commands*/
					if (shortcut == MMGUI_MAIN_CONTROL_SHORTCUT_USSD_EDITOR) {
						mmgui_main_ussd_edit(appdata->mmguiapp);
					}
				}
			}
			break;
		case MMGUI_MAIN_PAGE_INFO:
			break;
		case MMGUI_MAIN_PAGE_SCAN:
			if ((enabled) && (!blocked)) {
				pagecaps = mmguicore_newtworks_scan_get_capabilities(appdata->mmguiapp->core);
				if ((pagecaps & MMGUI_SCAN_CAPS_OBSERVE) && (operation == MMGUI_DEVICE_OPERATION_IDLE) && (!connected)) {
					/*scan networks*/
					if (shortcut == MMGUI_MAIN_CONTROL_SHORTCUT_SCAN_START) {
						mmgui_main_scan_start(appdata->mmguiapp);
					}
				}
			}
			break;
		case MMGUI_MAIN_PAGE_TRAFFIC:
			/*limits*/
			if (shortcut == MMGUI_MAIN_CONTROL_SHORTCUT_TRAFFIC_LIMIT) {
				mmgui_main_traffic_limits_dialog(appdata->mmguiapp);
			}
			/*connections*/
			if (shortcut == MMGUI_MAIN_CONTROL_SHORTCUT_TRAFFIC_CONNECTIONS) {
				mmgui_main_traffic_connections_dialog(appdata->mmguiapp);
			}
			/*statistics*/
			if (shortcut == MMGUI_MAIN_CONTROL_SHORTCUT_TRAFFIC_STATS) {
				mmgui_main_traffic_statistics_dialog(appdata->mmguiapp);
			}
			break;
		case MMGUI_MAIN_PAGE_CONTACTS:
			if ((enabled) && (!blocked)) {
				pagecaps = mmguicore_contacts_get_capabilities(appdata->mmguiapp->core);
				suppcaps = mmguicore_sms_get_capabilities(appdata->mmguiapp->core);
				if (pagecaps & MMGUI_CONTACTS_CAPS_EDIT) {
					/*add contact*/
					if (shortcut == MMGUI_MAIN_CONTROL_SHORTCUT_CONTACTS_NEW) {
						mmgui_main_contacts_new(appdata->mmguiapp);
					}
					/*remove contact*/
					if (shortcut == MMGUI_MAIN_CONTROL_SHORTCUT_CONTACTS_REMOVE) {
						mmgui_main_contacts_remove(appdata->mmguiapp);
					}
				}
				if (suppcaps & MMGUI_SMS_CAPS_SEND) {
					/*send sms*/
					if (shortcut == MMGUI_MAIN_CONTROL_SHORTCUT_CONTACTS_SMS) {
						mmgui_main_contacts_sms(appdata->mmguiapp);
					}
				}
			}
			break;
		default:
			break;
	}
}

gboolean mmgui_main_ui_test_device_state(mmgui_application_t mmguiapp, guint setpage)
{
	gboolean trytoenable, trytounlock, nonfunctional, limfunctional, needreg, enabled, locked, registered, prepared;
	gchar *enablemessage, *nonfuncmessage, *limfuncmessage, *prepmessage, *regmessage, *lockedmessage, *notenabledmessage;
	guint pagecaps;
	
	if (mmguiapp == NULL) return FALSE;
	
	locked = mmguicore_devices_get_locked(mmguiapp->core);
	enabled = mmguicore_devices_get_enabled(mmguiapp->core);
	registered = mmguicore_devices_get_registered(mmguiapp->core);
	prepared = mmguicore_devices_get_prepared(mmguiapp->core);
	
	/*Common messages*/
	prepmessage = _("Modem is not ready for operation. Please wait while modem being prepared...");
	enablemessage = NULL;
	nonfuncmessage = NULL;
	limfuncmessage = NULL;
	regmessage = NULL;
	lockedmessage = NULL;
	notenabledmessage = NULL;
	
	switch (setpage) {
		case MMGUI_MAIN_PAGE_DEVICES:
			trytoenable = FALSE;
			trytounlock = FALSE;
			needreg = FALSE;
			enablemessage = NULL;
			notenabledmessage = NULL;
			regmessage = NULL;
			lockedmessage = NULL;
			nonfuncmessage = NULL;
			limfuncmessage = NULL;
			nonfunctional = FALSE;
			limfunctional = FALSE;
			break;
		case MMGUI_MAIN_PAGE_SMS:
			trytoenable = TRUE;
			trytounlock = TRUE;
			needreg = TRUE;
			enablemessage = _("Modem must be enabled to read SMS. Enable modem?");
			notenabledmessage = _("Modem must be enabled to read and write SMS. Please enable modem.");
			regmessage = _("Modem must be registered in mobile network to receive and send SMS. Please wait...");
			lockedmessage = _("Modem must be unlocked to receive and send SMS. Please enter PIN code.");
			nonfuncmessage = _("Modem manager does not support SMS manipulation functions.");
			limfuncmessage = _("Modem manager does not support sending of SMS messages.");
			pagecaps = mmguicore_sms_get_capabilities(mmguiapp->core);
			if (pagecaps & MMGUI_SMS_CAPS_RECEIVE) {
				nonfunctional = FALSE;
			} else {
				nonfunctional = TRUE;
			}
			if (pagecaps & MMGUI_SMS_CAPS_SEND) {
				limfunctional = FALSE;
			} else {
				limfunctional = TRUE;
			}
			break;
		case MMGUI_MAIN_PAGE_USSD:
			trytoenable = TRUE;
			trytounlock = TRUE;
			needreg = TRUE;
			enablemessage = _("Modem must be enabled to send USSD. Enable modem?");
			notenabledmessage =  _("Modem must be enabled to send USSD. Please enable modem.");
			regmessage = _("Modem must be registered in mobile network to send USSD. Please wait...");
			lockedmessage = _("Modem must be unlocked to send USSD. Please enter PIN code.");
			nonfuncmessage = _("Modem manager does not support sending of USSD requests.");
			limfuncmessage = NULL;
			pagecaps = mmguicore_ussd_get_capabilities(mmguiapp->core);
			if (pagecaps & MMGUI_USSD_CAPS_SEND) {
				nonfunctional = FALSE;
			} else {
				nonfunctional = TRUE;
			}
			limfunctional = FALSE;
			break;
		case MMGUI_MAIN_PAGE_INFO:
			trytoenable = FALSE;
			trytounlock = FALSE;
			needreg = FALSE;
			enablemessage = NULL;
			regmessage = NULL;
			lockedmessage = NULL;
			nonfuncmessage = NULL;
			limfuncmessage = NULL;
			nonfunctional = FALSE;
			limfunctional = FALSE;
			break;
		case MMGUI_MAIN_PAGE_SCAN:
			trytoenable = TRUE;
			trytounlock = TRUE;
			needreg = FALSE;
			enablemessage = _("Modem must be enabled to scan for available networks. Enable modem?");
			notenabledmessage = _("Modem must be enabled to scan for available networks. Please enable modem.");
			regmessage = NULL;
			lockedmessage = _("Modem must be unlocked to scan for available networks. Please enter PIN code.");
			nonfuncmessage = _("Modem manager does not support scanning for available mobile networks.");
			limfuncmessage = _("Modem is connected now. Please disconnect to scan.");
			pagecaps = mmguicore_newtworks_scan_get_capabilities(mmguiapp->core);
			if (pagecaps & MMGUI_SCAN_CAPS_OBSERVE) {
				nonfunctional = FALSE;
			} else {
				nonfunctional = TRUE;
			}
			if (mmguicore_devices_get_connected(mmguiapp->core)) {
				limfunctional = TRUE;
			} else {
				limfunctional = FALSE;
			}
			break;
		case MMGUI_MAIN_PAGE_TRAFFIC:
			trytoenable = FALSE;
			trytounlock = FALSE;
			needreg = FALSE;
			enablemessage = NULL;
			notenabledmessage = NULL;
			regmessage = NULL;
			lockedmessage = NULL;
			nonfuncmessage = NULL;
			limfuncmessage = NULL;
			nonfunctional = FALSE;
			limfunctional = FALSE;
			break;
		case MMGUI_MAIN_PAGE_CONTACTS:
			trytoenable = TRUE;
			trytounlock = TRUE;
			needreg = FALSE;
			enablemessage = _("Modem must be enabled to export contacts from it. Enable modem?");
			notenabledmessage = _("Modem must be enabled to export contacts from it. Please enable modem.");
			regmessage = NULL;
			lockedmessage = _("Modem must be unlocked to export contacts from it. Please enter PIN code.");
			nonfuncmessage = _("Modem manager does not support modem contacts manipulation functions.");
			limfuncmessage = _("Modem manager does not support modem contacts edition functions.");
			pagecaps = mmguicore_contacts_get_capabilities(mmguiapp->core);
			if (pagecaps & MMGUI_CONTACTS_CAPS_EXPORT) {
				nonfunctional = FALSE;
			} else {
				nonfunctional = TRUE;
			}
			if (pagecaps & MMGUI_CONTACTS_CAPS_EDIT) {
				limfunctional = FALSE;
			} else {
				limfunctional = TRUE;
			}
			break;
		default:
			trytoenable = FALSE;
			trytounlock = FALSE;
			needreg = FALSE;
			enablemessage = NULL;
			notenabledmessage = NULL;
			regmessage = NULL;
			lockedmessage = NULL;
			nonfuncmessage = NULL;
			limfuncmessage = NULL;
			prepmessage = NULL;
			nonfunctional = FALSE;
			limfunctional = FALSE;
			break;
	}
	
	if (!prepared) {
		g_debug("Not prepared\n");
		mmgui_ui_infobar_show(mmguiapp, prepmessage, MMGUI_MAIN_INFOBAR_TYPE_INFO);
		mmgui_main_ui_page_control_disable(mmguiapp, setpage, TRUE, FALSE);
	} else if (locked) {
		g_debug("SIM locked\n");
		if (trytounlock) {
			mmgui_ui_infobar_show(mmguiapp, lockedmessage, MMGUI_MAIN_INFOBAR_TYPE_INFO);
		} else {
			if (mmguiapp->window->infobartimeout == 0) {
				mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_INTERRUPT, NULL);
			}
		}
		mmgui_main_ui_page_control_disable(mmguiapp, setpage, TRUE, FALSE);
	} else {
		if ((trytoenable) && (!enabled)) {
			g_debug("Must be enabled\n");
			if (mmgui_main_ui_question_dialog_open(mmguiapp, _("<b>Enable modem</b>"), enablemessage)) {
				if (mmguicore_devices_enable(mmguiapp->core, TRUE)) {
					mmgui_ui_infobar_show(mmguiapp, _("Enabling device..."), MMGUI_MAIN_INFOBAR_TYPE_PROGRESS);
				} else {
					mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_FAIL, mmguicore_get_last_error(mmguiapp->core));
				}
			} else {
				g_debug("Not enabled\n");
				mmgui_ui_infobar_show(mmguiapp, notenabledmessage, MMGUI_MAIN_INFOBAR_TYPE_INFO);
				mmgui_main_ui_page_control_disable(mmguiapp, setpage, TRUE, TRUE);
			}
		} else {
			if ((needreg) && (!registered)) {
				g_debug("Must be registered\n");
				mmgui_ui_infobar_show(mmguiapp, regmessage, MMGUI_MAIN_INFOBAR_TYPE_INFO);
				mmgui_main_ui_page_control_disable(mmguiapp, setpage, TRUE, TRUE);
			} else if (nonfunctional) {
				g_debug("Nonfunctional\n");
				mmgui_ui_infobar_show(mmguiapp, nonfuncmessage, MMGUI_MAIN_INFOBAR_TYPE_INFO);
				mmgui_main_ui_page_control_disable(mmguiapp, setpage, TRUE, FALSE);
			} else if (limfunctional) {
				g_debug("Limited functional\n");
				mmgui_ui_infobar_show(mmguiapp, limfuncmessage, MMGUI_MAIN_INFOBAR_TYPE_INFO);
				mmgui_main_ui_page_control_disable(mmguiapp, setpage, TRUE, TRUE);
			} else {
				g_debug("Fully functional\n");
				/*Do not hide infobar with status information*/
				if (mmguiapp->window->infobartimeout == 0) {
					mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_INTERRUPT, NULL);
				}
				mmgui_main_ui_page_control_disable(mmguiapp, setpage, FALSE, FALSE);
			}
		}
	}
	
	return TRUE;
}

static void mmgui_main_ui_open_page(mmgui_application_t mmguiapp, guint page)
{
	if ((mmguiapp == NULL) || (page > MMGUI_MAIN_PAGE_CONTACTS)) return;
	
	if ((page != MMGUI_MAIN_PAGE_DEVICES) && (mmguicore_devices_get_current(mmguiapp->core) == NULL)) return;
	
	/*Test device state*/
	mmgui_main_ui_test_device_state(mmguiapp, page);
	/*Bind shortcuts*/
	mmgui_main_ui_page_setup_shortcuts(mmguiapp, page);
	/*Open page*/
	gtk_notebook_set_current_page(GTK_NOTEBOOK(mmguiapp->window->notebook), page);
	/*Set section in application main menu*/
	mmgui_main_ui_application_menu_set_page(mmguiapp, page);
}

static void mmgui_main_ui_application_menu_set_page(mmgui_application_t mmguiapp, guint page)
{
	GAction *action;
	GVariant *sectionv;
		
	if ((mmguiapp == NULL) || (page > MMGUI_MAIN_PAGE_CONTACTS)) return;
	
	action = g_action_map_lookup_action(G_ACTION_MAP(mmguiapp->gtkapplication), "section");
	
	if (action == NULL) return;
	
	switch (page) {
		case MMGUI_MAIN_PAGE_DEVICES:
			sectionv = g_variant_new_string("devices");
			break;
		case MMGUI_MAIN_PAGE_SMS:
			sectionv = g_variant_new_string("sms");
			break;
		case MMGUI_MAIN_PAGE_USSD:
			sectionv = g_variant_new_string("ussd");
			break;
		case MMGUI_MAIN_PAGE_INFO:
			sectionv = g_variant_new_string("info");
			break;
		case MMGUI_MAIN_PAGE_SCAN:
			sectionv = g_variant_new_string("scan");
			break;
		case MMGUI_MAIN_PAGE_TRAFFIC:
			sectionv = g_variant_new_string("traffic");
			break;
		case MMGUI_MAIN_PAGE_CONTACTS:
			sectionv = g_variant_new_string("contacts");
			break;
		default:
			sectionv = g_variant_new_string("devices");
			break;
	}
	
	/*Set action in application menu*/
	g_simple_action_set_state(G_SIMPLE_ACTION(action), sectionv);
}

static void mmgui_main_ui_application_menu_set_state(mmgui_application_t mmguiapp, gboolean enabled)
{
	GAction *action;
		
	if (mmguiapp == NULL) return;
	
	action = g_action_map_lookup_action(G_ACTION_MAP(mmguiapp->gtkapplication), "section");
	
	if (action == NULL) return;
	
	/*Set state of section in application menu application menu*/
	g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);
}

void mmgui_main_ui_devices_button_toggled_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(object))) {
		mmgui_main_ui_open_page(mmguiapp, MMGUI_MAIN_PAGE_DEVICES);
	}
}

void mmgui_main_ui_sms_button_toggled_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(object))) {
		mmgui_main_ui_open_page(mmguiapp, MMGUI_MAIN_PAGE_SMS);
	}
}

void mmgui_main_ui_ussd_button_toggled_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(object))) {
		mmgui_main_ui_open_page(mmguiapp, MMGUI_MAIN_PAGE_USSD);
	}
}

void mmgui_main_ui_info_button_toggled_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(object))) {
		mmgui_main_ui_open_page(mmguiapp, MMGUI_MAIN_PAGE_INFO);
	}
}

void mmgui_main_ui_scan_button_toggled_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(object))) {
		mmgui_main_ui_open_page(mmguiapp, MMGUI_MAIN_PAGE_SCAN);
	}
}

void mmgui_main_ui_traffic_button_toggled_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(object))) {
		mmgui_main_ui_open_page(mmguiapp, MMGUI_MAIN_PAGE_TRAFFIC);
	}
}

void mmgui_main_ui_contacts_button_toggled_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(object))) {
		mmgui_main_ui_open_page(mmguiapp, MMGUI_MAIN_PAGE_CONTACTS);
	}
}

static enum _mmgui_main_exit_dialog_result mmgui_main_ui_window_hide_dialog(mmgui_application_t mmguiapp)
{
	gint response;
	/*gchar *strcolor;*/
	
	if (mmguiapp == NULL) return MMGUI_MAIN_EXIT_DIALOG_CANCEL;
	if ((mmguiapp->options == NULL) || (mmguiapp->settings == NULL)) return MMGUI_MAIN_EXIT_DIALOG_CANCEL;
	
	response = gtk_dialog_run(GTK_DIALOG(mmguiapp->window->exitdialog));
	
	gtk_widget_hide(mmguiapp->window->exitdialog);
	
	if (response > 0) {
		/*Ask again checkbox*/
		mmguiapp->options->askforhide = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->exitaskagain));
		gmm_settings_set_boolean(mmguiapp->settings, "behaviour_ask_to_hide", mmguiapp->options->askforhide);
		/*Exit and hide radiobuttons*/
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->exitcloseradio))) {
			/*Exit application selected*/
			mmguiapp->options->hidetotray = FALSE;
			gmm_settings_set_boolean(mmguiapp->settings, "behaviour_hide_to_tray", mmguiapp->options->hidetotray);
			return MMGUI_MAIN_EXIT_DIALOG_EXIT;
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->exithideradio))) {
			/*Hide to tray selected*/
			mmguiapp->options->hidetotray = TRUE;
			gmm_settings_set_boolean(mmguiapp->settings, "behaviour_hide_to_tray", mmguiapp->options->hidetotray);
			return MMGUI_MAIN_EXIT_DIALOG_HIDE;
		} else {
			/*Cancel clicked*/
			return MMGUI_MAIN_EXIT_DIALOG_CANCEL;
		}
	} else {
		return MMGUI_MAIN_EXIT_DIALOG_CANCEL;
	}
}

static void mmgui_main_ui_window_save_geometry(mmgui_application_t mmguiapp)
{
	if (mmguiapp == NULL) return;
	
	if (mmguiapp->options->savegeometry) {
		//Get window geometry and coordinates
		gtk_window_get_size(GTK_WINDOW(mmguiapp->window->window), &(mmguiapp->options->wgwidth), &(mmguiapp->options->wgheight));
		/*Get new coordinates only if window visible or use saved coordinates otherwise*/
		if (gtk_widget_get_visible(mmguiapp->window->window)) {
			gtk_window_get_position(GTK_WINDOW(mmguiapp->window->window), &(mmguiapp->options->wgposx), &(mmguiapp->options->wgposy));
		}
		//Save it
		if ((mmguiapp->options->wgwidth >= 1) && (mmguiapp->options->wgheight >= 1)) {
			//Window geometry
			gmm_settings_set_int(mmguiapp->settings, "window_geometry_width", mmguiapp->options->wgwidth);
			gmm_settings_set_int(mmguiapp->settings, "window_geometry_height", mmguiapp->options->wgheight);
			gmm_settings_set_int(mmguiapp->settings, "window_geometry_x", mmguiapp->options->wgposx);
			gmm_settings_set_int(mmguiapp->settings, "window_geometry_y", mmguiapp->options->wgposy);
			g_debug("Geometry: width: %i, height: %i, x: %i, y: %i\n", mmguiapp->options->wgwidth, mmguiapp->options->wgheight, mmguiapp->options->wgposx,  mmguiapp->options->wgposy);
		}
	}
}

gboolean mmgui_main_ui_window_delete_event_signal(GtkWidget *widget, GdkEvent  *event, gpointer data)
{
	mmgui_application_t mmguiapp;
	enum _mmgui_notifications_sound soundmode;
	enum _mmgui_main_exit_dialog_result dialogres;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return TRUE;
	
	if (mmguiapp->options->askforhide) {
		/*Ask at exit*/
		dialogres = mmgui_main_ui_window_hide_dialog(mmguiapp);
		if (dialogres == MMGUI_MAIN_EXIT_DIALOG_HIDE) {
			/*Hide application*/
			gtk_widget_hide_on_delete(mmguiapp->window->window);
			/*Show notification*/
			if (!mmguiapp->options->hidenotifyshown) {
				if (mmguiapp->options->usesounds) {
					soundmode = MMGUI_NOTIFICATIONS_SOUND_INFO;
				} else {
					soundmode = MMGUI_NOTIFICATIONS_SOUND_NONE;
				}
				mmgui_notifications_show(mmguiapp->notifications, _("Modem Manager GUI window hidden"), _("Use tray icon or messaging menu to show window again"), soundmode, NULL, NULL);
				mmguiapp->options->hidenotifyshown = TRUE;
				gmm_settings_set_boolean(mmguiapp->settings, "window_hide_notify_shown", mmguiapp->options->hidenotifyshown);
			}
			/*Set tray menu mark*/
			g_signal_handler_block(G_OBJECT(mmguiapp->window->showwin_tm), mmguiapp->window->traysigid);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mmguiapp->window->showwin_tm), FALSE);
			g_signal_handler_unblock(G_OBJECT(mmguiapp->window->showwin_tm), mmguiapp->window->traysigid);
			/*Save window state*/
			mmguiapp->options->minimized = TRUE;
			gmm_settings_set_boolean(mmguiapp->settings, "window_state_minimized", mmguiapp->options->minimized);
			return TRUE;
		} else if (dialogres == MMGUI_MAIN_EXIT_DIALOG_EXIT) {
			/*Exit application*/
			mmgui_main_ui_window_save_geometry(mmguiapp);
			return FALSE;
		} else {
			/*Do nothing*/
			return TRUE;
		}
	} else {
		/*Do not ask at exit*/
		if (mmguiapp->options->hidetotray) {
			gtk_widget_hide_on_delete(mmguiapp->window->window);
			/*Show notification*/
			if (!mmguiapp->options->hidenotifyshown) {
				if (mmguiapp->options->usesounds) {
					soundmode = MMGUI_NOTIFICATIONS_SOUND_INFO;
				} else {
					soundmode = MMGUI_NOTIFICATIONS_SOUND_NONE;
				}
				mmgui_notifications_show(mmguiapp->notifications, _("Modem Manager GUI window hidden"), _("Use tray icon or messaging menu to show window again"), soundmode, NULL, NULL);
				mmguiapp->options->hidenotifyshown = TRUE;
				gmm_settings_set_boolean(mmguiapp->settings, "window_hide_notify_shown", mmguiapp->options->hidenotifyshown);
			}
			/*Set tray menu mark*/
			g_signal_handler_block(G_OBJECT(mmguiapp->window->showwin_tm), mmguiapp->window->traysigid);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mmguiapp->window->showwin_tm), FALSE);
			g_signal_handler_unblock(G_OBJECT(mmguiapp->window->showwin_tm), mmguiapp->window->traysigid);
			/*Save window state*/
			mmguiapp->options->minimized = TRUE;
			gmm_settings_set_boolean(mmguiapp->settings, "window_state_minimized", mmguiapp->options->minimized);
			return TRUE;
		} else {
			mmgui_main_ui_window_save_geometry(mmguiapp);
			return FALSE;
		}
	}
}

void mmgui_main_ui_window_destroy_signal(GObject *object, gpointer data)
{
	mmgui_application_t mmguiapp;
				
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_application_terminate(mmguiapp);
}

static void mmgui_main_ui_exit_menu_item_activate_signal(GSimpleAction *action, GVariant *parameter, gpointer data)
{
	mmgui_application_t mmguiapp;
				
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_ui_window_save_geometry(mmguiapp);
	
	mmgui_main_application_terminate(mmguiapp);
}

static void mmgui_main_ui_help_menu_item_activate_signal(GSimpleAction *action, GVariant *parameter, gpointer data)
{
	mmgui_application_t mmguiapp;
	GError *error;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	error = NULL;
	
	if (!gtk_show_uri(gtk_widget_get_screen(mmguiapp->window->window), "help:modem-manager-gui", gtk_get_current_event_time(), &error)) {
		mmgui_main_application_unresolved_error(mmguiapp, _("Error while displaying the help contents"), error->message);
		g_error_free(error);
	}
}

static void mmgui_main_ui_about_menu_item_activate_signal(GSimpleAction *action, GVariant *parameter, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	gtk_dialog_run(GTK_DIALOG(mmguiapp->window->aboutdialog));
	
	gtk_widget_hide(mmguiapp->window->aboutdialog);
}

static void mmgui_main_ui_section_menu_item_activate_signal(GSimpleAction *action, GVariant *parameter, gpointer data)
{
	mmgui_application_t mmguiapp;
	const gchar *state;
	GtkWidget *toolbutton;
			
	mmguiapp = (mmgui_application_t)data;
	
	if ((mmguiapp == NULL) || (parameter == NULL)) return;
	
	state = g_variant_get_string(parameter, NULL);
	
	g_simple_action_set_state(action, g_variant_new_string(state));
	
	if (g_str_equal(state, "devices")) {
		toolbutton = mmguiapp->window->devbutton;
	} else if (g_str_equal(state, "sms")) {
		toolbutton = mmguiapp->window->smsbutton;
	} else if (g_str_equal(state, "ussd")) {
		toolbutton = mmguiapp->window->ussdbutton;
	} else if (g_str_equal(state, "info")) {
		toolbutton = mmguiapp->window->infobutton;
	} else if (g_str_equal(state, "scan")) {
		toolbutton = mmguiapp->window->scanbutton;
	} else if (g_str_equal(state, "traffic")) {
		toolbutton = mmguiapp->window->trafficbutton;
	} else if (g_str_equal(state, "contacts")) {
		toolbutton = mmguiapp->window->contactsbutton;
	} else {
		toolbutton = mmguiapp->window->devbutton;
	}
	
	if (gtk_widget_get_sensitive(toolbutton)) {
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(toolbutton), TRUE);
		g_debug("Application menu item activated: %s\n", state);
	}
}


void mmgui_main_ui_control_buttons_disable(mmgui_application_t mmguiapp, gboolean disable)
{
	if (mmguiapp == NULL) return;
	
	/*Toolbar*/
	gtk_widget_set_sensitive(mmguiapp->window->smsbutton, !disable);
	gtk_widget_set_sensitive(mmguiapp->window->ussdbutton, !disable);
	gtk_widget_set_sensitive(mmguiapp->window->infobutton, !disable);
	gtk_widget_set_sensitive(mmguiapp->window->scanbutton, !disable);
	gtk_widget_set_sensitive(mmguiapp->window->trafficbutton, !disable);
	gtk_widget_set_sensitive(mmguiapp->window->contactsbutton, !disable);
	/*Application menu*/
	mmgui_main_ui_application_menu_set_state(mmguiapp, !disable);
	
	if (disable) {
		mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_INTERRUPT, NULL);
		/*Toolbar*/
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(mmguiapp->window->devbutton), TRUE);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(mmguiapp->window->notebook), MMGUI_MAIN_PAGE_DEVICES);
		/*Application menu*/
		mmgui_main_ui_application_menu_set_page(mmguiapp, MMGUI_MAIN_PAGE_DEVICES);
		/*Show infobar*/
		mmgui_ui_infobar_show(mmguiapp, _("No devices found in system"), MMGUI_MAIN_INFOBAR_TYPE_INFO);
	} else {
		/*Hide infobar*/
		mmgui_ui_infobar_show_result(mmguiapp, MMGUI_MAIN_INFOBAR_RESULT_INTERRUPT, NULL);
	}
}

gboolean mmgui_main_ui_update_statusbar_from_thread(gpointer data)
{
	mmgui_application_t mmguiapp;
	mmguidevice_t device;
	gchar *statusmsg;
	gchar rxbuffer[32], txbuffer[32];
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return FALSE;
	if (mmguiapp->core == NULL) return FALSE;
	
	device = mmguicore_devices_get_current(mmguiapp->core);
	
	if (device != NULL) {
		/*Set signal icon*/
		if ((device->siglevel == 0) && (mmguiapp->window->signal0icon != NULL)) {
			gtk_image_set_from_pixbuf(GTK_IMAGE(mmguiapp->window->signalimage), mmguiapp->window->signal0icon);
		} else if ((device->siglevel > 0) && (device->siglevel <= 25) && (mmguiapp->window->signal25icon != NULL)) {
			gtk_image_set_from_pixbuf(GTK_IMAGE(mmguiapp->window->signalimage), mmguiapp->window->signal25icon);
		} else if ((device->siglevel > 25) && (device->siglevel <= 50) && (mmguiapp->window->signal50icon != NULL)) {
			gtk_image_set_from_pixbuf(GTK_IMAGE(mmguiapp->window->signalimage), mmguiapp->window->signal50icon);
		} else if ((device->siglevel > 50) && (device->siglevel <= 75) && (mmguiapp->window->signal75icon != NULL)) {
			gtk_image_set_from_pixbuf(GTK_IMAGE(mmguiapp->window->signalimage), mmguiapp->window->signal75icon);
		} else if ((device->siglevel > 75) && (mmguiapp->window->signal100icon != NULL)) {
			gtk_image_set_from_pixbuf(GTK_IMAGE(mmguiapp->window->signalimage), mmguiapp->window->signal100icon);
		}
		/*Show connection statistics*/
		if (!device->connected) {
			if ((device->operatorname == NULL) || ((device->operatorname != NULL) && (device->operatorname[0] == '\0'))) {
				/*Operator name is unknown - show registration status*/
				statusmsg = g_strdup(mmgui_str_format_reg_status(device->regstatus));
			} else {
				/*No network connection*/
				statusmsg = g_strdup_printf(_("%s disconnected"), device->operatorname);
			}
		} else {
			/*Network connection statistics*/
			statusmsg = g_strdup_printf("%s ↓ %s ↑ %s", device->operatorname, mmgui_str_format_bytes(device->rxbytes, rxbuffer, sizeof(rxbuffer), FALSE), mmgui_str_format_bytes(device->txbytes, txbuffer, sizeof(txbuffer), FALSE));
		}
		
		gtk_statusbar_pop(GTK_STATUSBAR(mmguiapp->window->statusbar), mmguiapp->window->sbcontext);
		
		mmguiapp->window->sbcontext = gtk_statusbar_get_context_id(GTK_STATUSBAR(mmguiapp->window->statusbar), statusmsg);
		
		gtk_statusbar_push(GTK_STATUSBAR(mmguiapp->window->statusbar), mmguiapp->window->sbcontext, statusmsg);
		
		g_free(statusmsg);
	} else {
		/*Zero signal level indicator*/
		gtk_image_set_from_pixbuf(GTK_IMAGE(mmguiapp->window->signalimage), mmguiapp->window->signal0icon);
		/*Clear statusbar*/
		gtk_statusbar_pop(GTK_STATUSBAR(mmguiapp->window->statusbar), mmguiapp->window->sbcontext);
	}
	
	return FALSE;
}

//TRAY
static void mmgui_main_tray_icon_activation_signal(GtkStatusIcon *status_icon, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	if ((mmguiapp->core == NULL) || (mmguiapp->window == NULL)) return;
	
	if (gtk_widget_get_visible(mmguiapp->window->window)) {
		/*Save window position*/
		if (mmguiapp->options->savegeometry) {
			gtk_window_get_position(GTK_WINDOW(mmguiapp->window->window), &(mmguiapp->options->wgposx), &(mmguiapp->options->wgposy));
		}
		/*Hide window*/
		gtk_widget_hide(mmguiapp->window->window);
		mmguiapp->options->minimized = TRUE;
		g_signal_handler_block(G_OBJECT(mmguiapp->window->showwin_tm), mmguiapp->window->traysigid);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mmguiapp->window->showwin_tm), FALSE);
		g_signal_handler_unblock(G_OBJECT(mmguiapp->window->showwin_tm), mmguiapp->window->traysigid);
	} else {
		/*Restore window position*/
		if (mmguiapp->options->savegeometry) {
			gtk_window_move(GTK_WINDOW(mmguiapp->window->window), mmguiapp->options->wgposx, mmguiapp->options->wgposy);
		}
		/*Show window*/
		gtk_widget_show(mmguiapp->window->window);
		mmguiapp->options->minimized = FALSE;
		g_signal_handler_block(G_OBJECT(mmguiapp->window->showwin_tm), mmguiapp->window->traysigid);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mmguiapp->window->showwin_tm), TRUE);
		g_signal_handler_unblock(G_OBJECT(mmguiapp->window->showwin_tm), mmguiapp->window->traysigid);
	}
	/*Save window state*/
	gmm_settings_set_boolean(mmguiapp->settings, "window_state_minimized", mmguiapp->options->minimized);
}

static void mmgui_main_tray_icon_window_show_signal(GtkCheckMenuItem *checkmenuitem, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	if ((mmguiapp->core == NULL) || (mmguiapp->window == NULL)) return;
	
	mmgui_main_tray_icon_activation_signal(GTK_STATUS_ICON(mmguiapp->window->statusicon), mmguiapp);
}

static void mmgui_main_tray_icon_new_sms_signal(GtkMenuItem *menuitem, gpointer data)
{
	mmgui_application_t mmguiapp;
	guint smscaps;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	if ((mmguiapp->core == NULL) || (mmguiapp->window == NULL)) return;
	
	if (!gtk_widget_get_visible(mmguiapp->window->window)) {
		gtk_widget_show(mmguiapp->window->window);
		g_signal_handler_block(G_OBJECT(mmguiapp->window->showwin_tm), mmguiapp->window->traysigid);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mmguiapp->window->showwin_tm), TRUE);
		g_signal_handler_unblock(G_OBJECT(mmguiapp->window->showwin_tm), mmguiapp->window->traysigid);
	} else {
		gtk_window_present(GTK_WINDOW(mmguiapp->window->window));
	}
	
	
	if (mmguicore_devices_get_enabled(mmguiapp->core)) {
		smscaps = mmguicore_sms_get_capabilities(mmguiapp->core);
		if (smscaps & MMGUI_SMS_CAPS_SEND) {
			gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(mmguiapp->window->smsbutton), TRUE);
			mmgui_main_sms_new(mmguiapp);
		}
	}
}

static void mmgui_main_tray_icon_exit_signal(GtkMenuItem *menuitem, gpointer data)
{
	mmgui_application_t mmguiapp;
				
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	mmgui_main_ui_window_save_geometry(mmguiapp);
	
	mmgui_main_application_terminate(mmguiapp);
}

static void mmgui_main_tray_popup_menu_show_signal(GtkStatusIcon *status_icon, guint button, guint activate_time, gpointer data)
{
	mmgui_application_t mmguiapp;
	guint smscaps;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	if ((mmguiapp->core == NULL) || (mmguiapp->window == NULL)) return;
	
	if (mmguicore_devices_get_enabled(mmguiapp->core)) {
		smscaps = mmguicore_sms_get_capabilities(mmguiapp->core);
		if (smscaps & MMGUI_SMS_CAPS_SEND) {
			gtk_widget_set_sensitive(mmguiapp->window->newsms_tm, TRUE);
		} else {
			gtk_widget_set_sensitive(mmguiapp->window->newsms_tm, FALSE);
		}
	} else {
		gtk_widget_set_sensitive(mmguiapp->window->newsms_tm, FALSE);
	}
	
	gtk_menu_popup(GTK_MENU(mmguiapp->window->traymenu), NULL, NULL, gtk_status_icon_position_menu, status_icon, button, activate_time);
}

static gboolean mmgui_main_tray_tooltip_show_signal(GtkStatusIcon *status_icon, gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, gpointer data)
{
	mmgui_application_t mmguiapp;
	guint unreadmessages;
	gchar strbuf[64];
	
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return FALSE;
	
	if (mmguicore_devices_get_current(mmguiapp->core) != NULL) {
		unreadmessages = mmgui_smsdb_get_unread_messages(mmguicore_devices_get_sms_db(mmguiapp->core));
		if (unreadmessages > 0) {
			memset(strbuf, 0, sizeof(strbuf));
			g_snprintf(strbuf, sizeof(strbuf), _("Unread messages: %u"), unreadmessages);
			gtk_tooltip_set_text(tooltip, strbuf);
		} else {
			gtk_tooltip_set_text(tooltip, _("No unread messages"));
		}
	} else {
		gtk_tooltip_set_text(tooltip, _("No unread messages"));
	}
	
	return TRUE;
}

static void mmgui_main_tray_icon_init(mmgui_application_t mmguiapp)
{
	if (mmguiapp == NULL) return;
	
	/*Tray icon*/
	mmguiapp->window->statusicon = gtk_status_icon_new_from_file(RESOURCE_MAINWINDOW_ICON);
	g_signal_connect(G_OBJECT(mmguiapp->window->statusicon), "activate", G_CALLBACK(mmgui_main_tray_icon_activation_signal), mmguiapp);
	gtk_status_icon_set_tooltip_text(mmguiapp->window->statusicon, _("No unread messages"));
	/*Tray menu*/
	mmguiapp->window->traymenu = gtk_menu_new();
	/*Show window entry*/
	mmguiapp->window->showwin_tm = gtk_check_menu_item_new_with_label(_("Show window"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mmguiapp->window->showwin_tm), gtk_widget_get_visible(mmguiapp->window->window));
	mmguiapp->window->traysigid = g_signal_connect(G_OBJECT(mmguiapp->window->showwin_tm), "toggled", G_CALLBACK(mmgui_main_tray_icon_window_show_signal), mmguiapp);
	/*Separator*/
	mmguiapp->window->sep1_tm = gtk_separator_menu_item_new();
	/*New SMS entry*/
	mmguiapp->window->newsms_tm = gtk_menu_item_new_with_label(_("New SMS"));
	gtk_widget_set_sensitive(mmguiapp->window->newsms_tm, FALSE);
	g_signal_connect(G_OBJECT(mmguiapp->window->newsms_tm), "activate", G_CALLBACK(mmgui_main_tray_icon_new_sms_signal), mmguiapp);
	/*Separator 2*/
	mmguiapp->window->sep2_tm = gtk_separator_menu_item_new();
	/*Quit entry*/
	mmguiapp->window->quit_tm = gtk_menu_item_new_with_label(_("Quit"));
	g_signal_connect(G_OBJECT(mmguiapp->window->quit_tm), "activate", G_CALLBACK(mmgui_main_tray_icon_exit_signal), mmguiapp);
	/*Packaging*/
	gtk_menu_shell_append(GTK_MENU_SHELL(mmguiapp->window->traymenu), mmguiapp->window->showwin_tm);
	gtk_menu_shell_append(GTK_MENU_SHELL(mmguiapp->window->traymenu), mmguiapp->window->sep1_tm);
	gtk_menu_shell_append(GTK_MENU_SHELL(mmguiapp->window->traymenu), mmguiapp->window->newsms_tm);
	gtk_menu_shell_append(GTK_MENU_SHELL(mmguiapp->window->traymenu), mmguiapp->window->sep2_tm);
	gtk_menu_shell_append(GTK_MENU_SHELL(mmguiapp->window->traymenu), mmguiapp->window->quit_tm);
	gtk_widget_show_all(mmguiapp->window->traymenu);
	/*Tray menu signal*/
	g_signal_connect(G_OBJECT(mmguiapp->window->statusicon), "popup-menu", G_CALLBACK(mmgui_main_tray_popup_menu_show_signal), mmguiapp);
	/*Tray tooltip signal*/
	g_signal_connect(G_OBJECT(mmguiapp->window->statusicon), "query-tooltip", G_CALLBACK(mmgui_main_tray_tooltip_show_signal), mmguiapp);
	gtk_status_icon_set_has_tooltip(mmguiapp->window->statusicon, TRUE);
}

/*Ayatana*/
static void mmgui_main_ayatana_event_callback(enum _mmgui_ayatana_event event, gpointer ayatana, gpointer data, gpointer userdata)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)userdata;
	
	if (userdata == NULL) return;
	
	gtk_window_present(GTK_WINDOW(mmguiapp->window->window));
	
	if (event == MMGUI_AYATANA_EVENT_CLIENT) {
		if (mmguicore_devices_get_enabled(mmguiapp->core)) {
			gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(mmguiapp->window->smsbutton), TRUE);
		}
	}
}

//Initialization
static void mmgui_main_application_unresolved_error(mmgui_application_t mmguiapp, gchar *caption, gchar *text)
{
	GtkWidget *dialog;
	
	if ((mmguiapp == NULL) || (caption == NULL) || (text == NULL)) return;
	
	//Show error message (Interface may be not built, so using custom message box)
	dialog = gtk_message_dialog_new(NULL,
									GTK_DIALOG_MODAL,
									GTK_MESSAGE_ERROR,
									GTK_BUTTONS_OK,
									"%s\n%s", caption, text);
	gtk_window_set_title(GTK_WINDOW(dialog), "Modem Manager GUI");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy (dialog);
	
	//Close application
	mmgui_main_application_terminate(mmguiapp);
}

static gboolean mmgui_main_contacts_load_from_thread(gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if (data == NULL) return G_SOURCE_REMOVE; 
	
	/*SMS autoconpletion for contacts*/
	mmgui_main_sms_load_contacts_from_system_addressbooks(mmguiapp);
	
	/*Import contacts from system address books*/
	mmgui_main_contacts_load_from_system_addressbooks(mmguiapp);
	
	return G_SOURCE_REMOVE;
}

static gboolean mmgui_main_settings_ui_load(mmgui_application_t mmguiapp)
{
	gchar *strparam;
	
	if (mmguiapp == NULL) return FALSE;
	if ((mmguiapp->window == NULL) || (mmguiapp->settings == NULL)) return FALSE;
	
	//Get last opened device and open it
	strparam = gmm_settings_get_string(mmguiapp->settings, "device_identifier", MMGUI_MAIN_DEFAULT_DEVICE_IDENTIFIER);
	mmgui_main_device_select_from_list(mmguiapp, strparam);
	g_free(strparam);
	
	return TRUE;
}

static gboolean mmgui_main_settings_load(mmgui_application_t mmguiapp)
{
	gchar *strparam;
	
	if (mmguiapp == NULL) return FALSE;
	if ((mmguiapp->options == NULL) || (mmguiapp->settings == NULL)) return FALSE;
	
	#if GTK_CHECK_VERSION(3,4,0)
		/*RX speed graph color (default 078B2DFF)*/
		strparam = gmm_settings_get_string(mmguiapp->settings, "graph_rx_color", MMGUI_MAIN_DEFAULT_RX_GRAPH_RGBA_COLOR);
		if (!gdk_rgba_parse(&mmguiapp->options->rxtrafficcolor, strparam)) {
			gdk_rgba_parse(&mmguiapp->options->rxtrafficcolor, MMGUI_MAIN_DEFAULT_RX_GRAPH_RGBA_COLOR);
		}
		g_free(strparam);
		/*TX speed graph color (default 99114DFF)*/
		strparam = gmm_settings_get_string(mmguiapp->settings, "graph_tx_color", MMGUI_MAIN_DEFAULT_TX_GRAPH_RGBA_COLOR);
		if (!gdk_rgba_parse(&mmguiapp->options->txtrafficcolor, strparam)) {
			gdk_rgba_parse(&mmguiapp->options->txtrafficcolor, MMGUI_MAIN_DEFAULT_TX_GRAPH_RGBA_COLOR);
		}
		g_free(strparam);
	#else
		/*RX speed graph color (default 078B2D)*/
		strparam = gmm_settings_get_string(mmguiapp->settings, "graph_rx_color", MMGUI_MAIN_DEFAULT_RX_GRAPH_RGB_COLOR);
		if (!gdk_color_parse(strparam, &mmguiapp->options->rxtrafficcolor)) {
			gdk_color_parse(MMGUI_MAIN_DEFAULT_RX_GRAPH_RGB_COLOR, &mmguiapp->options->rxtrafficcolor);
		}
		g_free(strparam);
		/*TX speed graph color (default 99114D)*/
		strparam = gmm_settings_get_string(mmguiapp->settings, "graph_tx_color", MMGUI_MAIN_DEFAULT_TX_GRAPH_RGB_COLOR);
		if (!gdk_color_parse(strparam, &mmguiapp->options->txtrafficcolor)) {
			gdk_color_parse(MMGUI_MAIN_DEFAULT_TX_GRAPH_RGB_COLOR, &mmguiapp->options->txtrafficcolor);
		}
		g_free(strparam);
	#endif
	
	/*SMS options*/
	mmguiapp->options->concatsms = gmm_settings_get_boolean(mmguiapp->settings, "sms_concatenation", FALSE);
	mmguiapp->options->smsexpandfolders = gmm_settings_get_boolean(mmguiapp->settings, "sms_expand_folders", TRUE);
	mmguiapp->options->smsoldontop = gmm_settings_get_boolean(mmguiapp->settings, "sms_old_on_top", TRUE);
	mmguiapp->options->smsvalidityperiod = gmm_settings_get_int(mmguiapp->settings, "sms_validity_period", -1);
	mmguiapp->options->smsdeliveryreport = gmm_settings_get_boolean(mmguiapp->settings, "sms_send_delivery_report", FALSE);
		
	/*Behaviour options*/
	mmguiapp->options->hidetotray = gmm_settings_get_boolean(mmguiapp->settings, "behaviour_hide_to_tray", FALSE);
	mmguiapp->options->usesounds = gmm_settings_get_boolean(mmguiapp->settings, "behaviour_use_sounds", TRUE);
	mmguiapp->options->askforhide = gmm_settings_get_boolean(mmguiapp->settings, "behaviour_ask_to_hide", TRUE);
	mmguiapp->options->savegeometry = gmm_settings_get_boolean(mmguiapp->settings, "behaviour_save_geometry", FALSE);
	
	/*Modules settings (coreoptions)*/
	mmguiapp->coreoptions->enabletimeout = gmm_settings_get_int(mmguiapp->settings, "modules_enable_device_timeout", 20);
	mmguiapp->coreoptions->sendsmstimeout = gmm_settings_get_int(mmguiapp->settings, "modules_send_sms_timeout", 35);
	mmguiapp->coreoptions->sendussdtimeout = gmm_settings_get_int(mmguiapp->settings, "modules_send_ussd_timeout", 25);
	mmguiapp->coreoptions->scannetworkstimeout = gmm_settings_get_int(mmguiapp->settings, "modules_scan_networks_timeout", 60);
	
	/*Preferred modules*/
	if (mmguiapp->coreoptions->mmmodule == NULL) {
		mmguiapp->coreoptions->mmmodule = gmm_settings_get_string(mmguiapp->settings, "modules_preferred_modem_manager", NULL);
	}
	if (mmguiapp->coreoptions->cmmodule == NULL) {
		mmguiapp->coreoptions->cmmodule = gmm_settings_get_string(mmguiapp->settings, "modules_preferred_connection_manager", NULL);
	}
	
	/*Window geometry*/
	mmguiapp->options->wgwidth = gmm_settings_get_int(mmguiapp->settings, "window_geometry_width", -1);
	mmguiapp->options->wgheight = gmm_settings_get_int(mmguiapp->settings, "window_geometry_height", -1);
	mmguiapp->options->wgposx = gmm_settings_get_int(mmguiapp->settings, "window_geometry_x", -1);
	mmguiapp->options->wgposy = gmm_settings_get_int(mmguiapp->settings, "window_geometry_y", -1);
	
	/*If window was minimized on exit*/
	mmguiapp->options->minimized = gmm_settings_get_boolean(mmguiapp->settings, "window_state_minimized", FALSE);
	
	/*If hide notification already shown*/
	mmguiapp->options->hidenotifyshown = gmm_settings_get_boolean(mmguiapp->settings, "window_hide_notify_shown", FALSE);
	
	return TRUE;
}

static gboolean mmgui_main_application_build_user_interface(mmgui_application_t mmguiapp)
{
	GtkBuilder *builder;
	GError *error;
	GtkStyleContext *context;
	GtkWidget *tbimage;
	gint i;
	static struct _mmgui_application_data shortcutsdata[MMGUI_MAIN_CONTROL_SHORTCUT_NUMBER];
	
	/*Widgets*/
	struct _mmgui_main_widgetset widgetset[] = {
		/*Window*/
		{"window", &(mmguiapp->window->window)},
		/*Controls*/
		{"toolbar", &(mmguiapp->window->toolbar)},
		{"statusbar", &(mmguiapp->window->statusbar)},
		{"infobar", &(mmguiapp->window->infobar)},
		{"infobarspinner", &(mmguiapp->window->infobarspinner)},
		{"infobarimage", &(mmguiapp->window->infobarimage)},
		{"infobarlabel", &(mmguiapp->window->infobarlabel)},
		{"infobarstopbutton", &(mmguiapp->window->infobarstopbutton)},
		{"notebook", &(mmguiapp->window->notebook)},
		{"signalimage", &(mmguiapp->window->signalimage)},
		/*Toolbar buttons*/
		{"devbutton", &(mmguiapp->window->devbutton)},
		{"smsbutton", &(mmguiapp->window->smsbutton)},
		{"ussdbutton", &(mmguiapp->window->ussdbutton)},
		{"infobutton", &(mmguiapp->window->infobutton)},
		{"scanbutton", &(mmguiapp->window->scanbutton)},
		{"trafficbutton", &(mmguiapp->window->trafficbutton)},
		{"contactsbutton", &(mmguiapp->window->contactsbutton)},
		/*Dialogs*/
		{"aboutdialog", &(mmguiapp->window->aboutdialog)},
		{"prefdialog", &(mmguiapp->window->prefdialog)},
		{"questiondialog", &(mmguiapp->window->questiondialog)},
		{"errordialog", &(mmguiapp->window->errordialog)},
		{"exitdialog", &(mmguiapp->window->exitdialog)},
		/*SMS send dialog*/
		{"newsmsdialog", &(mmguiapp->window->newsmsdialog)},
		{"smsnumberentry", &(mmguiapp->window->smsnumberentry)},
		{"smsnumbercombo", &(mmguiapp->window->smsnumbercombo)},
		{"smstextview", &(mmguiapp->window->smstextview)},
		{"newsmssendtb", &(mmguiapp->window->newsmssendtb)},
		{"newsmssavetb", &(mmguiapp->window->newsmssavetb)},
		{"newsmsspellchecktb", &(mmguiapp->window->newsmsspellchecktb)},
		/*Devices page*/
		{"devlist", &(mmguiapp->window->devlist)},
		{"devconnctl", &(mmguiapp->window->devconnctl)},
		{"devconneditor", &(mmguiapp->window->devconneditor)},
		{"devconncb", &(mmguiapp->window->devconncb)},
		/*Connections dialog*/
		{"conneditdialog", &(mmguiapp->window->conneditdialog)},
		{"connaddtoolbutton", &(mmguiapp->window->connaddtoolbutton)},
		{"connremovetoolbutton", &(mmguiapp->window->connremovetoolbutton)},
		{"connsavetoolbutton", &(mmguiapp->window->connsavetoolbutton)},
		{"contreeview", &(mmguiapp->window->contreeview)},
		{"connnamecombobox", &(mmguiapp->window->connnamecombobox)},
		{"connnamecomboboxentry", &(mmguiapp->window->connnamecomboboxentry)},
		{"connnameapnentry", &(mmguiapp->window->connnameapnentry)},
		{"connnethomeradiobutton", &(mmguiapp->window->connnethomeradiobutton)},
		{"connnetroamradiobutton", &(mmguiapp->window->connnetroamradiobutton)},
		{"connnetidentry", &(mmguiapp->window->connnetidentry)},
		{"connauthnumberentry", &(mmguiapp->window->connauthnumberentry)},
		{"connauthusernameentry", &(mmguiapp->window->connauthusernameentry)},
		{"connauthpassentry", &(mmguiapp->window->connauthpassentry)},
		{"conndnsdynradiobutton", &(mmguiapp->window->conndnsdynradiobutton)},
		{"conndnsstradiobutton", &(mmguiapp->window->conndnsstradiobutton)},
		{"conndns1entry", &(mmguiapp->window->conndns1entry)},
		{"conndns2entry", &(mmguiapp->window->conndns2entry)},
		/*Add connection dialog*/
		{"connadddialog", &(mmguiapp->window->connadddialog)},
		/*SMS page*/
		{"smsinfobar", &(mmguiapp->window->smsinfobar)},
		{"smsinfobarlabel", &(mmguiapp->window->smsinfobarlabel)},
		{"smslist", &(mmguiapp->window->smslist)},
		{"smstext", &(mmguiapp->window->smstext)},
		{"newsmsbutton", &(mmguiapp->window->newsmsbutton)},
		{"removesmsbutton", &(mmguiapp->window->removesmsbutton)},
		{"answersmsbutton", &(mmguiapp->window->answersmsbutton)},
		/*Info page*/
		{"devicevlabel", &(mmguiapp->window->devicevlabel)},
		{"operatorvlabel", &(mmguiapp->window->operatorvlabel)},
		{"operatorcodevlabel", &(mmguiapp->window->operatorcodevlabel)},
		{"regstatevlabel", &(mmguiapp->window->regstatevlabel)},
		{"modevlabel", &(mmguiapp->window->modevlabel)},
		{"imeivlabel", &(mmguiapp->window->imeivlabel)},
		{"imsivlabel", &(mmguiapp->window->imsivlabel)},
		{"signallevelprogressbar", &(mmguiapp->window->signallevelprogressbar)},
		{"3gpplocationvlabel", &(mmguiapp->window->info3gpplocvlabel)},
		{"gpslocationvlabel", &(mmguiapp->window->infogpslocvlabel)},
		{"equipmentimage", &(mmguiapp->window->equipmentimage)},
		{"networkimage", &(mmguiapp->window->networkimage)},
		{"locationimage", &(mmguiapp->window->locationimage)},
		/*USSD page*/
		{"ussdinfobar", &(mmguiapp->window->ussdinfobar)},
		{"ussdinfobarlabel", &(mmguiapp->window->ussdinfobarlabel)},
		{"ussdentry", &(mmguiapp->window->ussdentry)},
		{"ussdcombobox", &(mmguiapp->window->ussdcombobox)},
		{"ussdeditor", &(mmguiapp->window->ussdeditor)},
		{"ussdsend", &(mmguiapp->window->ussdsend)},
		{"ussdtext", &(mmguiapp->window->ussdtext)},
		/*Scan page*/
		{"scaninfobar", &(mmguiapp->window->scaninfobar)},
		{"scaninfobarlabel", &(mmguiapp->window->scaninfobarlabel)},
		{"scanlist", &(mmguiapp->window->scanlist)},
		{"startscanbutton", &(mmguiapp->window->startscanbutton)},
		{"scancreateconnectionbutton", &(mmguiapp->window->scancreateconnectionbutton)},
		/*Contacts page*/
		{"contactsinfobar", &(mmguiapp->window->contactsinfobar)},
		{"contactsinfobarlabel", &(mmguiapp->window->contactsinfobarlabel)},
		{"newcontactbutton", &(mmguiapp->window->newcontactbutton)},
		{"removecontactbutton", &(mmguiapp->window->removecontactbutton)},
		{"smstocontactbutton", &(mmguiapp->window->smstocontactbutton)},
		{"contactstreeview", &(mmguiapp->window->contactstreeview)},
		/*New contact dialog*/
		{"newcontactdialog", &(mmguiapp->window->newcontactdialog)},
		{"contactnameentry", &(mmguiapp->window->contactnameentry)},
		{"contactnumberentry", &(mmguiapp->window->contactnumberentry)},
		{"contactemailentry", &(mmguiapp->window->contactemailentry)},
		{"contactgroupentry", &(mmguiapp->window->contactgroupentry)},
		{"contactname2entry", &(mmguiapp->window->contactname2entry)},
		{"contactnumber2entry", &(mmguiapp->window->contactnumber2entry)},
		{"newcontactaddbutton", &(mmguiapp->window->newcontactaddbutton)},
		/*Traffic page*/
		{"trafficparamslist", &(mmguiapp->window->trafficparamslist)},
		{"trafficdrawingarea", &(mmguiapp->window->trafficdrawingarea)},
		/*Traffic limits dialog*/
		{"trafficlimitsdialog", &(mmguiapp->window->trafficlimitsdialog)},
		{"trafficlimitcheckbutton", &(mmguiapp->window->trafficlimitcheckbutton)},
		{"trafficamount", &(mmguiapp->window->trafficamount)},
		{"trafficunits", &(mmguiapp->window->trafficunits)},
		{"trafficmessage", &(mmguiapp->window->trafficmessage)},
		{"trafficaction", &(mmguiapp->window->trafficaction)},
		{"timelimitcheckbutton", &(mmguiapp->window->timelimitcheckbutton)},
		{"timeamount", &(mmguiapp->window->timeamount)},
		{"timeunits", &(mmguiapp->window->timeunits)},
		{"timemessage", &(mmguiapp->window->timemessage)},
		{"timeaction", &(mmguiapp->window->timeaction)},
		/*Connections dialog*/
		{"conndialog", &(mmguiapp->window->conndialog)},
		{"connscrolledwindow", &(mmguiapp->window->connscrolledwindow)},
		{"conntreeview", &(mmguiapp->window->conntreeview)},
		{"conntermtoolbutton", &(mmguiapp->window->conntermtoolbutton)},
		/*Traffic statistics dialog*/
		{"trafficstatsdialog", &(mmguiapp->window->trafficstatsdialog)},
		{"trafficstatstreeview", &(mmguiapp->window->trafficstatstreeview)},
		{"trafficstatsmonthcb", &(mmguiapp->window->trafficstatsmonthcb)},
		{"trafficstatsyearcb", &(mmguiapp->window->trafficstatsyearcb)},
		/*USSD edition dialog*/
		{"ussdeditdialog", &(mmguiapp->window->ussdeditdialog)},
		{"ussdedittreeview", &(mmguiapp->window->ussdedittreeview)},
		{"newussdtoolbutton", &(mmguiapp->window->newussdtoolbutton)},
		{"removeussdtoolbutton", &(mmguiapp->window->removeussdtoolbutton)},
		{"ussdencodingtoolbutton", &(mmguiapp->window->ussdencodingtoolbutton)},
		/*Preferences dialog*/
		{"prefsmsconcat", &(mmguiapp->window->prefsmsconcat)},
		{"prefsmsexpand", &(mmguiapp->window->prefsmsexpand)},
		{"prefssmsoldontop", &(mmguiapp->window->prefsmsoldontop)},
		{"prefsmsvalidityscale", &(mmguiapp->window->prefsmsvalidityscale)},
		{"prefsmsreportcb", &(mmguiapp->window->prefsmsreportcb)},
		{"preftrafficrxcolor", &(mmguiapp->window->preftrafficrxcolor)},
		{"preftraffictxcolor", &(mmguiapp->window->preftraffictxcolor)},
		{"prefbehavioursounds", &(mmguiapp->window->prefbehavioursounds)},
		{"prefbehaviourhide", &(mmguiapp->window->prefbehaviourhide)},
		{"prefbehaviourgeom", &(mmguiapp->window->prefbehaviourgeom)},
		{"prefbehaviourautostart", &(mmguiapp->window->prefbehaviourautostart)},
		{"prefenabletimeoutscale", &(mmguiapp->window->prefenabletimeoutscale)},
		{"prefsendsmstimeoutscale", &(mmguiapp->window->prefsendsmstimeoutscale)},
		{"prefsendussdtimeoutscale", &(mmguiapp->window->prefsendussdtimeoutscale)},
		{"prefscannetworkstimeoutscale", &(mmguiapp->window->prefscannetworkstimeoutscale)},
		{"prefmodulesmmcombo", &(mmguiapp->window->prefmodulesmmcombo)},
		{"prefmodulescmcombo", &(mmguiapp->window->prefmodulescmcombo)},
		/*Exit dialog*/
		{"exitaskagain", &(mmguiapp->window->exitaskagain)},
		{"exitcloseradio", &(mmguiapp->window->exitcloseradio)},
		{"exithideradio", &(mmguiapp->window->exithideradio)},
		/*Welcome window*/
		{"welcomewindow", &(mmguiapp->window->welcomewindow)},
		{"welcomeimage", &(mmguiapp->window->welcomeimage)},
		{"welcomenotebook", &(mmguiapp->window->welcomenotebook)},
		{"welcomemmcombo", &(mmguiapp->window->welcomemmcombo)},
		{"welcomecmcombo", &(mmguiapp->window->welcomecmcombo)},
		{"welcomeenablecb", &(mmguiapp->window->welcomeenablecb)},
		{"welcomebutton", &(mmguiapp->window->welcomebutton)},
		{"welcomeacttreeview", &(mmguiapp->window->welcomeacttreeview)},
	};
	/*Toolbar image buttons*/
	struct _mmgui_main_widgetset buttonimgset[] = {
		{RESOURCE_TOOLBAR_DEV, &(mmguiapp->window->devbutton)},
		{RESOURCE_TOOLBAR_SMS, &(mmguiapp->window->smsbutton)},
		{RESOURCE_TOOLBAR_USSD, &(mmguiapp->window->ussdbutton)},
		{RESOURCE_TOOLBAR_INFO, &(mmguiapp->window->infobutton)},
		{RESOURCE_TOOLBAR_SCAN, &(mmguiapp->window->scanbutton)},
		{RESOURCE_TOOLBAR_CONT, &(mmguiapp->window->contactsbutton)},
		{RESOURCE_TOOLBAR_TRAFFIC, &(mmguiapp->window->trafficbutton)}
	};
	/*Image widgets*/
	struct _mmgui_main_widgetset imgwidgetset[] = {
		{RESOURCE_INFO_EQUIPMENT, &(mmguiapp->window->equipmentimage)},
		{RESOURCE_INFO_NETWORK, &(mmguiapp->window->networkimage)},
		{RESOURCE_INFO_LOCATION, &(mmguiapp->window->locationimage)}
	};
	/*Pixbufs*/
	struct _mmgui_main_pixbufset pixbufset[] = {
		{RESOURCE_MAINWINDOW_ICON, &(mmguiapp->window->mainicon)},
		{RESOURCE_SIGNAL_0, &(mmguiapp->window->signal0icon)},
		{RESOURCE_SIGNAL_25, &(mmguiapp->window->signal25icon)},
		{RESOURCE_SIGNAL_50, &(mmguiapp->window->signal50icon)},
		{RESOURCE_SIGNAL_75, &(mmguiapp->window->signal75icon)},
		{RESOURCE_SIGNAL_100, &(mmguiapp->window->signal100icon)},
		{RESOURCE_SMS_READ, &(mmguiapp->window->smsreadicon)},
		{RESOURCE_SMS_UNREAD, &(mmguiapp->window->smsunreadicon)}
	};
	/*Application windows*/
	GtkWidget **appwindows[] = {
		&(mmguiapp->window->window),
		&(mmguiapp->window->prefdialog),
		&(mmguiapp->window->aboutdialog),
		&(mmguiapp->window->questiondialog),
		&(mmguiapp->window->errordialog),
		&(mmguiapp->window->newsmsdialog),
		&(mmguiapp->window->ussdeditdialog),
		&(mmguiapp->window->trafficlimitsdialog),
		&(mmguiapp->window->conndialog),
		&(mmguiapp->window->trafficstatsdialog),
		&(mmguiapp->window->conneditdialog),
		&(mmguiapp->window->connadddialog),
		&(mmguiapp->window->newcontactdialog),
		&(mmguiapp->window->welcomewindow),
	};
	/*Accelerator closures*/
	struct _mmgui_main_closureset closureset[] = {
		/*Closures for SMS page*/
		/*send sms message*/
		{MMGUI_MAIN_CONTROL_SHORTCUT_SMS_NEW, &(mmguiapp->window->newsmsclosure)},
		/*remove sms message*/
		{MMGUI_MAIN_CONTROL_SHORTCUT_SMS_REMOVE, &(mmguiapp->window->removesmsclosure)},
		/*answer sms message*/
		{MMGUI_MAIN_CONTROL_SHORTCUT_SMS_ANSWER, &(mmguiapp->window->answersmsclosure)},
		/*Closures for USSD page*/
		/*edit ussd commands*/
		{MMGUI_MAIN_CONTROL_SHORTCUT_USSD_EDITOR, &(mmguiapp->window->ussdeditorclosure)},
		/*send ussd request*/
		{MMGUI_MAIN_CONTROL_SHORTCUT_USSD_SEND, &(mmguiapp->window->ussdsendclosure)},
		/*Closures for Scan page*/
		/*scan networks*/
		{MMGUI_MAIN_CONTROL_SHORTCUT_SCAN_START, &(mmguiapp->window->startscanclosure)},
		/*Closures for Traffic page*/
		/*limits*/
		{MMGUI_MAIN_CONTROL_SHORTCUT_TRAFFIC_LIMIT, &(mmguiapp->window->trafficlimitclosure)},
		/*connections*/
		{MMGUI_MAIN_CONTROL_SHORTCUT_TRAFFIC_CONNECTIONS, &(mmguiapp->window->trafficconnclosure)},
		/*statistics*/
		{MMGUI_MAIN_CONTROL_SHORTCUT_TRAFFIC_STATS, &(mmguiapp->window->trafficstatsclosure)},
		/*Closures for Contacts page*/
		/*add contact*/
		{MMGUI_MAIN_CONTROL_SHORTCUT_CONTACTS_NEW, &(mmguiapp->window->newcontactclosure)},
		/*remove contact*/
		{MMGUI_MAIN_CONTROL_SHORTCUT_CONTACTS_REMOVE, &(mmguiapp->window->removecontactclosure)},
		/*send sms*/
		{MMGUI_MAIN_CONTROL_SHORTCUT_CONTACTS_SMS, &(mmguiapp->window->smstocontactclosure)}
	};
	
	
	if (mmguiapp == NULL) return FALSE;
	
	error = NULL;
	
	builder = gtk_builder_new();
	
	if (gtk_builder_add_from_file(builder, RESOURCE_MAINWINDOW_UI, &error) == 0) {
		g_printf("User interface file parse error: %s\n", (error->message != NULL) ? error->message : "Unknown");
		g_error_free(error);
		return FALSE;
	}
	/*Translation domain*/
	gtk_builder_set_translation_domain(builder, RESOURCE_LOCALE_DOMAIN);
	/*Loading widgets*/
	for (i=0; i<sizeof(widgetset)/sizeof(struct _mmgui_main_widgetset); i++) {
		*(widgetset[i].widget) = GTK_WIDGET(gtk_builder_get_object(builder, widgetset[i].name));
	}
	/*Loading images for toolbar buttons*/
	for (i=0; i<sizeof(buttonimgset)/sizeof(struct _mmgui_main_widgetset); i++) {
		tbimage = gtk_image_new_from_file(buttonimgset[i].name);
		gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(*(buttonimgset[i].widget)), GTK_WIDGET(tbimage));
		gtk_widget_show(tbimage);
	}
	/*Loading image widgets*/
	for (i=0; i<sizeof(imgwidgetset)/sizeof(struct _mmgui_main_widgetset); i++) {
		gtk_image_set_from_file(GTK_IMAGE(*(imgwidgetset[i].widget)), imgwidgetset[i].name);
	}
	/*Loading pixbufs*/
	for (i=0; i<sizeof(pixbufset)/sizeof(struct _mmgui_main_pixbufset); i++) {
		error = NULL;
		*(pixbufset[i].pixbuf) = gdk_pixbuf_new_from_file(pixbufset[i].name, &error);
		if (*(pixbufset[i].pixbuf) == NULL) {
			g_debug("Pixbuf '%s' load error: %s\n", pixbufset[i].name, (error->message != NULL) ? error->message : "Unknown");
			g_error_free(error);
		}
	}
	/*Using images*/
	gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(mmguiapp->window->aboutdialog), GDK_PIXBUF(mmguiapp->window->mainicon));
	gtk_image_set_from_pixbuf(GTK_IMAGE(mmguiapp->window->welcomeimage), GDK_PIXBUF(mmguiapp->window->mainicon));
	/*Collecting application windows*/
	for (i=0; i<sizeof(appwindows)/sizeof(GtkWidget **); i++) {
		gtk_window_set_application(GTK_WINDOW(*(appwindows[i])), GTK_APPLICATION(mmguiapp->gtkapplication));
		if ((*(appwindows[i]) != NULL) && (mmguiapp->window->mainicon != NULL)) {
			gtk_window_set_icon(GTK_WINDOW(*(appwindows[i])), mmguiapp->window->mainicon);
		}
	}
	/*Setting per-page shortcuts*/
	mmguiapp->window->pageshortcuts = NULL;
	mmguiapp->window->accelgroup = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(mmguiapp->window->window), mmguiapp->window->accelgroup);
	for (i=0; i<sizeof(closureset)/sizeof(struct _mmgui_main_closureset ); i++) {
		shortcutsdata[closureset[i].identifier].mmguiapp = mmguiapp;
		shortcutsdata[closureset[i].identifier].data = GUINT_TO_POINTER(closureset[i].identifier);
		*(closureset[i].closure) = g_cclosure_new_swap(G_CALLBACK(mmgui_main_ui_page_use_shortcuts_signal), &(shortcutsdata[closureset[i].identifier]), NULL);
	}
	/*Menubar*/
	g_object_set(G_OBJECT(mmguiapp->window->window), "application", mmguiapp->gtkapplication, NULL);
	gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(mmguiapp->window->window), TRUE);
	/*Default signal level icon*/
	if (mmguiapp->window->signal0icon != NULL) {
		gtk_image_set_from_pixbuf(GTK_IMAGE(mmguiapp->window->signalimage), mmguiapp->window->signal0icon);
	}
	/*Initialize lists and text fields*/
	mmgui_main_device_list_init(mmguiapp);
	mmgui_main_sms_list_init(mmguiapp);
	mmgui_main_ussd_list_init(mmguiapp);
	mmgui_main_ussd_accelerators_init(mmguiapp);
	mmgui_main_scan_list_init(mmguiapp);
	mmgui_main_contacts_list_init(mmguiapp);
	mmgui_main_traffic_list_init(mmguiapp);
	mmgui_main_traffic_accelerators_init(mmguiapp);
	mmgui_main_traffic_connections_list_init(mmguiapp);
	mmgui_main_traffic_traffic_statistics_list_init(mmguiapp);
	/*Toolbar style*/
	context = gtk_widget_get_style_context(GTK_WIDGET(mmguiapp->window->toolbar));
	gtk_style_context_add_class(context, GTK_STYLE_CLASS_PRIMARY_TOOLBAR);
	/*Binding signal handlers defined by Glade*/
	gtk_builder_connect_signals(builder, mmguiapp);
	/*Builder object is not needed anymore*/
	g_object_unref(G_OBJECT(builder));
	
	mmgui_main_tray_icon_init(mmguiapp);
	
	return TRUE;
}

static void mmgui_main_application_terminate(mmgui_application_t mmguiapp)
{
	if (mmguiapp == NULL) return;
	
	#if GLIB_CHECK_VERSION(2,32,0)
		g_application_quit(G_APPLICATION(mmguiapp->gtkapplication));
	#else	
		GtkWidget *win;
		GList *wlist, *wnext;
		
		wlist = gtk_application_get_windows(GTK_APPLICATION(mmguiapp->gtkapplication));
		while (wlist) {
			win = wlist->data;
			wnext = wlist->next;
			gtk_widget_destroy(GTK_WIDGET(win));
			wlist = wnext;
		}
	#endif
}

static void mmgui_main_application_startup_signal(GtkApplication *application, gpointer data)
{
	mmgui_application_t mmguiapp;
	GtkSettings *gtksettings;
	gboolean showappmenu, showmenubar;
	GMenu *menu, *actsection, *appsection, *prefsection, *helpsection, *quitsection;
	static GActionEntry app_actions[] = {
		{ "section", mmgui_main_ui_section_menu_item_activate_signal, "s", "'devices'", NULL },
		{ "preferences", mmgui_preferences_window_activate_signal, NULL, NULL, NULL },
		{ "help", mmgui_main_ui_help_menu_item_activate_signal, NULL, NULL, NULL },
		{ "about", mmgui_main_ui_about_menu_item_activate_signal, NULL, NULL, NULL },
		{ "quit", mmgui_main_ui_exit_menu_item_activate_signal, NULL, NULL, NULL },
	};
	
	mmguiapp = (mmgui_application_t)data;
	
	if ((application == NULL) || (mmguiapp == NULL)) return;
	
	g_action_map_add_action_entries(G_ACTION_MAP(application), app_actions, G_N_ELEMENTS(app_actions), mmguiapp);
	
	showappmenu = FALSE;
	showmenubar = FALSE;
	
	gtksettings = gtk_settings_get_default();
	g_object_get(G_OBJECT(gtksettings), "gtk-shell-shows-app-menu", &showappmenu, "gtk-shell-shows-menubar", &showmenubar, NULL);
	
	/*Main menu*/
	menu = g_menu_new();
	
	if ((showmenubar) || ((!showappmenu) && (!showmenubar))) {
		/*Classic menubar*/
		/*Pages*/
		appsection = g_menu_new();
		g_menu_append(appsection, _("_Devices"), "app.section::devices");
		mmgui_add_accelerator_with_parameter(application, "<Primary>F1", "app.section", "devices");
		g_menu_append(appsection, _("_SMS"), "app.section::sms");
		mmgui_add_accelerator_with_parameter(application, "<Primary>F2", "app.section", "sms");
		g_menu_append(appsection, _("_USSD"), "app.section::ussd");
		mmgui_add_accelerator_with_parameter(application, "<Primary>F3", "app.section", "ussd");
		g_menu_append(appsection, _("_Info"), "app.section::info");
		mmgui_add_accelerator_with_parameter(application, "<Primary>F4", "app.section", "info");
		g_menu_append(appsection, _("S_can"), "app.section::scan");
		mmgui_add_accelerator_with_parameter(application, "<Primary>F5", "app.section", "scan");
		g_menu_append(appsection, _("_Traffic"), "app.section::traffic");
		mmgui_add_accelerator_with_parameter(application, "<Primary>F6", "app.section", "traffic");
		g_menu_append(appsection, _("C_ontacts"), "app.section::contacts");
		mmgui_add_accelerator_with_parameter(application, "<Primary>F7", "app.section", "contacts");
		/*Quit*/
		quitsection = g_menu_new();
		g_menu_append(quitsection, _("_Quit"), "app.quit");
		mmgui_add_accelerator(application, "<Primary>q", "app.quit");
		/*Actions menu*/
		actsection = g_menu_new();
		g_menu_append_section(actsection, NULL, G_MENU_MODEL(appsection));
		g_menu_append_section(actsection, NULL, G_MENU_MODEL(quitsection));
		g_menu_append_submenu(menu, _("_Actions"), G_MENU_MODEL(actsection));
		/*Preferences*/
		prefsection = g_menu_new();
		g_menu_append(prefsection, _("_Preferences"), "app.preferences");
		mmgui_add_accelerator(application, "<Primary>p", "app.preferences");
		/*Edit menu*/
		g_menu_append_submenu(menu, _("_Edit"), G_MENU_MODEL(prefsection));
		/*Help*/
		helpsection = g_menu_new();
		g_menu_append(helpsection, _("_Help"), "app.help");
		mmgui_add_accelerator(application, "F1", "app.help");
		g_menu_append(helpsection, _("_About"), "app.about");
		/*Help menu*/
		g_menu_append_submenu(menu, _("_Help"), G_MENU_MODEL(helpsection));
		/*Set application menubar*/
		gtk_application_set_menubar(application, G_MENU_MODEL(menu));
	} else if (showappmenu) {
		/*GNOME 3 - style appmenu*/
		/*Toolbar actions*/
		appsection = g_menu_new();
		g_menu_append(appsection, _("Devices"), "app.section::devices");
		mmgui_add_accelerator_with_parameter(application, "<Primary>F1", "app.section", "devices");
		g_menu_append(appsection, _("SMS"), "app.section::sms");
		mmgui_add_accelerator_with_parameter(application, "<Primary>F2", "app.section", "sms");
		g_menu_append(appsection, _("USSD"), "app.section::ussd");
		mmgui_add_accelerator_with_parameter(application, "<Primary>F3", "app.section", "ussd");
		g_menu_append(appsection, _("Info"), "app.section::info");
		mmgui_add_accelerator_with_parameter(application, "<Primary>F4", "app.section", "info");
		g_menu_append(appsection, _("Scan"), "app.section::scan");
		mmgui_add_accelerator_with_parameter(application, "<Primary>F5", "app.section", "scan");
		g_menu_append(appsection, _("Traffic"), "app.section::traffic");
		mmgui_add_accelerator_with_parameter(application, "<Primary>F6", "app.section", "traffic");
		g_menu_append(appsection, _("Contacts"), "app.section::contacts");
		mmgui_add_accelerator_with_parameter(application, "<Primary>F7", "app.section", "contacts");
		g_menu_append_section(menu, NULL, G_MENU_MODEL(appsection));
		/*Preferences*/
		prefsection = g_menu_new();
		g_menu_append(prefsection, _("Preferences"), "app.preferences");
		mmgui_add_accelerator(application, "<Primary>p", "app.preferences");
		g_menu_append_section(menu, NULL, G_MENU_MODEL(prefsection));
		/*Help*/
		helpsection = g_menu_new();
		g_menu_append(helpsection, _("Help"), "app.help");
		mmgui_add_accelerator(application, "F1", "app.help");
		g_menu_append(helpsection, _("About"), "app.about");
		g_menu_append_section(menu, NULL, G_MENU_MODEL(helpsection));
		/*Quit*/
		quitsection = g_menu_new();
		g_menu_append(quitsection, _("Quit"), "app.quit");
		mmgui_add_accelerator(application, "<Primary>q", "app.quit");
		g_menu_append_section(menu, NULL, G_MENU_MODEL(quitsection));
		/*Set application menu*/
		gtk_application_set_app_menu(application, G_MENU_MODEL(menu));
	}
	
	g_object_unref(menu);
}

static void mmgui_main_continue_initialization(mmgui_application_t mmguiapp, mmguicore_t mmguicore)
{
	if (mmguiapp == NULL) return;
	
	/*Upadate library cache: name needed libraries first*/
	mmguiapp->libcache = mmgui_libpaths_cache_new("libnotify", "libcanberra", "libebook-1.2", "libmessaging-menu", "libindicate",  NULL);
	/*Notifications object*/
	mmguiapp->notifications = mmgui_notifications_new(mmguiapp->libcache);
	/*Address books object*/
	mmguiapp->addressbooks = mmgui_addressbooks_new(mmgui_main_event_callback, mmguiapp->libcache, mmguiapp);
	/*Open ayatana interface*/
	mmguiapp->ayatana = mmgui_ayatana_new(mmguiapp->libcache, mmgui_main_ayatana_event_callback, mmguiapp);
	/*Get available devices*/
	if (mmguicore_devices_enum(mmguiapp->core)) {
		mmgui_main_device_list_fill(mmguiapp);
	}
	/*Load UI-specific settings and open device if any*/
	mmgui_main_settings_ui_load(mmguiapp);
	/*Finally show window*/
	if ((!mmguiapp->options->invisible) && (!mmguiapp->options->minimized)) {
		gtk_widget_show(mmguiapp->window->window);
	}
	/*Init SMS spellchecker*/
	#if RESOURCE_SPELLCHECKER_ENABLED
		mmgui_main_sms_spellcheck_init(mmguiapp);
	#endif
	/*Restore window geometry*/
	if (mmguiapp->options->savegeometry) {
		if ((mmguiapp->options->wgwidth >= 1) && (mmguiapp->options->wgheight >= 1)) {
			gtk_window_resize(GTK_WINDOW(mmguiapp->window->window), mmguiapp->options->wgwidth, mmguiapp->options->wgheight);
			gtk_window_move(GTK_WINDOW(mmguiapp->window->window), mmguiapp->options->wgposx, mmguiapp->options->wgposy);
		}
	}
	/*Redraw traffic graph signal*/
	g_signal_connect(G_OBJECT(mmguiapp->window->trafficdrawingarea), "draw", G_CALLBACK(mmgui_main_traffic_speed_plot_draw), mmguiapp);
}

static void mmgui_main_application_activate_signal(GtkApplication *application, gpointer data)
{
	mmgui_application_t mmguiapp;
	GList *windowlist;
		
	mmguiapp = (mmgui_application_t)data;
	
	if ((application == NULL) || (mmguiapp == NULL)) return;
	
	windowlist = gtk_application_get_windows(GTK_APPLICATION(application));
	
	if (windowlist != NULL) {
		/*Present main window*/
		gtk_window_present(GTK_WINDOW(windowlist->data));
		/*Save window state*/
		mmguiapp->options->minimized = FALSE;
		gmm_settings_set_boolean(mmguiapp->settings, "window_state_minimized", mmguiapp->options->minimized);
	} else {
		if (mmgui_main_application_build_user_interface(mmguiapp)) {
			/*Add main window to application window list*/
			gtk_application_add_window(GTK_APPLICATION(application), GTK_WINDOW(mmguiapp->window->window));
			/*Settings object*/
			mmguiapp->settings = gmm_settings_open(RESOURCE_LOCALE_DOMAIN, "settings.conf");
			/*Load global settings*/
			mmgui_main_settings_load(mmguiapp);
			/*Core object*/
			mmguiapp->core = mmguicore_init(mmgui_main_event_callback, mmguiapp->coreoptions, mmguiapp);
			if (mmguiapp->core == NULL) {
				mmgui_main_application_unresolved_error(mmguiapp, _("Error while initialization"), _("Unable to find MMGUI modules. Please check if application installed correctly"));
				return;
			}
			/*Show start dialog to select preferred modules*/
			if ((mmguiapp->coreoptions->mmmodule == NULL) || (mmguiapp->coreoptions->cmmodule == NULL)) {
				mmgui_welcome_window_services_page_open(mmguiapp);
			} else {
				mmguicore_start(mmguiapp->core);
			}
		} else {
			/*If GtkBuilder is unable to load .ui file*/
			mmgui_main_application_unresolved_error(mmguiapp, _("Error while initialization"), _("Interface building error"));
			return;
		}
	}
}

static void mmgui_main_application_shutdown_signal(GtkApplication *application, gpointer data)
{
	mmgui_application_t mmguiapp;
	
	mmguiapp = (mmgui_application_t)data;
	
	if ((application == NULL) || (mmguiapp == NULL)) return;
	
	/*Close library cache*/
	mmgui_libpaths_cache_close(mmguiapp->libcache);
	/*Close notifications interface*/
	mmgui_notifications_close(mmguiapp->notifications);
	/*Close addressbooks interface*/
	mmgui_addressbooks_close(mmguiapp->addressbooks);
	/*Close ayatana interface*/
	mmgui_ayatana_close(mmguiapp->ayatana);
	/*Close core interface*/
	mmguicore_close(mmguiapp->core);
	/*Close settings interface*/
	gmm_settings_close(mmguiapp->settings);
}

static void mmgui_main_modules_list(mmgui_application_t mmguiapp)
{
	GSList *modulelist;
	GSList *iterator;
	mmguimodule_t module;
	guint mtype;
	guint mtypes[2] = {MMGUI_MODULE_TYPE_MODEM_MANAGER, MMGUI_MODULE_TYPE_CONNECTION_MANGER};
	
	if (mmguiapp == NULL) return;
	
	/*Create MMGUI core object*/
	mmguiapp->core = mmguicore_init(NULL, NULL, NULL);
	
	if (mmguiapp->core == NULL) {
		g_printf(_("Unable to find MMGUI modules. Please check if application installed correctly"));
		return;
	}
	
	/*Enumerate modules*/
	modulelist = mmguicore_modules_get_list(mmguiapp->core);
	
	if (modulelist != NULL) {
		for (mtype = 0; mtype < sizeof(mtypes)/sizeof(guint); mtype++) {
			/*New module type*/
			switch (mtype) {
				case MMGUI_MODULE_TYPE_MODEM_MANAGER:
					g_printf(_("Modem management modules:\n"));
					g_printf("       %21s | %s\n", _("Module"), _("Description"));
					break;
				case MMGUI_MODULE_TYPE_CONNECTION_MANGER:
					g_printf(_("Connection management modules:\n"));
					g_printf("       %21s | %s\n", _("Module"), _("Description"));
					break;
				default:
					break;
			}
			for (iterator=modulelist; iterator; iterator=iterator->next) {
				module = (mmguimodule_t)iterator->data;
				if (module->type == mtypes[mtype]) {
					/*New module*/
					g_printf("%3s%3s %15s | %s\n", (module->activationtech != MMGUI_SVCMANGER_ACTIVATION_TECH_NA) ? "[A]" : "[-]", module->applicable ? "[R]" : "[-]", module->shortname, module->description);
				}
			}
		}
		g_slist_free(modulelist);
	}
	
	mmguicore_close(mmguiapp->core);
}

#ifdef __GLIBC__
static void mmgui_main_application_backtrace_signal_handler(int sig, siginfo_t *info, ucontext_t *ucontext)
{
	void *trace[10];
	gchar **tracepoints = (char **)NULL;
	gint i, tracelen = 0;
	
	if (sig == SIGSEGV) {
		tracelen = backtrace(trace, 10);
		#if (defined(__i386__))
			trace[1] = (void *) ucontext->uc_mcontext.gregs[REG_EIP];
		#elif (defined(__x86_64__))
			trace[1] = (void *) ucontext->uc_mcontext.gregs[REG_RIP];
		#elif (defined(__ppc__) || defined(__powerpc__))
			trace[1] = (void *) ucontext->uc_mcontext.regs->nip;
		#elif (defined(__arm__))
			trace[1] = (void *) ucontext->uc_mcontext.arm_pc;
		#endif
		
		g_printf(_("Segmentation fault at address: %p\n"), info->si_addr);
		
		if (tracelen > 0) {
			g_printf(_("Stack trace:\n"));
			tracepoints = backtrace_symbols(trace, tracelen);
			for (i=1; i<tracelen; ++i) {
				g_printf("%i. %s\n", i, tracepoints[i]);
			}
			free(tracepoints);
		}
	}
	
	exit(1);
}
#endif

static void mmgui_main_application_termination_signal_handler(int sig, siginfo_t *info, ucontext_t *ucontext)
{
	GApplication *application;
	GAction *quitaction;
		
	application = g_application_get_default();
	
	if (application != NULL) {
		/*Quit action*/
		quitaction = g_action_map_lookup_action(G_ACTION_MAP(application), "quit");
		if (quitaction != NULL) {
			/*Activate action used for appmenu*/
			g_action_activate(quitaction, NULL);
		}
	}
}

int main(int argc, char *argv[])
{
	mmgui_application_t mmguiapp;
	struct sigaction termsa;
	GOptionContext *optcontext;
	gboolean listmodules;
	GError *error;
	gint status;
	
	mmguiapp = g_new0(struct _mmgui_application, 1);
	mmguiapp->options = g_new0(struct _mmgui_cli_options, 1);
	mmguiapp->window = g_new0(struct _mmgui_main_window, 1);
	mmguiapp->coreoptions = g_new0(struct _mmgui_core_options, 1);
	
	/*Default core options*/
	mmguiapp->options->invisible = FALSE;
	mmguiapp->coreoptions->mmmodule = NULL;
	mmguiapp->coreoptions->cmmodule = NULL;
	mmguiapp->coreoptions->trafficenabled = FALSE;
	mmguiapp->coreoptions->trafficamount = 150;
	mmguiapp->coreoptions->trafficunits = 0;
	mmguiapp->coreoptions->trafficmessage = NULL;
	mmguiapp->coreoptions->trafficaction = 0;
	mmguiapp->coreoptions->trafficfull = 0;
	mmguiapp->coreoptions->trafficexecuted = FALSE;
	mmguiapp->coreoptions->timeenabled = FALSE;
	mmguiapp->coreoptions->timeamount = 60;
	mmguiapp->coreoptions->timeunits = 0;
	mmguiapp->coreoptions->timemessage = NULL;
	mmguiapp->coreoptions->timeaction = 0;
	mmguiapp->coreoptions->timefull = 0;
	mmguiapp->coreoptions->timeexecuted = FALSE;
	listmodules = FALSE;
	
	//Predefined CLI options
	GOptionEntry entries[] = {
		{ "invisible",   'i', 0, G_OPTION_ARG_NONE,   &mmguiapp->options->invisible,    _("Do not show window on start"), NULL },
		{ "mmmodule",    'm', 0, G_OPTION_ARG_STRING, &mmguiapp->coreoptions->mmmodule, _("Use specified modem management module"), NULL },
		{ "cmmodule",    'c', 0, G_OPTION_ARG_STRING, &mmguiapp->coreoptions->cmmodule, _("Use specified connection management module"), NULL },
		{ "listmodules", 'l', 0, G_OPTION_ARG_NONE,   &listmodules,                     _("List all available modules and exit"), NULL },
		{ NULL }
	};
	
	//Locale
	#ifndef LC_ALL
		#define LC_ALL 0
	#endif
	
	//Backtrace handler
	#ifdef __GLIBC__
		struct sigaction btsa;
		
		btsa.sa_sigaction = (void *)mmgui_main_application_backtrace_signal_handler;
		sigemptyset(&btsa.sa_mask);
		btsa.sa_flags = SA_RESTART | SA_SIGINFO;
		/*Segmentation fault signal*/
		sigaction(SIGSEGV, &btsa, NULL);
	#endif
	
	/*Termination handler*/
	termsa.sa_sigaction = (void *)mmgui_main_application_termination_signal_handler;
	sigemptyset(&termsa.sa_mask);
	termsa.sa_flags = SA_RESTART | SA_SIGINFO;
	/*Termination, interrruption and hungup signals*/
	sigaction(SIGTERM, &termsa, NULL);
	sigaction(SIGINT, &termsa, NULL);
	sigaction(SIGHUP, &termsa, NULL);
	
	setlocale(LC_ALL, "");
	bindtextdomain(RESOURCE_LOCALE_DOMAIN, RESOURCE_LOCALE_DIR);
	bind_textdomain_codeset(RESOURCE_LOCALE_DOMAIN, "UTF-8");
	textdomain(RESOURCE_LOCALE_DOMAIN);
	
	g_set_application_name("Modem Manager GUI");
	
	//CLI options parsing
	optcontext = g_option_context_new(_("- tool for EDGE/3G/4G modem specific functions control"));
	g_option_context_add_main_entries(optcontext, entries, RESOURCE_LOCALE_DOMAIN);
	g_option_context_add_group(optcontext, gtk_get_option_group(TRUE));
	
	error = NULL;
	
	if (!g_option_context_parse(optcontext, &argc, &argv, &error)) {
		g_printf(_("Command line option parsing failed: %s\n"), (error->message != NULL) ? error->message : "Unknown");
		g_option_context_free(optcontext);
		g_error_free(error);
		g_free(mmguiapp->options);
		g_free(mmguiapp->window);
		g_free(mmguiapp);
		return EXIT_FAILURE;
	}
	
	g_option_context_free(optcontext);
	
	if (listmodules) {
		/*Modules listing*/
		mmgui_main_modules_list(mmguiapp);
		/*Exit*/
		g_free(mmguiapp->coreoptions);
		g_free(mmguiapp->options);
		g_free(mmguiapp->window);
		g_free(mmguiapp);
		return EXIT_SUCCESS;
	}
	
	//Run GTK+ application
	mmguiapp->gtkapplication = gtk_application_new("org.gtk.ModemManagerGUI", 0);
		
	g_signal_connect(mmguiapp->gtkapplication, "startup", G_CALLBACK(mmgui_main_application_startup_signal), mmguiapp);
	
	g_signal_connect(mmguiapp->gtkapplication, "activate", G_CALLBACK(mmgui_main_application_activate_signal), mmguiapp);
	
	g_signal_connect(mmguiapp->gtkapplication, "shutdown", G_CALLBACK(mmgui_main_application_shutdown_signal), mmguiapp);
	
	status = g_application_run(G_APPLICATION(mmguiapp->gtkapplication), argc, argv);
	
	//Free previously allocated resources
	g_object_unref(G_OBJECT(mmguiapp->gtkapplication));
	
	if (mmguiapp->options != NULL) {
		g_free(mmguiapp->options);
	}
	if (mmguiapp->window != NULL) {
		g_free(mmguiapp->window);
	}
	if (mmguiapp->coreoptions != NULL) {
		if (mmguiapp->coreoptions->mmmodule != NULL) {
			g_free(mmguiapp->coreoptions->mmmodule);
		}
		if (mmguiapp->coreoptions->cmmodule != NULL) {
			g_free(mmguiapp->coreoptions->cmmodule);
		}
		if (mmguiapp->coreoptions->trafficmessage != NULL) {
			g_free(mmguiapp->coreoptions->trafficmessage);
		}
		if (mmguiapp->coreoptions->timemessage != NULL) {
			g_free(mmguiapp->coreoptions->timemessage);
		}
		g_free(mmguiapp->coreoptions);
	}
	if (mmguiapp != NULL) {
		g_free(mmguiapp);
	}
	
	return status;
}
