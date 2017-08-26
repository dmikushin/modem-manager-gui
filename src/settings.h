/*
 *      settings.h
 *      
 *      Copyright 2012 Alex <alex@linuxonly.ru>
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

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

struct _settings {
	gchar *filename;
	GKeyFile *keyfile;
};

typedef struct _settings *settings_t;

settings_t gmm_settings_open(gchar *appname, gchar *filename);
gboolean gmm_settings_close(settings_t settings);
gboolean gmm_settings_set_string(settings_t settings, gchar *key, gchar *value);
gchar *gmm_settings_get_string(settings_t settings, gchar *key, gchar *defvalue);
gboolean gmm_settings_set_boolean(settings_t settings, gchar *key, gboolean value);
gboolean gmm_settings_get_boolean(settings_t settings, gchar *key, gboolean defvalue);
gboolean gmm_settings_set_int(settings_t settings, gchar *key, gint value);
gint gmm_settings_get_int(settings_t settings, gchar *key, gint defvalue);
gboolean gmm_settings_set_double(settings_t settings, gchar *key, gdouble value);
gdouble gmm_settings_get_double(settings_t settings, gchar *key, gdouble defvalue);

#endif /* __SETTINGS_H__ */
