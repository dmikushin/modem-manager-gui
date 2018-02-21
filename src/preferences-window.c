/*
 *      preferences-window.c
 *      
 *      Copyright 2015-2018 Alex <alex@linuxonly.ru>
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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../resources.h"
#include "strformat.h"
#include "main.h"
#include "preferences-window.h"

enum _mmgui_preferences_window_service_list {
	MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_ICON = 0,
	MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_NAME,
	MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_MODULE,
	MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_SERVICENAME,
	MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_COMPATIBILITY,
	MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_AVAILABLE,
	MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_COMPATIBLE,
	MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_COLUMNS
};

static void mmgui_preferences_window_update_compatible_modules(mmgui_application_t mmguiapp, GtkComboBox *currentcombo, GtkComboBox *othercombo);
static void mmgui_preferences_window_services_page_modules_combo_set_sensitive(GtkCellLayout *cell_layout, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data);
static void mmgui_preferences_window_services_page_modules_combo_fill(GtkComboBox *combo, GSList *modules, gint type, mmguimodule_t currentmodule);
static gboolean mmgui_main_application_is_in_autostart(mmgui_application_t mmguiapp);
static gboolean mmgui_main_application_add_to_autostart(mmgui_application_t mmguiapp);
static gboolean mmgui_main_application_remove_from_autostart(mmgui_application_t mmguiapp);


static void mmgui_preferences_window_update_compatible_modules(mmgui_application_t mmguiapp, GtkComboBox *currentcombo, GtkComboBox *othercombo)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *compatibility, *servicename;
	gchar **comparray;
	gboolean compatible, available;
	gint i;
	GtkTreePath *comppath;
	GtkTreeRowReference *compreference;
	
	if ((mmguiapp == NULL) || (currentcombo == NULL) || (othercombo == NULL)) return;
	
	model = gtk_combo_box_get_model(currentcombo);
	if (model != NULL) {
		if (gtk_combo_box_get_active_iter(currentcombo, &iter)) {
			/*Get current module compatibility string*/
			gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_COMPATIBILITY, &compatibility, -1);
			if (compatibility != NULL) {
				/*Get list of compatible service names*/
				comparray = g_strsplit(compatibility, ";", -1);
				/*Iterate through other available modules*/
				model = gtk_combo_box_get_model(GTK_COMBO_BOX(othercombo));
				if (model != NULL) {
					compreference = NULL;
					if (gtk_tree_model_get_iter_first(model, &iter)) {
						do {
							/*Get other available module service name*/
							gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_AVAILABLE, &available,
																			MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_SERVICENAME, &servicename,
																			-1);
							if (servicename != NULL) {
								/*Try to find this name in array of compatible service names*/
								compatible = FALSE;
								if (available) {
									i = 0;
									while (comparray[i] != NULL) {
										if (g_str_equal(comparray[i], servicename)) {
											if (compreference == NULL) {
												comppath = gtk_tree_model_get_path(model, &iter);
												if (comppath != NULL) {
													compreference = gtk_tree_row_reference_new(model, comppath);
													gtk_tree_path_free(comppath);
												}
											}
											compatible = TRUE;
											break;
										}
										i++;
									}
								}
								/*Set flag*/
								gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_COMPATIBLE, compatible, -1);
								/*Free name*/
								g_free(servicename);
							} else {
								/*Undefined is always compatible*/
								if (compreference == NULL) {
									comppath = gtk_tree_model_get_path(model, &iter);
									if (comppath != NULL) {
										compreference = gtk_tree_row_reference_new(model, comppath);
										gtk_tree_path_free(comppath);
									}
								}
								gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_COMPATIBLE, TRUE, -1);
							}
						} while (gtk_tree_model_iter_next(model, &iter));
					}
					/*Try to select first compatible module if needed*/
					if (compreference != NULL) {
						compatible = FALSE;
						servicename = NULL;
						/*If other selected module is compatible?*/
						if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(othercombo), &iter)) {
							gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_COMPATIBLE, &compatible,
																			MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_SERVICENAME, &servicename,
																			-1);
						}
						/*If not, select one avoiding undefined*/
						if ((!compatible) || (servicename == NULL)) {
							comppath = gtk_tree_row_reference_get_path(compreference);
							if (comppath != NULL) {
								if (gtk_tree_model_get_iter(model, &iter, comppath)) {
									gtk_combo_box_set_active_iter(GTK_COMBO_BOX(othercombo), &iter);
								}
								gtk_tree_path_free(comppath);
							}
						}
						/*Free resources*/
						gtk_tree_row_reference_free(compreference);
						if (servicename != NULL) {
							g_free(servicename);
						}
					}
				}
				/*Free resources*/
				g_strfreev(comparray);
				g_free(compatibility);
			}
		}
	}
}

