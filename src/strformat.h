/*
 *      strformat.h
 *      
 *      Copyright 2013-2014 Alex <alex@linuxonly.ru>
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

#ifndef __STRFORMAT_H__
#define __STRFORMAT_H__

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "mmguicore.h"

gchar *mmgui_str_format_speed(gfloat speed, gchar *buffer, gsize bufsize, gboolean small);
gchar *mmgui_str_format_time_number(guchar number, gchar *buffer, gsize bufsize);
gchar *mmgui_str_format_time(guint64 seconds, gchar *buffer, gsize bufsize, gboolean small);
gchar *mmgui_str_format_bytes(guint64 bytes, gchar *buffer, gsize bufsize, gboolean small);
gchar *mmgui_str_format_sms_time(time_t timestamp, gchar *buffer, gsize bufsize);
gchar *mmgui_str_format_mode_string(enum _mmgui_device_modes mode);
gchar *mmgui_str_format_na_status_string(enum _mmgui_network_availability status);
gchar *mmgui_str_format_access_tech_string(enum _mmgui_access_tech status);
gchar *mmgui_str_format_reg_status(enum _mmgui_reg_status status);
gchar *mmgui_str_format_operator_code(gint operatorcode, enum _mmgui_device_types type, gchar *buffer, gsize bufsize);
gchar *mmgui_str_format_message_validity_period(gdouble value);
gchar *mmgui_str_format_operation_timeout_period(gdouble value);

#endif /* __STRFORMAT_H__ */
