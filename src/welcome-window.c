/*
 *      welcome-window.c
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
 
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "svcmanager.h"
#include "mmguicore.h"
#include "welcome-window.h"

enum _mmgui_welcome_window_pages {
	MMGUI_WELCOME_WINDOW_SERVICES_PAGE = 0,
	MMGUI_WELCOME_WINDOW_ACTIVATION_PAGE
};

enum _mmgui_welcome_window_activation_list {
	MMGUI_WELCOME_WINDOW_ACTIVATION_SERVICE = 0,
	MMGUI_WELCOME_WINDOW_ACTIVATION_STATE,
	MMGUI_WELCOME_WINDOW_ACTIVATION_COLUMNS
};

enum _mmgui_welcome_window_service_list {
	MMGUI_WELCOME_WINDOW_SERVICE_LIST_ICON = 0,
	MMGUI_WELCOME_WINDOW_SERVICE_LIST_NAME,
	MMGUI_WELCOME_WINDOW_SERVICE_LIST_MODULE,
	MMGUI_WELCOME_WINDOW_SERVICE_LIST_SERVICENAME,
	MMGUI_WELCOME_WINDOW_SERVICE_LIST_COMPATIBILITY,
	MMGUI_WELCOME_WINDOW_SERVICE_LIST_AVAILABLE,
	MMGUI_WELCOME_WINDOW_SERVICE_LIST_COMPATIBLE,
	MMGUI_WELCOME_WINDOW_SERVICE_LIST_COLUMNS
};


static void mmgui_welcome_window_terminate_application(mmgui_application_t mmguiapp);
static void mmgui_welcome_window_services_page_update_compatible_modules(mmgui_application_t mmguiapp, GtkComboBox *currentcombo, GtkComboBox *othercombo);
static void mmgui_welcome_window_services_page_modules_combo_set_sensitive(GtkCellLayout *cell_layout, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data);
static void mmgui_welcome_window_services_page_modules_combo_fill(GtkComboBox *combo, GSList *modules, gint type, mmguimodule_t currentmodule);

static void mmgui_welcome_window_terminate_application(mmgui_application_t mmguiapp)
{
	if (mmguiapp == NULL) return;
	
	/*Terminate application*/
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

static void mmgui_welcome_window_services_page_update_compatible_modules(mmgui_application_t mmguiapp, GtkComboBox *currentcombo, GtkComboBox *othercombo)
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
			gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, MMGUI_WELCOME_WINDOW_SERVICE_LIST_COMPATIBILITY, &compatibility, -1);
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
							gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, MMGUI_WELCOME_WINDOW_SERVICE_LIST_AVAILABLE, &available,
																			MMGUI_WELCOME_WINDOW_SERVICE_LIST_SERVICENAME, &servicename,
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
								gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_WELCOME_WINDOW_SERVICE_LIST_COMPATIBLE, compatible, -1);
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
								gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_WELCOME_WINDOW_SERVICE_LIST_COMPATIBLE, TRUE, -1);
							}
						} while (gtk_tree_model_iter_next(model, &iter));
					}
					/*Try to select first compatible module if needed*/
					if (compreference != NULL) {
						compatible = FALSE;
						servicename = NULL;
						/*If other selected module is compatible?*/
						if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(othercombo), &iter)) {
							gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, MMGUI_WELCOME_WINDOW_SERVICE_LIST_COMPATIBLE, &compatible,
																			MMGUI_WELCOME_WINDOW_SERVICE_LIST_SERVICENAME, &servicename,
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

void mmgui_welcome_window_services_page_mm_modules_combo_changed(GtkComboBox *widget, gpointer data)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	g_signal_handlers_block_by_func(mmguiapp->window->welcomecmcombo, mmgui_welcome_window_services_page_cm_modules_combo_changed, mmguiapp);
	mmgui_welcome_window_services_page_update_compatible_modules(mmguiapp, widget, GTK_COMBO_BOX(mmguiapp->window->welcomecmcombo));
	g_signal_handlers_unblock_by_func(mmguiapp->window->welcomecmcombo, mmgui_welcome_window_services_page_cm_modules_combo_changed, mmguiapp);
}