void mmgui_preferences_window_services_page_mm_modules_combo_changed(GtkComboBox *widget, gpointer data)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	g_signal_handlers_block_by_func(mmguiapp->window->prefmodulescmcombo, mmgui_preferences_window_services_page_cm_modules_combo_changed, mmguiapp);
	mmgui_preferences_window_update_compatible_modules(mmguiapp, widget, GTK_COMBO_BOX(mmguiapp->window->prefmodulescmcombo));
	g_signal_handlers_unblock_by_func(mmguiapp->window->prefmodulescmcombo, mmgui_preferences_window_services_page_cm_modules_combo_changed, mmguiapp);
}

void mmgui_preferences_window_services_page_cm_modules_combo_changed(GtkComboBox *widget, gpointer data)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	g_signal_handlers_block_by_func(mmguiapp->window->prefmodulesmmcombo, mmgui_preferences_window_services_page_mm_modules_combo_changed, mmguiapp);
	mmgui_preferences_window_update_compatible_modules(mmguiapp, widget, GTK_COMBO_BOX(mmguiapp->window->prefmodulesmmcombo));
	g_signal_handlers_unblock_by_func(mmguiapp->window->prefmodulesmmcombo, mmgui_preferences_window_services_page_mm_modules_combo_changed, mmguiapp);
}

static void mmgui_preferences_window_services_page_modules_combo_set_sensitive(GtkCellLayout *cell_layout, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data)
{
	gboolean available, compatible;
	
	gtk_tree_model_get(tree_model, iter, MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_AVAILABLE, &available,
										MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_COMPATIBLE, &compatible,
										-1);
	
	g_object_set(cell, "sensitive", available && compatible, NULL);
}

