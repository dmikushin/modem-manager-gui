/*
 *      strformat.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "strformat.h"
#include "mmguicore.h"

gchar *mmgui_str_format_speed(gfloat speed, gchar *buffer, gsize bufsize, gboolean small)
{
	gdouble fpvalue;
	
	if ((buffer == NULL) || (bufsize == 0)) return NULL;
	
	memset(buffer, 0, bufsize);
	
	if (speed < 1024.0) {
		if (small) {
			g_snprintf(buffer, bufsize, _("<small><b>%.3f kbps</b></small>"), speed);
		} else {
			g_snprintf(buffer, bufsize, _("%.3f kbps"), speed);
		}
	} else if ((speed >= 1024.0) && (speed < 1048576.0)) {
		fpvalue = speed / (gdouble)(1024.0);
		if (small) {
			g_snprintf(buffer, bufsize, _("<small><b>%.3g Mbps</b></small>"), fpvalue);
		} else {
			g_snprintf(buffer, bufsize, _("%.3g Mbps"), fpvalue);
		}
	} else {
		fpvalue = speed / (gdouble)(1048576.0);
		if (small) {
			g_snprintf(buffer, bufsize, _("<small><b>%.3g Gbps</b></small>"), fpvalue);
		} else {
			g_snprintf(buffer, bufsize, _("%.3g Gbps"), fpvalue);
		}
	}
	
	return buffer;	
}

gchar *mmgui_str_format_time_number(guchar number, gchar *buffer, gsize bufsize)
{
	if ((buffer == NULL) || (bufsize == 0)) return NULL;
	
	memset(buffer, 0, bufsize);
	
	if (number < 10) {
		g_snprintf(buffer, bufsize, "0%u", (guint)number);
	} else {
		g_snprintf(buffer, bufsize, "%u", (guint)number);
	}
	
	return buffer;
}

gchar *mmgui_str_format_time(guint64 seconds, gchar *buffer, gsize bufsize, gboolean small)
{
	gchar secbuffer[3], minbuffer[3], hourbuffer[3];
	
	if ((buffer == NULL) || (bufsize == 0)) return NULL;
	
	memset(buffer, 0, bufsize);
	
	if (seconds < 60) {
		if (small) {
			g_snprintf(buffer, bufsize, _("<small><b>%u sec</b></small>"), (guint)seconds);
		} else {
			g_snprintf(buffer, bufsize, _("%u sec"), (guint)seconds);
		}
	} else if ((seconds >= 60) && (seconds < 3600)) {
		if (small) {
			g_snprintf(buffer, bufsize, _("<small><b>%s:%s</b></small>"), mmgui_str_format_time_number(seconds%3600/60, minbuffer, sizeof(minbuffer)), mmgui_str_format_time_number(seconds%60, secbuffer, sizeof(secbuffer)));
		} else {
			g_snprintf(buffer, bufsize, _("%s:%s"), mmgui_str_format_time_number(seconds%3600/60, minbuffer, sizeof(minbuffer)), mmgui_str_format_time_number(seconds%60, secbuffer, sizeof(secbuffer)));
		}
	} else if ((seconds >= 3600) && (seconds < 86400)) {
		if (small) {
			g_snprintf(buffer, bufsize, _("<small><b>%s:%s:%s</b></small>"), mmgui_str_format_time_number(seconds%86400/3600, hourbuffer, sizeof(hourbuffer)), mmgui_str_format_time_number(seconds%3600/60, minbuffer, sizeof(minbuffer)), mmgui_str_format_time_number(seconds%60, secbuffer, sizeof(secbuffer)));
		} else {
			g_snprintf(buffer, bufsize, _("%s:%s:%s"), mmgui_str_format_time_number(seconds%86400/3600, hourbuffer, sizeof(hourbuffer)), mmgui_str_format_time_number(seconds%3600/60, minbuffer, sizeof(minbuffer)), mmgui_str_format_time_number(seconds%60, secbuffer, sizeof(secbuffer)));
		}
	} else {
		if (small) {
			g_snprintf(buffer, bufsize, _("<small><b>%" G_GUINT64_FORMAT " day(s) %s:%s:%s</b></small>"), seconds/86400, mmgui_str_format_time_number(seconds%86400/3600, hourbuffer, sizeof(hourbuffer)), mmgui_str_format_time_number(seconds%3600/60, minbuffer, sizeof(minbuffer)), mmgui_str_format_time_number(seconds%60, secbuffer, sizeof(secbuffer)));
		} else {
			g_snprintf(buffer, bufsize, _("%" G_GUINT64_FORMAT " day(s) %s:%s:%s"), seconds/86400, mmgui_str_format_time_number(seconds%86400/3600, hourbuffer, sizeof(hourbuffer)), mmgui_str_format_time_number(seconds%3600/60, minbuffer, sizeof(minbuffer)), mmgui_str_format_time_number(seconds%60, secbuffer, sizeof(secbuffer)));
		}
	}
	
	return buffer;
}

gchar *mmgui_str_format_bytes(guint64 bytes, gchar *buffer, gsize bufsize, gboolean small)
{
	gdouble fpvalue;
	
	if ((buffer == NULL) || (bufsize == 0)) return NULL;
	
	memset(buffer, 0, bufsize);
	
	if (bytes < 1024) {
		if (small) {
			g_snprintf(buffer, bufsize, _("<small><b>%u</b></small>"), (guint)bytes);
		} else {
			g_snprintf(buffer, bufsize, _("%u"), (guint)bytes);
		}
	} else if ((bytes >= 1024) && (bytes < 1048576ull)) {
		fpvalue = bytes / (gdouble)(1024.0);
		if (small) {
			g_snprintf(buffer, bufsize, _("<small><b>%.3g Kb</b></small>"), fpvalue);
		} else {
			g_snprintf(buffer, bufsize, _("%.3g Kb"), fpvalue);
		}
	} else if ((bytes >= 1048576ull) && (bytes < 1073741824ull)) {
		fpvalue = bytes / (gdouble)(1048576.0);
		if (small) {
			g_snprintf(buffer, bufsize, _("<small><b>%.3g Mb</b></small>"), fpvalue);
		} else {
			g_snprintf(buffer, bufsize, _("%.3g Mb"), fpvalue);
		}
	} else if ((bytes >= 1073741824ull) && (bytes < 109951162800ull)) {
		fpvalue = bytes / (gdouble)(1073741824.0);
		if (small) {
			g_snprintf(buffer, bufsize, _("<small><b>%.3g Gb</b></small>"), fpvalue);
		} else {
			g_snprintf(buffer, bufsize, _("%.3g Gb"), fpvalue);
		}
	} else {
		fpvalue = bytes / (gdouble)(109951162800.0);
		if (small) {
			g_snprintf(buffer, bufsize, _("<small><b>%.3g Tb</b></small>"), fpvalue);
		} else {
			g_snprintf(buffer, bufsize, _("%.3g Tb"), fpvalue);
		}
	}
	
	return buffer;	
}


gchar *mmgui_str_format_sms_time(time_t timestamp, gchar *buffer, gsize bufsize)
{
	time_t todaytime;
	struct tm *ftime;
	gdouble delta;
	
	
	if ((buffer == NULL) || (bufsize == 0)) return NULL;
	
	/*Truncate today's time*/
	todaytime = time(NULL);
	ftime = localtime(&todaytime);
	ftime->tm_hour = 0;
	ftime->tm_min = 0;
	ftime->tm_sec = 0;
	todaytime = mktime(ftime);
	/*Calculate time interval*/
	delta = difftime(todaytime, timestamp);
	/*Prepare mssage time structure*/
	ftime = localtime(&timestamp);
		
	memset(buffer, 0, bufsize);
	
	if (delta <= 0.0) {
		if (strftime(buffer, bufsize, _("Today, %T"), ftime) == 0) {
			g_snprintf(buffer, bufsize, _("Unknown"));
		}
	} else if ((delta > 0.0) && (delta < 86400.0)) {
		if (strftime(buffer, bufsize, _("Yesterday, %T"), ftime) == 0) {
			g_snprintf(buffer, bufsize, _("Unknown"));
		}
	} else {
		if (strftime(buffer, bufsize, _("%d %B %Y, %T"), ftime) == 0) {
			g_snprintf(buffer, bufsize, _("Unknown"));
		}
	}
	
	return buffer;
}