void mmgui_welcome_window_services_page_cm_modules_combo_changed(GtkComboBox *widget, gpointer data)
{
	mmgui_application_t mmguiapp;
			
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
	
	g_signal_handlers_block_by_func(mmguiapp->window->welcomemmcombo, mmgui_welcome_window_services_page_mm_modules_combo_changed, mmguiapp);
	mmgui_welcome_window_services_page_update_compatible_modules(mmguiapp, widget, GTK_COMBO_BOX(mmguiapp->window->welcomemmcombo));
	g_signal_handlers_unblock_by_func(mmguiapp->window->welcomemmcombo, mmgui_welcome_window_services_page_mm_modules_combo_changed, mmguiapp);
}

gboolean mmgui_welcome_window_delete_event_signal(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	mmgui_application_t mmguiapp;
				
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return FALSE;
	
	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiapp->window->welcomenotebook)) == MMGUI_WELCOME_WINDOW_SERVICES_PAGE) {
		/*Close window and terminate application*/
		mmgui_welcome_window_terminate_application(mmguiapp);
	} else if (gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiapp->window->welcomenotebook)) == MMGUI_WELCOME_WINDOW_ACTIVATION_PAGE) {
		/*Close window and terminate application in case of error*/
		if (gtk_widget_get_sensitive(mmguiapp->window->welcomebutton)) {
			mmgui_welcome_window_terminate_application(mmguiapp);
		}
	} else {
		g_debug("Unknown welcome window page: %u\n", gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiapp->window->welcomenotebook)));		
	}
	
	return FALSE;
}

void mmgui_welcome_window_button_clicked_signal(GtkButton *button, gpointer data)
{
	mmgui_application_t mmguiapp;
	GtkTreeModel *model;
	GtkTreeIter iter;
				
	mmguiapp = (mmgui_application_t)data;
	
	if (mmguiapp == NULL) return;
		
	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiapp->window->welcomenotebook)) == MMGUI_WELCOME_WINDOW_SERVICES_PAGE) {
		/*Disable startup button*/
		gtk_widget_set_sensitive(mmguiapp->window->welcomebutton, FALSE);
		/*Save selected modules*/
		/*Modem manager*/
		model = gtk_combo_box_get_model(GTK_COMBO_BOX(mmguiapp->window->welcomemmcombo));
		if (model != NULL) {
			if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(mmguiapp->window->welcomemmcombo), &iter)) {
				gtk_tree_model_get(model, &iter, MMGUI_WELCOME_WINDOW_SERVICE_LIST_MODULE, &mmguiapp->coreoptions->mmmodule, -1);
				gmm_settings_set_string(mmguiapp->settings, "modules_preferred_modem_manager", mmguiapp->coreoptions->mmmodule);
			}
		}
		/*Connection manager*/
		model = gtk_combo_box_get_model(GTK_COMBO_BOX(mmguiapp->window->welcomecmcombo));
		if (model != NULL) {
			if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(mmguiapp->window->welcomecmcombo), &iter)) {
				gtk_tree_model_get(model, &iter, MMGUI_WELCOME_WINDOW_SERVICE_LIST_MODULE, &mmguiapp->coreoptions->cmmodule, -1);
				gmm_settings_set_string(mmguiapp->settings, "modules_preferred_connection_manager", mmguiapp->coreoptions->cmmodule);
			}
		}
		/*Services enablement flag*/
		mmguiapp->coreoptions->enableservices = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmguiapp->window->welcomeenablecb));
		gmm_settings_set_boolean(mmguiapp->settings, "modules_enable_services", mmguiapp->coreoptions->enableservices);
		/*Start execution*/
		mmguicore_start(mmguiapp->core);
	} else if (gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiapp->window->welcomenotebook)) == MMGUI_WELCOME_WINDOW_ACTIVATION_PAGE) {
		/*Close window and terminate application*/
		mmgui_welcome_window_terminate_application(mmguiapp);
	} else {
		g_debug("Unknown welcome window page: %u\n", gtk_notebook_get_current_page(GTK_NOTEBOOK(mmguiapp->window->welcomenotebook)));		
	}
}

