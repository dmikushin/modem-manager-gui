/*
 *      modem-settings.c
 *      
 *      Copyright 2014-2017 Alex <alex@linuxonly.ru>
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

#include <stdlib.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>

#include "modem-settings.h"


modem_settings_t mmgui_modem_settings_open(const gchar *persistentid)
{
	modem_settings_t settings;
	gchar *filepath;
		
	if (persistentid == NULL) return NULL;
	
	/*Form path using XDG standard*/
	filepath = g_build_path(G_DIR_SEPARATOR_S, g_get_user_data_dir(), "modem-manager-gui", "devices", persistentid, NULL);
	
	if (filepath == NULL) return NULL;
	
	if (g_mkdir_with_parents(filepath, 0755) != 0) {
		g_warning("Cant create modem settings directory");
		g_free(filepath);
		return NULL;
	}
	
	settings = g_new(struct _modem_settings, 1);
	
	settings->filename = g_build_filename(filepath, "modem-settings.conf", NULL);
	
	settings->keyfile = g_key_file_new();
	
	g_free(filepath);
	
	/*Do not show any error messages here*/
	g_key_file_load_from_file(settings->keyfile, settings->filename, G_KEY_FILE_NONE, NULL);
		
	return settings;
}

gboolean mmgui_modem_settings_close(modem_settings_t settings)
{
	gchar *filedata;
	gsize datasize;
	GError *error;
	
	if (settings == NULL) return FALSE;
	if ((settings->filename == NULL) || (settings->keyfile == NULL)) return FALSE;
	
	error = NULL;
	
	filedata = g_key_file_to_data(settings->keyfile, &datasize, &error);
	
	if (filedata != NULL) {
		if (!g_file_set_contents(settings->filename, filedata, datasize, &error)) {
			g_warning("No data saved to file");
			g_error_free(error);
			error = NULL;
		}
	} else {
		g_warning("No data saved to file - empty");
		g_error_free(error);
		error = NULL;
	}
	
	g_free(filedata);
	g_free(settings->filename);
	g_key_file_free(settings->keyfile);
	g_free(settings);
	
	return TRUE;
}

gboolean mmgui_modem_settings_set_string(modem_settings_t settings, gchar *key, gchar *value)
{
	if ((settings == NULL) || (key == NULL) || (value == NULL)) return FALSE;
	
	g_key_file_set_string(settings->keyfile, "settings", key, value);
	
	return TRUE;
}

gchar *mmgui_modem_settings_get_string(modem_settings_t settings, gchar *key, gchar *defvalue)
{
	gchar *value;
	GError *error;
	
	if ((settings == NULL) || (key == NULL)) return g_strdup(defvalue);
	
	error = NULL;
	
	value = g_key_file_get_string(settings->keyfile, "settings", key, &error);
	
	if ((value == NULL) && (error != NULL)) {
		g_error_free(error);
		return g_strdup(defvalue);
	} else {
		return g_strdup(value);
	}
}

gboolean mmgui_modem_settings_set_string_list(modem_settings_t settings, gchar *key, gchar **value)
{
	gsize length;
	gint i;
	
	if ((settings == NULL) || (key == NULL) || (value == NULL)) return FALSE;
	
	i = 0;
	length = 0;
		
	while (value[i] != NULL) {
		length++;
		i++;
	}
	
	if (length == 0) return FALSE;
	
	g_key_file_set_string_list(settings->keyfile, "settings", key, (const gchar *const *)value, length);
	
	return TRUE;
}

gchar **mmgui_modem_settings_get_string_list(modem_settings_t settings, gchar *key, gchar **defvalue)
{
	gchar **value;
	GError *error;
	
	if ((settings == NULL) || (key == NULL)) return defvalue;
	
	error = NULL;
	
	value = g_key_file_get_string_list(settings->keyfile, "settings", key, NULL, &error);
	
	if ((value == NULL) && (error != NULL)) {
		g_error_free(error);
		return defvalue;
	} else {
		return value;
	}
}

gboolean mmgui_modem_settings_set_boolean(modem_settings_t settings, gchar *key, gboolean value)
{
	if ((settings == NULL) || (key == NULL)) return FALSE;
	
	g_key_file_set_boolean(settings->keyfile, "settings", key, value);
	
	return TRUE;
}

gboolean mmgui_modem_settings_get_boolean(modem_settings_t settings, gchar *key, gboolean defvalue)
{
	gboolean value;
	GError *error;
	
	if ((settings == NULL) || (key == NULL)) return defvalue;
	
	error = NULL;
	
	value = g_key_file_get_boolean(settings->keyfile, "settings", key, &error);
	
	if ((error != NULL)) {
		g_error_free(error);
		return defvalue;
	} else {
		return value;
	}
}

gboolean mmgui_modem_settings_set_int(modem_settings_t settings, gchar *key, gint value)
{
	if ((settings == NULL) || (key == NULL)) return FALSE;
	
	g_key_file_set_integer(settings->keyfile, "settings", key, value);
	
	return TRUE;
}

gint mmgui_modem_settings_get_int(modem_settings_t settings, gchar *key, gint defvalue)
{
	gint value;
	GError *error;
	
	if ((settings == NULL) || (key == NULL)) return defvalue;
	
	error = NULL;
	
	value = g_key_file_get_integer(settings->keyfile, "settings", key, &error);
	
	if ((error != NULL)) {
		g_error_free(error);
		return defvalue;
	} else {
		return value;
	}
}

gboolean mmgui_modem_settings_set_int64(modem_settings_t settings, gchar *key, gint64 value)
{
	if ((settings == NULL) || (key == NULL)) return FALSE;
	
	g_key_file_set_int64(settings->keyfile, "settings", key, value);
	
	return TRUE;
}

gint64 mmgui_modem_settings_get_int64(modem_settings_t settings, gchar *key, gint64 defvalue)
{
	gint value;
	GError *error;
	
	if ((settings == NULL) || (key == NULL)) return defvalue;
	
	error = NULL;
	
	value = g_key_file_get_int64(settings->keyfile, "settings", key, &error);
	
	if ((error != NULL)) {
		g_error_free(error);
		return defvalue;
	} else {
		return value;
	}
}

gboolean mmgui_modem_settings_set_double(modem_settings_t settings, gchar *key, gdouble value)
{
	if ((settings == NULL) || (key == NULL)) return FALSE;
	
	g_key_file_set_double(settings->keyfile, "settings", key, value);
	
	return TRUE;
}

gdouble mmgui_modem_settings_get_double(modem_settings_t settings, gchar *key, gdouble defvalue)
{
	gdouble value;
	GError *error;
	
	if ((settings == NULL) || (key == NULL)) return defvalue;
	
	error = NULL;
	
	value = g_key_file_get_boolean(settings->keyfile, "settings", key, &error);
	
	if ((error != NULL)) {
		g_error_free(error);
		return defvalue;
	} else {
		return value;
	}
}