gchar *mmgui_str_format_mode_string(enum _mmgui_device_modes mode)
{
	switch (mode) {
		case MMGUI_DEVICE_MODE_UNKNOWN:
			return _("Unknown");
		case MMGUI_DEVICE_MODE_GSM:
			return "GSM";
		case MMGUI_DEVICE_MODE_GSM_COMPACT:
			return "Compact GSM";
		case MMGUI_DEVICE_MODE_GPRS:
			return "GPRS";
		case MMGUI_DEVICE_MODE_EDGE:
			return "EDGE (ETSI 27.007: \"GSM w/EGPRS\")";
		case MMGUI_DEVICE_MODE_UMTS:
			return "UMTS (ETSI 27.007: \"UTRAN\")";
		case MMGUI_DEVICE_MODE_HSDPA:
			return "HSDPA (ETSI 27.007: \"UTRAN w/HSDPA\")";
		case MMGUI_DEVICE_MODE_HSUPA:
			return "HSUPA (ETSI 27.007: \"UTRAN w/HSUPA\")";
		case MMGUI_DEVICE_MODE_HSPA:
			return "HSPA (ETSI 27.007: \"UTRAN w/HSDPA and HSUPA\")";
		case MMGUI_DEVICE_MODE_HSPA_PLUS:
			return "HSPA+ (ETSI 27.007: \"UTRAN w/HSPA+\")";
		case MMGUI_DEVICE_MODE_1XRTT:
			return "CDMA2000 1xRTT";
		case MMGUI_DEVICE_MODE_EVDO0:
			return "CDMA2000 EVDO revision 0";
		case MMGUI_DEVICE_MODE_EVDOA:
			return "CDMA2000 EVDO revision A";
		case MMGUI_DEVICE_MODE_EVDOB:
			return "CDMA2000 EVDO revision B";
		case MMGUI_DEVICE_MODE_LTE:
			return "LTE (ETSI 27.007: \"E-UTRAN\")";
		default:
			return _("Unknown");
	}
}