static void mmgui_welcome_window_services_page_modules_combo_set_sensitive(GtkCellLayout *cell_layout, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data)
{
	gboolean available, compatible;
	
	gtk_tree_model_get(tree_model, iter, MMGUI_WELCOME_WINDOW_SERVICE_LIST_AVAILABLE, &available,
										MMGUI_WELCOME_WINDOW_SERVICE_LIST_COMPATIBLE, &compatible,
										-1);
	
	g_object_set(cell, "sensitive", available && compatible, NULL);
}

static void mmgui_welcome_window_services_page_modules_combo_fill(GtkComboBox *combo, GSList *modules, gint type, mmguimodule_t currentmodule)
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
		
	store = gtk_list_store_new(MMGUI_WELCOME_WINDOW_SERVICE_LIST_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	
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
								MMGUI_WELCOME_WINDOW_SERVICE_LIST_ICON, icon,
								MMGUI_WELCOME_WINDOW_SERVICE_LIST_NAME, module->description,
								MMGUI_WELCOME_WINDOW_SERVICE_LIST_MODULE, module->shortname,
								MMGUI_WELCOME_WINDOW_SERVICE_LIST_SERVICENAME, module->servicename,
								MMGUI_WELCOME_WINDOW_SERVICE_LIST_COMPATIBILITY, module->compatibility,
								MMGUI_WELCOME_WINDOW_SERVICE_LIST_AVAILABLE, module->applicable || (module->activationtech != MMGUI_SVCMANGER_ACTIVATION_TECH_NA),
								MMGUI_WELCOME_WINDOW_SERVICE_LIST_COMPATIBLE, TRUE,
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
						MMGUI_WELCOME_WINDOW_SERVICE_LIST_ICON, "user-invisible",
						MMGUI_WELCOME_WINDOW_SERVICE_LIST_NAME, _("Undefined"),
						MMGUI_WELCOME_WINDOW_SERVICE_LIST_MODULE, "undefined",
						MMGUI_WELCOME_WINDOW_SERVICE_LIST_SERVICENAME, NULL,
						MMGUI_WELCOME_WINDOW_SERVICE_LIST_COMPATIBILITY, NULL,
						MMGUI_WELCOME_WINDOW_SERVICE_LIST_AVAILABLE, TRUE,
						MMGUI_WELCOME_WINDOW_SERVICE_LIST_COMPATIBLE, TRUE,
						-1);
	
	if (moduleid == -1) {
		moduleid = curid;
	}
	
	area = gtk_cell_layout_get_area(GTK_CELL_LAYOUT(combo));
	
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, TRUE);
	gtk_cell_area_attribute_connect(area, renderer, "icon-name",  MMGUI_WELCOME_WINDOW_SERVICE_LIST_ICON);
	
	gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(combo), renderer, mmgui_welcome_window_services_page_modules_combo_set_sensitive, NULL, NULL);
	
	renderer = gtk_cell_renderer_text_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, TRUE, FALSE, FALSE);
	gtk_cell_area_attribute_connect(area, renderer, "text", MMGUI_WELCOME_WINDOW_SERVICE_LIST_NAME);
	
	gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(combo), renderer, mmgui_welcome_window_services_page_modules_combo_set_sensitive, NULL, NULL);
	
	gtk_combo_box_set_model(GTK_COMBO_BOX(combo), GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), moduleid);
}

