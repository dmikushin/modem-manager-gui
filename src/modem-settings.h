/*
 *      modem-settings.h
 *      
 *      Copyright 2014 Alex <alex@linuxonly.ru>
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

#ifndef __MODEM_SETTINGS_H__
#define __MODEM_SETTINGS_H__

struct _modem_settings {
	gchar *filename;
	GKeyFile *keyfile;
};

typedef struct _modem_settings *modem_settings_t;

modem_settings_t mmgui_modem_settings_open(const gchar *persistentid);
gboolean mmgui_modem_settings_close(modem_settings_t settings);
gboolean mmgui_modem_settings_set_string(modem_settings_t settings, gchar *key, gchar *value);
gchar *mmgui_modem_settings_get_string(modem_settings_t settings, gchar *key, gchar *defvalue);
gboolean mmgui_modem_settings_set_string_list(modem_settings_t settings, gchar *key, gchar **value);
gchar **mmgui_modem_settings_get_string_list(modem_settings_t settings, gchar *key, gchar **defvalue);
gboolean mmgui_modem_settings_set_boolean(modem_settings_t settings, gchar *key, gboolean value);
gboolean mmgui_modem_settings_get_boolean(modem_settings_t settings, gchar *key, gboolean defvalue);
gboolean mmgui_modem_settings_set_int(modem_settings_t settings, gchar *key, gint value);
gint mmgui_modem_settings_get_int(modem_settings_t settings, gchar *key, gint defvalue);
gboolean mmgui_modem_settings_set_int64(modem_settings_t settings, gchar *key, gint64 value);
gint64 mmgui_modem_settings_get_int64(modem_settings_t settings, gchar *key, gint64 defvalue);
gboolean mmgui_modem_settings_set_double(modem_settings_t settings, gchar *key, gdouble value);
gdouble mmgui_modem_settings_get_double(modem_settings_t settings, gchar *key, gdouble defvalue);

#endif /* __MODEM_SETTINGS_H__ */