static void mmgui_preferences_window_services_page_modules_combo_fill(GtkComboBox *combo, GSList *modules, gint type, mmguimodule_t currentmodule)
{
	GSList *iterator;
	GtkListStore *store;
	gchar *icon;
	GtkCellArea *area;
	GtkCellRenderer *renderer;
	GtkTreeIter iter;
	mmguimodule_t module;
	gint moduleid, curid;
	
	if ((combo == NULL) || (modules == NULL)) return;
		
	store = gtk_list_store_new(MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	
	moduleid = -1;
	curid = 0;
	
	for (iterator=modules; iterator; iterator=iterator->next) {
		module = iterator->data;
		if (module->type == type) {
			if (module->applicable) {
				icon = "user-available";
			} else if (module->activationtech == MMGUI_SVCMANGER_ACTIVATION_TECH_SYSTEMD) {
				icon = "user-away";
			} else if (module->activationtech == MMGUI_SVCMANGER_ACTIVATION_TECH_DBUS) {
				icon = "user-away";
			} else {
				icon = "user-offline";
			}
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter,
								MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_ICON, icon,
								MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_NAME, module->description,
								MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_MODULE, module->shortname,
								MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_SERVICENAME, module->servicename,
								MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_COMPATIBILITY, module->compatibility,
								MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_AVAILABLE, module->applicable || (module->activationtech != MMGUI_SVCMANGER_ACTIVATION_TECH_NA),
								MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_COMPATIBLE, TRUE,
								-1);
			if (currentmodule != NULL) {
				if (currentmodule == module) {
					moduleid = curid;
				}
			} else {
				if (((module->applicable) || (module->activationtech != MMGUI_SVCMANGER_ACTIVATION_TECH_NA)) && (moduleid == -1)) {
					moduleid = curid;
				}
			}
			curid++;
		}
	}
	
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
						MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_ICON, "user-invisible",
						MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_NAME, _("Undefined"),
						MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_MODULE, "undefined",
						MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_SERVICENAME, NULL,
						MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_COMPATIBILITY, NULL,
						MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_AVAILABLE, TRUE,
						MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_COMPATIBLE, TRUE,
						-1);
	
	if (moduleid == -1) {
		moduleid = curid;
	}
	
	area = gtk_cell_layout_get_area(GTK_CELL_LAYOUT(combo));
	
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, TRUE);
	gtk_cell_area_attribute_connect(area, renderer, "icon-name",  MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_ICON);
	
	gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(combo), renderer, mmgui_preferences_window_services_page_modules_combo_set_sensitive, NULL, NULL);
	
	renderer = gtk_cell_renderer_text_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, TRUE, FALSE, FALSE);
	gtk_cell_area_attribute_connect(area, renderer, "text", MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_NAME);
	
	gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(combo), renderer, mmgui_preferences_window_services_page_modules_combo_set_sensitive, NULL, NULL);
	
	gtk_combo_box_set_model(GTK_COMBO_BOX(combo), GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), moduleid);
}