void mmgui_welcome_window_services_page_open(mmgui_application_t mmguiapp)
{
	GSList *modules;
	
	if (mmguiapp == NULL) return;
	if (mmguiapp->core == NULL) return;
	
	modules = mmguicore_modules_get_list(mmguiapp->core);
	
	if (modules == NULL) return;
	
	/*Modem manager*/
	mmgui_welcome_window_services_page_modules_combo_fill(GTK_COMBO_BOX(mmguiapp->window->welcomemmcombo), modules, MMGUI_MODULE_TYPE_MODEM_MANAGER, NULL);
	
	/*Connection manager*/
	mmgui_welcome_window_services_page_modules_combo_fill(GTK_COMBO_BOX(mmguiapp->window->welcomecmcombo), modules, MMGUI_MODULE_TYPE_CONNECTION_MANGER, NULL);
	
	/*Go to activation page*/
	gtk_notebook_set_current_page(GTK_NOTEBOOK(mmguiapp->window->welcomenotebook), MMGUI_WELCOME_WINDOW_SERVICES_PAGE);
	
	/*Prepare startup button*/
	gtk_button_set_label(GTK_BUTTON(mmguiapp->window->welcomebutton), _("_Let's Start!"));
	gtk_widget_set_sensitive(mmguiapp->window->welcomebutton, TRUE);
	
	/*Show window*/
	if (!gtk_widget_get_visible(mmguiapp->window->welcomewindow)) {
		gtk_widget_show_all(mmguiapp->window->welcomewindow);
	}
}

void mmgui_welcome_window_activation_page_open(mmgui_application_t mmguiapp)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *store;
		
	if (mmguiapp == NULL) return;
	
	/*Initialize list store*/
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Service"), renderer, "text", MMGUI_WELCOME_WINDOW_ACTIVATION_SERVICE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->welcomeacttreeview), column);
		
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("State"), renderer, "markup", MMGUI_WELCOME_WINDOW_ACTIVATION_STATE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(mmguiapp->window->welcomeacttreeview), column);
	
	store = gtk_list_store_new(MMGUI_WELCOME_WINDOW_ACTIVATION_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(mmguiapp->window->welcomeacttreeview), GTK_TREE_MODEL(store));
	g_object_unref(store);
	
	/*Go to activation page*/
	gtk_notebook_set_current_page(GTK_NOTEBOOK(mmguiapp->window->welcomenotebook), MMGUI_WELCOME_WINDOW_ACTIVATION_PAGE);
	
	/*Prepare startup button*/
	gtk_button_set_label(GTK_BUTTON(mmguiapp->window->welcomebutton), _("_Close"));
	gtk_widget_set_sensitive(mmguiapp->window->welcomebutton, FALSE);
	
	/*Show window*/
	if (!gtk_widget_get_visible(mmguiapp->window->welcomewindow)) {
		gtk_widget_show_all(mmguiapp->window->welcomewindow);
	}
}

void mmgui_welcome_window_close(mmgui_application_t mmguiapp)
{
	if (mmguiapp == NULL) return;
	
	/*Hide window*/
	if (gtk_widget_get_visible(mmguiapp->window->welcomewindow)) {
		gtk_widget_hide(mmguiapp->window->welcomewindow);
	}
}

void mmgui_welcome_window_activation_page_add_service(mmgui_application_t mmguiapp, gchar *service)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	if ((mmguiapp == NULL) || (service == NULL)) return;
	
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->welcomeacttreeview));
	if (model != NULL) {
		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_WELCOME_WINDOW_ACTIVATION_SERVICE, service,
														MMGUI_WELCOME_WINDOW_ACTIVATION_STATE, _("Activation..."),
														-1);
	}
}

void mmgui_welcome_window_activation_page_set_state(mmgui_application_t mmguiapp, gchar *error)
{
	GtkTreeModel *model;
	gint rows;
	GtkTreeIter iter;
	gchar *errmarkup;
	
	if (mmguiapp == NULL) return;
		
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mmguiapp->window->welcomeacttreeview));
	if (model != NULL) {
		rows = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(model), NULL);
		if (gtk_tree_model_iter_nth_child(model, &iter, NULL, rows - 1)) {
			if (error == NULL) {
				/*Successfull activation*/
				gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_WELCOME_WINDOW_ACTIVATION_STATE, _("<span foreground=\"green\">Success</span>"), -1);
			} else {
				/*Error while activation*/
				errmarkup = g_strdup_printf(_("<span foreground=\"red\">%s</span>"), error);
				gtk_list_store_set(GTK_LIST_STORE(model), &iter, MMGUI_WELCOME_WINDOW_ACTIVATION_STATE, errmarkup, -1);
				g_free(errmarkup);
				/*Enable close button*/
				gtk_widget_set_sensitive(mmguiapp->window->welcomebutton, TRUE);
			}
		}
	}
}
