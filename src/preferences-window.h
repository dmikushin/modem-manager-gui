/*
 *      preferences-window.h
 *      
 *      Copyright 2015-2017 Alex <alex@linuxonly.ru>
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

#ifndef __PREFERENCES_WINDOW_H__
#define __PREFERENCES_WINDOW_H__

#include <glib.h>

void mmgui_preferences_window_services_page_mm_modules_combo_changed(GtkComboBox *widget, gpointer data);
void mmgui_preferences_window_services_page_cm_modules_combo_changed(GtkComboBox *widget, gpointer data);
void mmgui_preferences_window_activate_signal(GSimpleAction *action, GVariant *parameter, gpointer data);
gchar *mmgui_preferences_window_message_validity_scale_value_format(GtkScale *scale, gdouble value, gpointer data);
gchar *mmgui_preferences_window_timeout_scale_value_format(GtkScale *scale, gdouble value, gpointer data);

#endif /* __PREFERENCES_WINDOW_H__ */