gchar *mmgui_str_format_na_status_string(enum _mmgui_network_availability status)
{
	switch (status) {
		case MMGUI_NA_UNKNOWN:
			return _("Unknown");
		case MMGUI_NA_AVAILABLE:
			return _("Available");
		case MMGUI_NA_CURRENT:
			return _("Current");
		case MMGUI_NA_FORBIDDEN:
			return _("Forbidden");
		default:
			return _("Unknown");
	}
}

gchar *mmgui_str_format_access_tech_string(enum _mmgui_access_tech status)
{
	switch (status) {
		case MMGUI_ACCESS_TECH_GSM:
			return "GSM";
		case MMGUI_ACCESS_TECH_GSM_COMPACT:
			return "GSM Compact";
		case MMGUI_ACCESS_TECH_UMTS:
			return "UMTS";
		case MMGUI_ACCESS_TECH_EDGE:
			return "EDGE";
		case MMGUI_ACCESS_TECH_HSDPA:
			return "HSDPA";
		case MMGUI_ACCESS_TECH_HSUPA:
			return "HSUPA";
		case MMGUI_ACCESS_TECH_HSPA:
			return "HSPA";
		case MMGUI_ACCESS_TECH_HSPA_PLUS:
			return "HSPA+";
		case MMGUI_ACCESS_TECH_LTE:
			return "LTE";
		default:
			return "Unknown";
	}
}

gchar *mmgui_str_format_reg_status(enum _mmgui_reg_status status)
{
	switch (status) {
		case MMGUI_REG_STATUS_IDLE:
			return _("Not registered");
		case MMGUI_REG_STATUS_HOME:
			return _("Home network");
		case MMGUI_REG_STATUS_SEARCHING:
			return _("Searching");
		case MMGUI_REG_STATUS_DENIED:
			return _("Registration denied");
		case MMGUI_REG_STATUS_UNKNOWN:
			return _("Unknown status");
		case MMGUI_REG_STATUS_ROAMING:
			return _("Roaming network");
		default:
			return _("Unknown status");
	}
}

gchar *mmgui_str_format_operator_code(gint operatorcode, enum _mmgui_device_types type, gchar *buffer, gsize bufsize)
{
	
	if ((buffer == NULL) || (bufsize == 0)) return NULL;
	
	memset(buffer, 0, bufsize);
	
	if (operatorcode != 0) {
		if (type == MMGUI_DEVICE_TYPE_GSM) {
			if ((operatorcode & 0x0000ffff) < 10) {
				/*MCC+MNC (4 to 5 digits)*/
				g_snprintf(buffer, bufsize, "%u0%u", (operatorcode & 0xffff0000) >> 16, operatorcode & 0x0000ffff);
			} else {
				/*MCC+MNC (5 and 6 digits)*/
				g_snprintf(buffer, bufsize, "%u%u", (operatorcode & 0xffff0000) >> 16, operatorcode & 0x0000ffff);
			}
		} else if (type == MMGUI_DEVICE_TYPE_CDMA) {
			/*SID*/
			g_snprintf(buffer, bufsize, "%u", operatorcode);
		}
	} else {
		g_snprintf(buffer, bufsize, _("Unknown"));
	}
	
	return buffer;
}

gchar *mmgui_str_format_message_validity_period(gdouble value)
{
	gfloat num;
	gchar *res;
	
	if ((value >= 0.0) && (value <= 143.0)) {
		/*Minutes*/
		num = (value + 1.0) * 5.0;
		res = g_strdup_printf(_("%3.0f minutes"), num);
	} else if ((value >= 144.0) && (value <= 167.0)) {
		/*Hours*/
		num = 12.0 + ((value - 143.0) * 0.5);
		res = g_strdup_printf(_("%3.1f hours"), num);
	} else if ((value >= 168.0) && (value <= 196.0)) {
		/*Days*/
		num = (value - 166.0);
		res = g_strdup_printf(_("%2.0f days"), num);
	} else if ((value >= 197.0) && (value <= 255.0)) {
		/*Weeks*/
		num = (value - 192.0);
		res = g_strdup_printf(_("%2.0f weeks"), num);
	} else {
		/*Undefined*/
		res = g_strdup(_("Undefined"));
	}
	
	return res;
}

gchar *mmgui_str_format_operation_timeout_period(gdouble value)
{
	gchar *res;
	
	if ((value >= 0.0) && (value < 60.0)) {
		/*Seconds*/
		res = g_strdup_printf(_("%2.0f sec"), value);
	} else if (value >= 60.0) {
		/*Minutes*/
		res = g_strdup_printf(_("%u min, %u sec"), (guint)value / 60, (guint)value % 60);
	} else {
		/*Undefined*/
		res = g_strdup("Undefined");
	}
	
	return res;	
}