void mmgui_preferences_window_activate_signal(GSimpleAction *action, GVariant *parameter, gpointer data)
{
	mmgui_application_t mmguiapp;
	gint response;
	gchar *strcolor, *modulename;
	gboolean autostart;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gint argcp, i, p;
	gchar **argvp;
	gchar *rawcmd;
	gboolean correctform;
		
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	if ((mmguiapp->options == NULL) || (mmguiapp->settings == NULL)) return;
	
	autostart = mmgui_main_application_is_in_autostart(mmguiapp);
		
	/*Show SMS settings*/
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefsmsconcat), mmguiapp->options->concatsms);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefsmsexpand), mmguiapp->options->smsexpandfolders);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefsmsoldontop), mmguiapp->options->smsoldontop);
	gtk_range_set_value(GTK_RANGE(mmguiapp->window->prefsmsvalidityscale), (gdouble)mmguiapp->options->smsvalidityperiod);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefsmsreportcb), mmguiapp->options->smsdeliveryreport);
	if (mmguiapp->options->smscustomcommand != NULL) {
		gtk_entry_set_text(GTK_ENTRY(mmguiapp->window->prefsmscommandentry), mmguiapp->options->smscustomcommand);
	}
	/*Show behaviour settings*/
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefbehaviourhide), mmguiapp->options->hidetotray);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefbehavioursounds), mmguiapp->options->usesounds);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefbehaviourgeom), mmguiapp->options->savegeometry);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefbehaviourautostart), autostart);
	/*Show graph color and movement direction settings*/
	#if GTK_CHECK_VERSION(3,4,0)
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(mmguiapp->window->preftrafficrxcolor), (const GdkRGBA *)&mmguiapp->options->rxtrafficcolor);
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(mmguiapp->window->preftraffictxcolor), (const GdkRGBA *)&mmguiapp->options->txtrafficcolor);
	#else
		gtk_color_button_set_use_alpha(GTK_COLOR_BUTTON(mmguiapp->window->preftrafficrxcolor), FALSE);
		gtk_color_button_set_use_alpha(GTK_COLOR_BUTTON(mmguiapp->window->preftraffictxcolor), FALSE);
		gtk_color_button_set_color(GTK_COLOR_BUTTON(mmguiapp->window->preftrafficrxcolor), (const GdkColor *)&(mmguiapp->options->rxtrafficcolor));
		gtk_color_button_set_color(GTK_COLOR_BUTTON(mmguiapp->window->preftraffictxcolor), (const GdkColor *)&(mmguiapp->options->txtrafficcolor));
	#endif
	gtk_combo_box_set_active(GTK_COMBO_BOX(mmguiapp->window->preftrafficmovdircombo), mmguiapp->options->graphrighttoleft ? 1 : 0);
	/*Show modules settings*/
	gtk_range_set_value(GTK_RANGE(mmguiapp->window->prefenabletimeoutscale), (gdouble)mmguiapp->coreoptions->enabletimeout);
	gtk_range_set_value(GTK_RANGE(mmguiapp->window->prefsendsmstimeoutscale), (gdouble)mmguiapp->coreoptions->sendsmstimeout);
	gtk_range_set_value(GTK_RANGE(mmguiapp->window->prefsendussdtimeoutscale), (gdouble)mmguiapp->coreoptions->sendussdtimeout);
	gtk_range_set_value(GTK_RANGE(mmguiapp->window->prefscannetworkstimeoutscale), (gdouble)mmguiapp->coreoptions->scannetworkstimeout);
	
	/*Preferred modem manager*/
	if (gtk_combo_box_get_model(GTK_COMBO_BOX(mmguiapp->window->prefmodulesmmcombo)) == NULL) {
		g_signal_handlers_block_by_func(mmguiapp->window->prefmodulesmmcombo, mmgui_preferences_window_services_page_mm_modules_combo_changed, mmguiapp);
		mmgui_preferences_window_services_page_modules_combo_fill(GTK_COMBO_BOX(mmguiapp->window->prefmodulesmmcombo), mmguiapp->core->modules, MMGUI_MODULE_TYPE_MODEM_MANAGER, (mmguimodule_t)mmguiapp->core->moduleptr);
		g_signal_handlers_unblock_by_func(mmguiapp->window->prefmodulesmmcombo, mmgui_preferences_window_services_page_mm_modules_combo_changed, mmguiapp);
	} else if (mmguiapp->coreoptions->mmmodule != NULL) {
		model = gtk_combo_box_get_model(GTK_COMBO_BOX(mmguiapp->window->prefmodulesmmcombo));
		if (model != NULL) {
			if (gtk_tree_model_get_iter_first(model, &iter)) {
				do {
					gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_MODULE, &modulename, -1);
					if (modulename != NULL) {
						if (g_str_equal(modulename, mmguiapp->coreoptions->mmmodule)) {
							g_free(modulename);
							gtk_combo_box_set_active_iter(GTK_COMBO_BOX(mmguiapp->window->prefmodulesmmcombo), &iter);
							break;
						}
						g_free(modulename);
					}
				} while (gtk_tree_model_iter_next(model, &iter));
			}
		}
	}
	/*Preferred connection manager*/
	if (gtk_combo_box_get_model(GTK_COMBO_BOX(mmguiapp->window->prefmodulescmcombo)) == NULL) {
		g_signal_handlers_block_by_func(mmguiapp->window->prefmodulescmcombo, mmgui_preferences_window_services_page_cm_modules_combo_changed, mmguiapp);
		mmgui_preferences_window_services_page_modules_combo_fill(GTK_COMBO_BOX(mmguiapp->window->prefmodulescmcombo), mmguiapp->core->modules, MMGUI_MODULE_TYPE_CONNECTION_MANGER, (mmguimodule_t)mmguiapp->core->cmoduleptr);
		g_signal_handlers_unblock_by_func(mmguiapp->window->prefmodulescmcombo, mmgui_preferences_window_services_page_cm_modules_combo_changed, mmguiapp);
	} else if (mmguiapp->coreoptions->cmmodule != NULL) {
		model = gtk_combo_box_get_model(GTK_COMBO_BOX(mmguiapp->window->prefmodulescmcombo));
		if (model != NULL) {
			if (gtk_tree_model_get_iter_first(model, &iter)) {
				do {
					gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_MODULE, &modulename, -1);
					if (modulename != NULL) {
						if (g_str_equal(modulename, mmguiapp->coreoptions->cmmodule)) {
							g_free(modulename);
							gtk_combo_box_set_active_iter(GTK_COMBO_BOX(mmguiapp->window->prefmodulescmcombo), &iter);
							break;
						}
						g_free(modulename);
					}
				} while (gtk_tree_model_iter_next(model, &iter));
			}
		}
	}
	
	/*Show active pages*/
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagessmscb), mmguiapp->options->smspageenabled);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagesussdcb), mmguiapp->options->ussdpageenabled);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagesinfocb), mmguiapp->options->infopageenabled);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagesscancb), mmguiapp->options->scanpageenabled);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagestrafficcb), mmguiapp->options->trafficpageenabled);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagescontactscb), mmguiapp->options->contactspageenabled);
	
	
	response = gtk_dialog_run(GTK_DIALOG(mmguiapp->window->prefdialog));
	
	
	if (response > 0) {
		/*Save SMS settings*/
		mmguiapp->options->concatsms = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefsmsconcat));
		gmm_settings_set_boolean(mmguiapp->settings, "sms_concatenation", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefsmsconcat)));
		mmguiapp->options->smsexpandfolders = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefsmsexpand));
		gmm_settings_set_boolean(mmguiapp->settings, "sms_expand_folders", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefsmsexpand)));
		mmguiapp->options->smsoldontop = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefsmsoldontop));
		gmm_settings_set_boolean(mmguiapp->settings, "sms_old_on_top", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefsmsoldontop)));
		mmguiapp->options->smsvalidityperiod = (gint)gtk_range_get_value(GTK_RANGE(mmguiapp->window->prefsmsvalidityscale));
		gmm_settings_set_int(mmguiapp->settings, "sms_validity_period", (gint)gtk_range_get_value(GTK_RANGE(mmguiapp->window->prefsmsvalidityscale)));
		mmguiapp->options->smsdeliveryreport = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefsmsreportcb));
		gmm_settings_set_boolean(mmguiapp->settings, "sms_send_delivery_report", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefsmsreportcb)));
		/*Custom command validation*/
		if (mmguiapp->options->smscustomcommand != NULL) {
			g_free(mmguiapp->options->smscustomcommand);
		}
		if (g_shell_parse_argv(gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->prefsmscommandentry)), &argcp, &argvp, NULL)) {
			correctform = TRUE;
			for (i = 0; i < argcp; i++) {
				for (p = 0; argvp[i][p] != '\0'; p++) {
					if (argvp[i][p] == '%') {
						if ((argvp[i][p + 1] == 'n') || (argvp[i][p + 1] == 't')) {
							p++;
						} else {
							correctform = FALSE;
							break;
						}
					}
				}
			}
			g_strfreev(argvp);
			if (correctform) {
				/*Update command*/
				mmguiapp->options->smscustomcommand = g_strdup(gtk_entry_get_text(GTK_ENTRY(mmguiapp->window->prefsmscommandentry)));
				/*Save custom command in escaped form*/
				rawcmd = g_strescape(mmguiapp->options->smscustomcommand, NULL);
				gmm_settings_set_string(mmguiapp->settings, "sms_custom_command", rawcmd);
				g_free(rawcmd);
			} else {
				/*Update command*/
				mmguiapp->options->smscustomcommand = g_strdup("");
				/*Save custom command in escaped form*/
				gmm_settings_set_string(mmguiapp->settings, "sms_custom_command", "");
			}
			
		} else {
			/*Update command*/
			mmguiapp->options->smscustomcommand = g_strdup("");
			/*Save custom command in escaped form*/
			gmm_settings_set_string(mmguiapp->settings, "sms_custom_command", "");
		}
		
		/*Save program behaviour settings*/
		mmguiapp->options->hidetotray = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefbehaviourhide));
		gmm_settings_set_boolean(mmguiapp->settings, "behaviour_hide_to_tray", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefbehaviourhide)));
		mmguiapp->options->usesounds = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefbehavioursounds));
		gmm_settings_set_boolean(mmguiapp->settings, "behaviour_use_sounds", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefbehavioursounds)));
		mmguiapp->options->savegeometry = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefbehaviourgeom));
		gmm_settings_set_boolean(mmguiapp->settings, "behaviour_save_geometry", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefbehaviourgeom)));
		/*Autostart*/
		if ((gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefbehaviourautostart))) && (!autostart)) {
			mmgui_main_application_add_to_autostart(mmguiapp);
		} else if ((!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefbehaviourautostart))) && (autostart)) {
			mmgui_main_application_remove_from_autostart(mmguiapp);
		}
		/*Save graph colors and movement direction*/
		#if GTK_CHECK_VERSION(3,4,0)
			gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(mmguiapp->window->preftrafficrxcolor), &(mmguiapp->options->rxtrafficcolor));
			strcolor = gdk_rgba_to_string((const GdkRGBA *)&mmguiapp->options->rxtrafficcolor);
			gmm_settings_set_string(mmguiapp->settings, "graph_rx_color", strcolor);
			g_free(strcolor);
			gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(mmguiapp->window->preftraffictxcolor), &(mmguiapp->options->txtrafficcolor)); 
			strcolor = gdk_rgba_to_string((const GdkRGBA *)&mmguiapp->options->txtrafficcolor);
			gmm_settings_set_string(mmguiapp->settings, "graph_tx_color", strcolor);
			g_free(strcolor);
		#else
			gtk_color_button_get_color(GTK_COLOR_BUTTON(mmguiapp->window->preftrafficrxcolor), &(mmguiapp->options->rxtrafficcolor));
			strcolor = gdk_color_to_string((const GdkColor *)&mmguiapp->options->rxtrafficcolor);
			gmm_settings_set_string(mmguiapp->settings, "graph_rx_color", strcolor);
			g_free(strcolor);
			gtk_color_button_get_color(GTK_COLOR_BUTTON(mmguiapp->window->preftraffictxcolor), &(mmguiapp->options->txtrafficcolor));
			strcolor = gdk_color_to_string((const GdkColor *)&mmguiapp->options->txtrafficcolor);
			gmm_settings_set_string(mmguiapp->settings, "graph_tx_color", strcolor);
			g_free(strcolor);
		#endif
		mmguiapp->options->graphrighttoleft = (gboolean)gtk_combo_box_get_active(GTK_COMBO_BOX(mmguiapp->window->preftrafficmovdircombo));
		gmm_settings_set_boolean(mmguiapp->settings, "graph_right_to_left", (gboolean)gtk_combo_box_get_active(GTK_COMBO_BOX(mmguiapp->window->preftrafficmovdircombo)));
		/*Save and apply modules settings*/
		mmguiapp->coreoptions->enabletimeout = (gint)gtk_range_get_value(GTK_RANGE(mmguiapp->window->prefenabletimeoutscale));
		gmm_settings_set_int(mmguiapp->settings, "modules_enable_device_timeout", (gint)gtk_range_get_value(GTK_RANGE(mmguiapp->window->prefenabletimeoutscale)));
		mmguiapp->coreoptions->sendsmstimeout = (gint)gtk_range_get_value(GTK_RANGE(mmguiapp->window->prefsendsmstimeoutscale));
		gmm_settings_set_int(mmguiapp->settings, "modules_send_sms_timeout", (gint)gtk_range_get_value(GTK_RANGE(mmguiapp->window->prefsendsmstimeoutscale)));
		mmguiapp->coreoptions->sendussdtimeout = (gint)gtk_range_get_value(GTK_RANGE(mmguiapp->window->prefsendussdtimeoutscale));
		gmm_settings_set_int(mmguiapp->settings, "modules_send_ussd_timeout", (gint)gtk_range_get_value(GTK_RANGE(mmguiapp->window->prefsendussdtimeoutscale)));
		mmguiapp->coreoptions->scannetworkstimeout = (gint)gtk_range_get_value(GTK_RANGE(mmguiapp->window->prefscannetworkstimeoutscale));
		gmm_settings_set_int(mmguiapp->settings, "modules_scan_networks_timeout", (gint)gtk_range_get_value(GTK_RANGE(mmguiapp->window->prefscannetworkstimeoutscale)));
		mmguicore_modules_mm_set_timeouts(mmguiapp->core, MMGUI_DEVICE_OPERATION_ENABLE, mmguiapp->coreoptions->enabletimeout,
														MMGUI_DEVICE_OPERATION_SEND_SMS, mmguiapp->coreoptions->sendsmstimeout,
														MMGUI_DEVICE_OPERATION_SEND_USSD, mmguiapp->coreoptions->sendussdtimeout,
														MMGUI_DEVICE_OPERATION_SCAN, mmguiapp->coreoptions->scannetworkstimeout,
														-1);
		/*Save preferred modules*/
		model = gtk_combo_box_get_model(GTK_COMBO_BOX(mmguiapp->window->prefmodulesmmcombo));
		if (model != NULL) {
			if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(mmguiapp->window->prefmodulesmmcombo), &iter)) {
				if (mmguiapp->coreoptions->mmmodule != NULL) {
					g_free(mmguiapp->coreoptions->mmmodule);
				}
				gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_MODULE, &mmguiapp->coreoptions->mmmodule, -1);
				gmm_settings_set_string(mmguiapp->settings, "modules_preferred_modem_manager", mmguiapp->coreoptions->mmmodule);
			}
		}
		model = gtk_combo_box_get_model(GTK_COMBO_BOX(mmguiapp->window->prefmodulescmcombo));
		if (model != NULL) {
			if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(mmguiapp->window->prefmodulescmcombo), &iter)) {
				if (mmguiapp->coreoptions->cmmodule != NULL) {
					g_free(mmguiapp->coreoptions->cmmodule);
				}
				gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, MMGUI_PREFERENCES_WINDOW_SERVICE_LIST_MODULE, &mmguiapp->coreoptions->cmmodule, -1);
				gmm_settings_set_string(mmguiapp->settings, "modules_preferred_connection_manager", mmguiapp->coreoptions->cmmodule);
			}
		}
		
		/*Save active pages*/
		mmguiapp->options->smspageenabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagessmscb));
		gmm_settings_set_boolean(mmguiapp->settings, "pages_sms_enabled", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagessmscb)));
		mmguiapp->options->ussdpageenabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagesussdcb));
		gmm_settings_set_boolean(mmguiapp->settings, "pages_ussd_enabled", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagesussdcb)));
		mmguiapp->options->infopageenabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagesinfocb));
		gmm_settings_set_boolean(mmguiapp->settings, "pages_info_enabled", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagesinfocb)));
		mmguiapp->options->scanpageenabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagesscancb));
		gmm_settings_set_boolean(mmguiapp->settings, "pages_scan_enabled", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagesscancb)));
		mmguiapp->options->trafficpageenabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagestrafficcb));
		gmm_settings_set_boolean(mmguiapp->settings, "pages_traffic_enabled", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagestrafficcb)));
		mmguiapp->options->contactspageenabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagescontactscb));
		gmm_settings_set_boolean(mmguiapp->settings, "pages_contacts_enabled", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->prefactivepagescontactscb)));
		mmgui_main_window_update_active_pages(mmguiapp);
	}
	
	gtk_widget_hide(mmguiapp->window->prefdialog);
}

