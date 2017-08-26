/*
 *      settings.c
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

#include <stdlib.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>

#include "settings.h"


settings_t gmm_settings_open(gchar *appname, gchar *filename)
{
	settings_t settings;
	gchar *confpath;
	gchar *filedata;
	gsize datasize;
	GError *error;
	
	if ((appname == NULL) || (filename == NULL)) return NULL;
	
	//Form path using XDG standard
	confpath = g_build_path(G_DIR_SEPARATOR_S, g_get_user_config_dir(), appname, NULL);
	
	if (g_mkdir_with_parents(confpath, 0755) != 0) {
		g_warning("Cant create program settings directory");
		g_free(confpath);
		return NULL;
	}
	
	g_free(confpath);
	
	settings = g_new(struct _settings, 1);
	
	settings->filename = g_build_filename(g_get_user_config_dir(), appname, filename, NULL);
	
	settings->keyfile = g_key_file_new();
	
	error = NULL;
	
	if (g_file_get_contents(settings->filename, &filedata, &datasize, &error)) {
		if (!g_key_file_load_from_data(settings->keyfile, filedata, datasize, G_KEY_FILE_NONE, &error)) {
			g_warning("No data loaded from file");
			g_error_free(error);
			error = NULL;
		}
	} else {
		g_warning("No data loaded from file");
		g_error_free(error);
		error = NULL;
	}
	
	return settings;
}

gboolean gmm_settings_close(settings_t settings)
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
		g_warning("No data saved to file");
		g_error_free(error);
		error = NULL;
	}
	
	g_free(filedata);
	g_free(settings->filename);
	g_key_file_free(settings->keyfile);
	g_free(settings);
	
	return TRUE;
}

gboolean gmm_settings_set_string(settings_t settings, gchar *key, gchar *value)
{
	if ((settings == NULL) || (key == NULL) || (value == NULL)) return FALSE;
	
	g_key_file_set_string(settings->keyfile, "settings", key, value);
	
	return TRUE;
}

gchar *gmm_settings_get_string(settings_t settings, gchar *key, gchar *defvalue)
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

gboolean gmm_settings_set_boolean(settings_t settings, gchar *key, gboolean value)
{
	if ((settings == NULL) || (key == NULL)) return FALSE;
	
	g_key_file_set_boolean(settings->keyfile, "settings", key, value);
	
	return TRUE;
}

gboolean gmm_settings_get_boolean(settings_t settings, gchar *key, gboolean defvalue)
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

gboolean gmm_settings_set_int(settings_t settings, gchar *key, gint value)
{
	if ((settings == NULL) || (key == NULL)) return FALSE;
	
	g_key_file_set_integer(settings->keyfile, "settings", key, value);
	
	return TRUE;
}

gint gmm_settings_get_int(settings_t settings, gchar *key, gint defvalue)
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

gboolean gmm_settings_set_double(settings_t settings, gchar *key, gdouble value)
{
	if ((settings == NULL) || (key == NULL)) return FALSE;
	
	g_key_file_set_double(settings->keyfile, "settings", key, value);
	
	return TRUE;
}

gdouble gmm_settings_get_double(settings_t settings, gchar *key, gdouble defvalue)
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