gchar *mmgui_preferences_window_message_validity_scale_value_format(GtkScale *scale, gdouble value, gpointer data)
{
	return mmgui_str_format_message_validity_period(value);	
}

gchar *mmgui_preferences_window_timeout_scale_value_format(GtkScale *scale, gdouble value, gpointer data)
{
	return mmgui_str_format_operation_timeout_period(value);
}

static gboolean mmgui_main_application_is_in_autostart(mmgui_application_t mmguiapp)
{
	gchar *linkfile, *desktopfile;
	struct stat statbuf;
	gssize len;
	
	if (mmguiapp == NULL) return FALSE;
	
	/*Form autostart link path using XDG standard*/
	linkfile = g_build_filename(g_get_user_config_dir(), "autostart", "modem-manager-gui.desktop", NULL);
	
	if (linkfile == NULL) return FALSE;
	
	/*Test if link points to the right file*/
	if (lstat(linkfile, &statbuf) != -1) {
		if ((S_ISLNK(statbuf.st_mode)) && (statbuf.st_size > 0)) {
			desktopfile = g_malloc0(statbuf.st_size + 1);
			len = readlink(linkfile, desktopfile, statbuf.st_size + 1);
			if (len != -1) {
				desktopfile[len] = '\0';
				if (g_str_equal(RESOURCE_DESKTOP_FILE, desktopfile)) {
					g_free(desktopfile);
					g_free(linkfile);
					return TRUE;
				} else {
					g_free(desktopfile);
				}
			} else {
				g_free(desktopfile);
			}
		}
	}
	
	g_free(linkfile);
	
	return FALSE;
}

static gboolean mmgui_main_application_add_to_autostart(mmgui_application_t mmguiapp)
{
	gchar *autostartdir, *linkfile, *desktopfile;
	struct stat statbuf;
	gssize len;
	
	if (mmguiapp == NULL) return FALSE;
	
	/*Create autostart directory tree*/
	autostartdir = g_build_path(G_DIR_SEPARATOR_S, g_get_user_config_dir(), "autostart", NULL);
	
	if (autostartdir == NULL) return FALSE;
	
	if (!g_file_test(autostartdir, G_FILE_TEST_IS_DIR)) {
		if (g_mkdir_with_parents(autostartdir, S_IRWXU|S_IRGRP|S_IROTH) == -1) {
			/*Unable to create autostart directory*/
			g_free(autostartdir);
			return FALSE;
		}
	}
	
	g_free(autostartdir);
	
	/*Form autostart link path using XDG standard*/
	linkfile = g_build_filename(g_get_user_config_dir(), "autostart", "modem-manager-gui.desktop", NULL);
	
	if (linkfile == NULL) return FALSE;
	
	if (lstat(linkfile, &statbuf) != -1) {
		if ((S_ISLNK(statbuf.st_mode)) && (statbuf.st_size > 0)) {
			desktopfile = g_malloc0(statbuf.st_size + 1);
			len = readlink(linkfile, desktopfile, statbuf.st_size + 1);
			if (len != -1) {
				desktopfile[len] = '\0';
				if (!g_str_equal(RESOURCE_DESKTOP_FILE, desktopfile)) {
					/*Remove wrong symlink*/
					g_free(desktopfile);
					unlink(linkfile);
				} else {
					/*Symlink already exists*/
					g_free(desktopfile);
					g_free(linkfile);
					return TRUE;
				}
			} else {
				/*Remove unreadable symlink*/
				g_free(desktopfile);
				unlink(linkfile);
			}
		} else {
			/*Remove regular file*/
			unlink(linkfile);
		}
	}
	
	/*Create symlink*/
	if (symlink(RESOURCE_DESKTOP_FILE, linkfile) == 0) {
		g_free(linkfile);
		return TRUE;
	}
	
	g_free(linkfile);
	
	return FALSE;
}

static gboolean mmgui_main_application_remove_from_autostart(mmgui_application_t mmguiapp)
{
	gchar *linkfile;
	struct stat statbuf;
		
	if (mmguiapp == NULL) return FALSE;
	
	/*Form autostart link path using XDG standard*/
	linkfile = g_build_filename(g_get_user_config_dir(), "autostart", "modem-manager-gui.desktop", NULL);
	
	if (linkfile == NULL) return FALSE;
	
	if (lstat(linkfile, &statbuf) != -1) {
		/*Remove symlink*/
		unlink(linkfile);
		g_free(linkfile);
		return TRUE;
	}
		
	g_free(linkfile);
	
	return FALSE;
}
