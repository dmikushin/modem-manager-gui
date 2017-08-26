/*
 *      trafficdb.c
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

#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <string.h>
#include <gdbm.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "trafficdb.h"

#define TRAFFICDB_DAY_XML "<traffic>\n\t<daytime>%" G_GUINT64_FORMAT "</daytime>\n\t<dayrxbytes>%" G_GUINT64_FORMAT "</dayrxbytes>\n\t<daytxbytes>%" G_GUINT64_FORMAT "</daytxbytes>\n\t<dayduration>%u</dayduration>\n\t<sesstime>%" G_GUINT64_FORMAT "</sesstime>\n\t<sessrxbytes>%" G_GUINT64_FORMAT "</sessrxbytes>\n\t<sesstxbytes>%" G_GUINT64_FORMAT "</sesstxbytes>\n\t<sessduration>%u</sessduration>\n</traffic>\n\n"

enum _mmgui_trafficdb_xml_elements {
	TRAFFICDB_XML_PARAM_DAY_TIME = 0,
	TRAFFICDB_XML_PARAM_DAY_RX,
	TRAFFICDB_XML_PARAM_DAY_TX,
	TRAFFICDB_XML_PARAM_DAY_DURATION,
	TRAFFICDB_XML_PARAM_SESSION_TIME,
	TRAFFICDB_XML_PARAM_SESSION_RX,
	TRAFFICDB_XML_PARAM_SESSION_TX,
	TRAFFICDB_XML_PARAM_SESSION_DURATION,
	TRAFFICDB_XML_PARAM_NULL
};

static gint mmgui_trafficdb_xml_parameter = TRAFFICDB_XML_PARAM_NULL;

static guint mmgui_trafficdb_get_month_days(guint month, guint year);
static guint mmgui_trafficdb_get_year_days(guint year);
static time_t mmgui_trafficdb_truncate_day_timesatmp(time_t currenttime);
static time_t mmgui_trafficdb_get_month_begin_timestamp(guint month, guint year);
static time_t mmgui_trafficdb_get_month_end_timestamp(guint month, guint year);
static time_t mmgui_trafficdb_get_year_begin_timestamp(guint year);
static time_t mmgui_trafficdb_get_year_end_timestamp(guint year);

static mmgui_day_traffic_t mmgui_trafficdb_xml_parse(gchar *xml, gsize size);
static void mmgui_trafficdb_xml_get_element(GMarkupParseContext *context, const gchar *element, const gchar **attr_names, const gchar **attr_values, gpointer data, GError **error);
static void mmgui_trafficdb_xml_get_value(GMarkupParseContext *context, const gchar *text, gsize size, gpointer data, GError **error);
static void mmgui_trafficdb_xml_end_element(GMarkupParseContext *context, const gchar *element, gpointer data, GError **error);

#define	MMGUI_TRAFFICDB_WEEK_END                  6
#define	MMGUI_TRAFFICDB_WEEK_BEGIN                0

#define	MMGUI_TRAFFICDB_LEAP_YEAR_DAYS            366
#define	MMGUI_TRAFFICDB_NORMAL_YEAR_DAYS          365

#define	MMGUI_TRAFFICDB_LEAP_YEAR_FEBRUARY_DAYS   29

static guint mmgui_trafficdb_month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


static guint mmgui_trafficdb_get_month_days(guint month, guint year)
{
	guint days;
	
	if (month > 11) return 0;
	
	if ((month == 1) && ((year % 4) == 0)) {
		days = MMGUI_TRAFFICDB_LEAP_YEAR_FEBRUARY_DAYS;
	} else {
		days = mmgui_trafficdb_month_days[month];
	}
	
	return days;
}

static guint mmgui_trafficdb_get_year_days(guint year)
{
	if ((year % 4) == 0) {
		return MMGUI_TRAFFICDB_LEAP_YEAR_DAYS;
	} else {
		return MMGUI_TRAFFICDB_NORMAL_YEAR_DAYS;
	}
}

time_t mmgui_trafficdb_get_new_day_timesatmp(time_t currenttime, gboolean *monthsend, gboolean *yearsend)
{
	struct tm *ltime;
	time_t restime;
	
	if (monthsend != NULL) *monthsend = FALSE;
	if (yearsend != NULL) *yearsend = FALSE;
	
	ltime = localtime((const time_t *)&currenttime);
	
	g_debug("Current timesatamp: [%u] %s", (guint)currenttime, ctime(&currenttime));
	
	/*Day of week*/
	if ((ltime->tm_wday + 1) > MMGUI_TRAFFICDB_WEEK_END) {
		/*Next week*/
		ltime->tm_wday = MMGUI_TRAFFICDB_WEEK_BEGIN;
	} else {
		/*Next day*/
		ltime->tm_wday++;
	}
	/*Year*/
	if ((ltime->tm_yday + 1) >= mmgui_trafficdb_get_year_days(ltime->tm_year)) {
		/*Next year*/
		ltime->tm_year++;
		ltime->tm_yday = 0;
		if (monthsend != NULL) *monthsend = TRUE;
		if (yearsend != NULL) *yearsend = TRUE;
	} else {
		/*Next day*/
		ltime->tm_yday++;
	}
	/*Month*/
	if ((ltime->tm_mday + 1) > mmgui_trafficdb_get_month_days(ltime->tm_mon, ltime->tm_year)) {
		/*New month*/
		if ((ltime->tm_mon + 1) > 11) {
			/*New year*/
			ltime->tm_mon = 0;
			if (monthsend != NULL) *monthsend = TRUE;
			if (yearsend != NULL) *yearsend = TRUE;
		} else {
			/*Next month*/
			ltime->tm_mon++;
			if (monthsend != NULL) *monthsend = TRUE;
		}
		ltime->tm_mday = 1;
	} else {
		/*Next day in month*/
		ltime->tm_mday++;
	}
	/*Hours, minutes, seconds*/
	ltime->tm_hour = 0;
	ltime->tm_min = 0;
	ltime->tm_sec = 0;
	
	restime = mktime(ltime);
	
	g_debug("New day timestamp: [%u] %s", (guint)restime, ctime(&restime));
	
	return restime;
}

static time_t mmgui_trafficdb_truncate_day_timesatmp(time_t currenttime)
{
	struct tm *ltime;
	time_t restime;
	 
	ltime = localtime((const time_t *)&currenttime);
	
	g_debug("Non-truncated timestamp: [%u] %s", (guint)currenttime, ctime(&currenttime));
	
	/*Hours, minutes, seconds*/
	ltime->tm_hour = 0;
	ltime->tm_min = 0;
	ltime->tm_sec = 0;
	
	restime = mktime(ltime);
	
	g_debug("Truncated timestamp: [%u] %s", (guint)restime, ctime(&restime));
	
	return restime;
}

static time_t mmgui_trafficdb_get_month_begin_timestamp(guint month, guint year)
{
	struct tm ltime;
	time_t restime;
	
	/*Year, month, day*/
	ltime.tm_year = year - 1900;
	ltime.tm_mon = month;
	ltime.tm_mday = 1;
	/*Hours, minutes, seconds*/
	ltime.tm_hour = 0;
	ltime.tm_min = 0;
	ltime.tm_sec = 0;
	
	restime = mktime(&ltime);
	
	g_debug("Month begin timestamp: [%u] %s", (guint)restime, ctime(&restime));
	
	return restime;
}

static time_t mmgui_trafficdb_get_month_end_timestamp(guint month, guint year)
{
	struct tm ltime;
	time_t restime;
	
	/*Year, month, day*/
	ltime.tm_year = year - 1900;
	ltime.tm_mon = month;
	ltime.tm_mday = mmgui_trafficdb_get_month_days(month, year);
	/*Hours, minutes, seconds*/
	ltime.tm_hour = 23;
	ltime.tm_min = 59;
	ltime.tm_sec = 59;
	
	restime = mktime(&ltime);
	
	g_debug("Month end timestamp: [%u] %s", (guint)restime, ctime(&restime));
	
	return restime;
}

static time_t mmgui_trafficdb_get_year_begin_timestamp(guint year)
{
	struct tm ltime;
	time_t restime;
	
	/*Year, month, day*/
	ltime.tm_year = year - 1900;
	ltime.tm_mon = 0;
	ltime.tm_mday = 1;
	/*Hours, minutes, seconds*/
	ltime.tm_hour = 0;
	ltime.tm_min = 0;
	ltime.tm_sec = 0;
	
	restime = mktime(&ltime);
	
	g_debug("Year begin timestamp: [%u] %s", (guint)restime, ctime(&restime));
	
	return restime;
}

static time_t mmgui_trafficdb_get_year_end_timestamp(guint year)
{
	struct tm ltime;
	time_t restime;
	
	/*Year, month, day*/
	ltime.tm_year = year - 1900;
	ltime.tm_mon = 11;
	ltime.tm_mday = mmgui_trafficdb_get_month_days(11, year);
	/*Hours, minutes, seconds*/
	ltime.tm_hour = 23;
	ltime.tm_min = 59;
	ltime.tm_sec = 59;
	
	restime = mktime(&ltime);
	
	g_debug("Year end timestamp: [%u] %s", (guint)restime, ctime(&restime));
	
	return restime;
}

mmgui_trafficdb_t mmgui_trafficdb_open(const gchar *persistentid, const gchar *internalid)
{
	mmgui_trafficdb_t trafficdb;
	const gchar *newfilepath;
	const gchar *newfilename;
	gchar filename[64];
	const gchar *oldfilename;
	time_t currenttime;
	
	if (persistentid == NULL) return NULL;
	
	/*Form path using XDG standard*/
	newfilepath = g_build_path(G_DIR_SEPARATOR_S, g_get_user_data_dir(), "modem-manager-gui", "devices", persistentid, NULL);
	
	if (newfilepath == NULL) return NULL;
	
	/*If directory structure not exists, create it*/
	if (!g_file_test(newfilepath, G_FILE_TEST_IS_DIR)) {
		if (g_mkdir_with_parents(newfilepath, S_IRUSR|S_IWUSR|S_IXUSR|S_IXGRP|S_IXOTH) == -1) {
			g_warning("Failed to make XDG data directory: %s", newfilepath);
		}
	}
	
	/*Form file name*/
	newfilename = g_build_filename(newfilepath, "traffic.gdbm", NULL);
	
	g_free((gchar *)newfilepath);
	
	if (newfilename == NULL) return NULL;
	
	/*If file already exists, just work with it*/
	if ((!g_file_test(newfilename, G_FILE_TEST_EXISTS)) && (internalid != NULL)) {
		/*Form old-style file path*/
		memset(filename, 0, sizeof(filename));
		g_snprintf(filename, sizeof(filename), "traffic-%s.gdbm", internalid);
		
		oldfilename = g_build_filename(g_get_home_dir(), ".config", "modem-manager-gui", filename, NULL);	
		
		if (oldfilename != NULL) {
			/*If file exists in old location, move it*/
			if (g_file_test(oldfilename, G_FILE_TEST_EXISTS)) {
				if (g_rename(oldfilename, newfilename) == -1) {
					g_warning("Failed to move file into XDG data directory: %s -> %s", oldfilename, newfilename);
				}
			}
		}
		
		g_free((gchar *)oldfilename);
	}
	
	trafficdb = g_new(struct _mmgui_trafficdb, 1);
	
	currenttime = time(NULL);
	
	trafficdb->filepath = newfilename;
	trafficdb->presdaytime = mmgui_trafficdb_truncate_day_timesatmp(currenttime);
	trafficdb->nextdaytime = mmgui_trafficdb_get_new_day_timesatmp(currenttime, &trafficdb->monthendstomorrow, &trafficdb->yearendstomorrow);
	trafficdb->sessactive = FALSE;
	trafficdb->sessinitialized = FALSE;
	trafficdb->sessstate = MMGUI_TRAFFICDB_SESSION_STATE_UNKNOWN;
	trafficdb->dayrxbytes = 0;
	trafficdb->daytxbytes = 0;
	trafficdb->dayduration = 0;
	trafficdb->sesstime = 0;
	trafficdb->sessrxbytes = 0;
	trafficdb->sesstxbytes = 0;
	trafficdb->sessduration = 0;
	trafficdb->monthrxbytes = 0;
	trafficdb->monthtxbytes = 0;
	trafficdb->monthduration = 0;
	trafficdb->yearrxbytes = 0;
	trafficdb->yeartxbytes = 0;
	trafficdb->yearduration = 0;
	
	return trafficdb;
}

gboolean mmgui_trafficdb_close(mmgui_trafficdb_t trafficdb)
{
	if (trafficdb == NULL) return FALSE;
	
	if (trafficdb->sessactive) {
		/*Close current session*/
		mmgui_trafficdb_session_close(trafficdb);
	}
	
	if (trafficdb->filepath != NULL) {
		/*Free file path*/
		g_free((gchar *)trafficdb->filepath);
	}
	
	g_free(trafficdb);
	
	return TRUE;
}

gboolean mmgui_trafficdb_traffic_update(mmgui_trafficdb_t trafficdb, mmgui_traffic_update_t update)
{
	time_t currenttime, pastdaytime;
	gint sessdeltatime;
	mmgui_day_traffic_t pasttraffic;
	struct _mmgui_day_traffic traffic;
	
	if ((trafficdb == NULL) || (update == NULL)) return FALSE;
	if ((!trafficdb->sessactive) || (trafficdb->sessstate == MMGUI_TRAFFICDB_SESSION_STATE_UNKNOWN)) return FALSE;
		
	currenttime = time(NULL);
	
	/*Initialization*/
	if (!trafficdb->sessinitialized) {
		/*Count time interval between day begin time and session start time*/
		sessdeltatime = difftime(trafficdb->presdaytime, trafficdb->sesstime);
		/*New session (first session of a day started day before or second session of a day)*/
		if (trafficdb->sessstate == MMGUI_TRAFFICDB_SESSION_STATE_NEW) {
			/*If session was started before day begin, corrections needed*/
			if (sessdeltatime > 0.0) {
				/*Past day session*/
				pastdaytime = mmgui_trafficdb_truncate_day_timesatmp(trafficdb->sesstime);
				pasttraffic = mmgui_trafficdb_day_traffic_read(trafficdb, pastdaytime);
				if (pasttraffic != NULL) {
					pasttraffic->sessrxbytes = update->fullrxbytes;
					pasttraffic->sesstxbytes = update->fulltxbytes;
					pasttraffic->sessduration = sessdeltatime;
					/*Write to database*/
					if (!mmgui_trafficdb_day_traffic_write(trafficdb, pasttraffic)) {
						g_debug("Failed to write traffic statistics to database\n");
					}
					g_free(pasttraffic);
				}
				/*Current session*/
				trafficdb->sesstime = trafficdb->presdaytime;
				trafficdb->sessrxbytes = 0;
				trafficdb->sesstxbytes = 0;
				trafficdb->sessduration = update->fulltime - sessdeltatime;
				/*Count statistics values*/
				trafficdb->monthduration += trafficdb->sessduration;
				trafficdb->yearduration += trafficdb->sessduration;
			} else {
				/*Set session initial values*/
				trafficdb->sessrxbytes = update->fullrxbytes;
				trafficdb->sesstxbytes = update->fulltxbytes;
				trafficdb->sessduration = update->fulltime;
				/*Count statistics values*/
				trafficdb->monthrxbytes += update->fullrxbytes;
				trafficdb->monthtxbytes += update->fulltxbytes;
				trafficdb->monthduration += update->fulltime;
				trafficdb->yearrxbytes += update->fullrxbytes;
				trafficdb->yeartxbytes += update->fulltxbytes;
				trafficdb->yearduration += update->fulltime;
			}
		} else if (trafficdb->sessstate == MMGUI_TRAFFICDB_SESSION_STATE_OLD) {
			/*Old session*/
			/*If session was started before day begin, do not do anything*/
			if (sessdeltatime <= 0.0) {
				/*Update with actual information*/
				trafficdb->sessrxbytes = update->fullrxbytes;
				trafficdb->sesstxbytes = update->fulltxbytes;
				trafficdb->sessduration = update->fulltime;
			}
		}
		trafficdb->sessinitialized = TRUE;
	} else {
		/*Already initialized session*/
		/*Write traffic statistics at the end of the day*/
		if (currenttime > trafficdb->nextdaytime) {
			/*Write current statistics*/
			memset(&traffic, 0, sizeof(traffic));
			traffic.daytime = trafficdb->presdaytime;
			traffic.dayrxbytes = trafficdb->dayrxbytes;
			traffic.daytxbytes = trafficdb->daytxbytes;
			traffic.dayduration = trafficdb->dayduration;
			traffic.sesstime = trafficdb->sesstime;
			traffic.sessrxbytes = trafficdb->sessrxbytes;
			traffic.sesstxbytes = trafficdb->sesstxbytes;
			traffic.sessduration = trafficdb->sessduration;
			/*Write to database*/
			if (!mmgui_trafficdb_day_traffic_write(trafficdb, &traffic)) {
				g_debug("Failed to write traffic statistics to database\n");
			}
			/*Correct values*/
			/*Year and month statistics values*/
			if (trafficdb->monthendstomorrow) {
				trafficdb->monthrxbytes = update->deltarxbytes;
				trafficdb->monthtxbytes = update->deltarxbytes;
				trafficdb->monthduration = update->deltarxbytes;
			}
			if (trafficdb->yearendstomorrow) {
				trafficdb->yearrxbytes = update->deltarxbytes;
				trafficdb->yeartxbytes = update->deltarxbytes;
				trafficdb->yearduration = update->deltarxbytes;
			}
			/*Current session*/
			trafficdb->dayrxbytes = 0;
			trafficdb->daytxbytes = 0;
			trafficdb->dayduration = 0;
			trafficdb->sessrxbytes = update->deltarxbytes;
			trafficdb->sesstxbytes = update->deltatxbytes;
			trafficdb->sessduration = update->deltaduration;
			trafficdb->presdaytime = mmgui_trafficdb_truncate_day_timesatmp(currenttime);
			trafficdb->nextdaytime = mmgui_trafficdb_get_new_day_timesatmp(currenttime, &trafficdb->monthendstomorrow, &trafficdb->yearendstomorrow);
		} else {
			/*Count statistics values*/
			trafficdb->monthrxbytes += update->deltarxbytes;
			trafficdb->monthtxbytes += update->deltatxbytes;
			trafficdb->monthduration += update->deltaduration;
			trafficdb->yearrxbytes += update->deltarxbytes;
			trafficdb->yeartxbytes += update->deltatxbytes;
			trafficdb->yearduration += update->deltaduration;
			/*Count current values*/
			trafficdb->sessrxbytes += update->deltarxbytes;
			trafficdb->sesstxbytes += update->deltatxbytes;
			trafficdb->sessduration += update->deltaduration;
		}
	}
	
	return TRUE;
}

gboolean mmgui_trafficdb_session_new(mmgui_trafficdb_t trafficdb, time_t starttime)
{
	time_t currenttime, daytime;
	mmgui_day_traffic_t traffic;
	GDBM_FILE db;
	datum key, data;
	gchar dayid[64];
	time_t monthbegtime, monthendtime, yearbegtime, yearendtime, curdaytime;
	struct tm timestruct;
	guint64 currxbytes, curtxbytes, curduration;
	
	if (trafficdb == NULL) return FALSE;
	
	if (trafficdb->sessactive) {
		//Close current session
		mmgui_trafficdb_session_close(trafficdb);
	}
	
	currenttime = time(NULL);
	
	daytime = mmgui_trafficdb_truncate_day_timesatmp(currenttime);
	
	traffic = mmgui_trafficdb_day_traffic_read(trafficdb, daytime);
	
	if (traffic) {
		/*Restore available session data*/
		if (starttime == traffic->sesstime) {
			trafficdb->sessstate = MMGUI_TRAFFICDB_SESSION_STATE_OLD;
			trafficdb->sesstime = traffic->sesstime;
			trafficdb->dayrxbytes = traffic->dayrxbytes;
			trafficdb->daytxbytes = traffic->daytxbytes;
			trafficdb->dayduration = traffic->dayduration;
			trafficdb->sessrxbytes = traffic->sessrxbytes;
			trafficdb->sesstxbytes = traffic->sesstxbytes;
			trafficdb->sessduration = traffic->sessduration;
		} else {
			trafficdb->sessstate = MMGUI_TRAFFICDB_SESSION_STATE_NEW;
			trafficdb->sesstime = starttime;
			trafficdb->dayrxbytes = traffic->dayrxbytes + traffic->sessrxbytes;
			trafficdb->daytxbytes = traffic->daytxbytes + traffic->sesstxbytes;
			trafficdb->dayduration = traffic->dayduration + traffic->sessduration;
			trafficdb->sessrxbytes = 0;
			trafficdb->sesstxbytes = 0;
			trafficdb->sessduration = 0;
		}
		g_free(traffic);
	} else {
		/*No sessions started today*/
		trafficdb->sessstate = MMGUI_TRAFFICDB_SESSION_STATE_NEW;
		trafficdb->sesstime = starttime;
		trafficdb->dayrxbytes = 0;
		trafficdb->daytxbytes = 0;
		trafficdb->dayduration = 0;
		trafficdb->sessrxbytes = 0;
		trafficdb->sesstxbytes = 0;
		trafficdb->sessduration = 0;
	}
	
	/*Set common values*/
	trafficdb->presdaytime = mmgui_trafficdb_truncate_day_timesatmp(currenttime);
	trafficdb->nextdaytime = mmgui_trafficdb_get_new_day_timesatmp(currenttime, &trafficdb->monthendstomorrow, &trafficdb->yearendstomorrow);
	trafficdb->sessactive = TRUE;
	trafficdb->sessinitialized = FALSE;
	
	/*Year and month values*/
	trafficdb->yearrxbytes = 0;
	trafficdb->yeartxbytes = 0;
	trafficdb->yearduration = 0;
	trafficdb->monthrxbytes = 0;
	trafficdb->monthtxbytes = 0;
	trafficdb->monthduration = 0;
	
	localtime_r((const time_t *)&currenttime, &timestruct);
	
	monthbegtime = mmgui_trafficdb_get_month_begin_timestamp(timestruct.tm_mon, timestruct.tm_year + 1900);
	monthendtime = mmgui_trafficdb_get_month_end_timestamp(timestruct.tm_mon, timestruct.tm_year + 1900);
	
	yearbegtime = mmgui_trafficdb_get_year_begin_timestamp(timestruct.tm_year + 1900);
	yearendtime = mmgui_trafficdb_get_year_end_timestamp(timestruct.tm_year + 1900);
	
	db = gdbm_open((gchar *)trafficdb->filepath, 0, GDBM_READER, 0755, 0);
	
	if (db != NULL) {
		/*Get data from database if possible*/
		key = gdbm_firstkey(db);
		if (key.dptr != NULL) {
			do {
				memset(dayid, 0, sizeof(dayid));
				strncpy(dayid, key.dptr, key.dsize);
				curdaytime = (time_t)strtoull(dayid, NULL, 10);
				if ((difftime(yearendtime, curdaytime) >= 0.0) && (difftime(curdaytime, yearbegtime) > 0.0)) {
					data = gdbm_fetch(db, key);
					if (data.dptr != NULL) {
						traffic = mmgui_trafficdb_xml_parse(data.dptr, data.dsize);
						if (curdaytime == trafficdb->presdaytime) {
							/*Active session correction*/
							currxbytes = trafficdb->dayrxbytes + trafficdb->sessrxbytes;
							curtxbytes = trafficdb->daytxbytes + trafficdb->sesstxbytes;
							curduration = trafficdb->dayduration + trafficdb->sessduration;
						} else {
							/*Database values*/
							currxbytes = traffic->dayrxbytes + traffic->sessrxbytes;
							curtxbytes = traffic->daytxbytes + traffic->sesstxbytes;
							curduration = traffic->dayduration + traffic->sessduration;
						}
						
						/*Year statistics*/
						trafficdb->yearrxbytes += currxbytes;
						trafficdb->yeartxbytes += curtxbytes;
						trafficdb->yearduration += curduration;
						
						/*Month statistics*/
						if ((difftime(monthendtime, curdaytime) >= 0.0) && (difftime(curdaytime, monthbegtime) > 0.0)) {
							trafficdb->monthrxbytes += currxbytes;
							trafficdb->monthtxbytes += curtxbytes;
							trafficdb->monthduration += curduration;
						}
					}
				}
				key = gdbm_nextkey(db, key);
			} while (key.dptr != NULL);
		}
		gdbm_close(db);
	}
	
	return TRUE;
}

gboolean mmgui_trafficdb_session_close(mmgui_trafficdb_t trafficdb)
{
	time_t currenttime, daytime;
	struct _mmgui_day_traffic traffic;
	
	if (trafficdb == NULL) return FALSE;
	if (!trafficdb->sessactive) return FALSE;
	
	currenttime = time(NULL);
	
	daytime = mmgui_trafficdb_truncate_day_timesatmp(currenttime);
	
	/*Write current statistics*/
	memset(&traffic, 0, sizeof(traffic));
	traffic.daytime = daytime;
	traffic.dayrxbytes = trafficdb->dayrxbytes;
	traffic.daytxbytes = trafficdb->daytxbytes;
	traffic.dayduration = trafficdb->dayduration;
	traffic.sesstime = trafficdb->sesstime;
	traffic.sessrxbytes = trafficdb->sessrxbytes;
	traffic.sesstxbytes = trafficdb->sesstxbytes;
	traffic.sessduration = trafficdb->sessduration;
	
	/*Write to database*/
	if (!mmgui_trafficdb_day_traffic_write(trafficdb, &traffic)) {
		g_debug("Failed to write traffic statistics to database\n");
	}
	
	/*Zero values*/
	trafficdb->nextdaytime = 0;
	trafficdb->sessactive = FALSE;
	trafficdb->sessinitialized = FALSE;
	trafficdb->sessstate = MMGUI_TRAFFICDB_SESSION_STATE_UNKNOWN;
	trafficdb->dayrxbytes = 0;
	trafficdb->daytxbytes = 0;
	trafficdb->dayduration = 0;
	trafficdb->sesstime = 0;
	trafficdb->sessrxbytes = 0;
	trafficdb->sesstxbytes = 0;
	trafficdb->sessduration = 0;
	
	return TRUE;
}

gboolean mmgui_trafficdb_session_get_day_traffic(mmgui_trafficdb_t trafficdb, mmgui_day_traffic_t traffic)
{
	time_t currenttime;
	
	if ((trafficdb == NULL) || (traffic == NULL)) return FALSE;
	if (!trafficdb->sessactive) return FALSE;
	
	currenttime = time(NULL);
	
	traffic->daytime = mmgui_trafficdb_truncate_day_timesatmp(currenttime);
	traffic->dayrxbytes = trafficdb->dayrxbytes;
	traffic->daytxbytes = trafficdb->daytxbytes;
	traffic->dayduration = trafficdb->dayduration;
	traffic->sesstime = trafficdb->sesstime;
	traffic->sessrxbytes = trafficdb->sessrxbytes;
	traffic->sesstxbytes = trafficdb->sesstxbytes;
	traffic->sessduration = trafficdb->sessduration;
	
	return TRUE;
}

gboolean mmgui_trafficdb_day_traffic_write(mmgui_trafficdb_t trafficdb, mmgui_day_traffic_t daytraffic)
{
	GDBM_FILE db;
	gchar dayid[64];
	gint idlen;
	datum key, data;
	gchar *daytrafficxml;
	
	if ((trafficdb == NULL) || (daytraffic == NULL)) return FALSE;
	if (trafficdb->filepath == NULL) return FALSE;
			
	db = gdbm_open((gchar *)trafficdb->filepath, 0, GDBM_WRCREAT, 0755, 0);
	
	if (db == NULL) return FALSE;
	
	memset(dayid, 0, sizeof(dayid));
	idlen = snprintf(dayid, sizeof(dayid), "%" G_GUINT64_FORMAT "", daytraffic->daytime);
	key.dptr = (gchar *)dayid;
	key.dsize = idlen;
		
	daytrafficxml = g_strdup_printf(TRAFFICDB_DAY_XML,
									daytraffic->daytime,
									daytraffic->dayrxbytes,
									daytraffic->daytxbytes,
									daytraffic->dayduration,
									daytraffic->sesstime,
									daytraffic->sessrxbytes,
									daytraffic->sesstxbytes,
									daytraffic->sessduration);
	
	data.dptr = daytrafficxml;
	data.dsize = strlen(daytrafficxml);
	
	if (gdbm_store(db, key, data, GDBM_REPLACE) == -1) {
		g_warning("Unable to write to database");
		gdbm_close(db);
		g_free(daytrafficxml);
		return FALSE;
	}
	
	gdbm_sync(db);
	gdbm_close(db);
	
	g_free(daytrafficxml);
	
	return TRUE;
}

static gint mmgui_trafficdb_traffic_list_sort_compare(gconstpointer a, gconstpointer b)
{
	mmgui_day_traffic_t day1, day2;
		
	day1 = (mmgui_day_traffic_t)a;
	day2 = (mmgui_day_traffic_t)b;
	
	if (day1->daytime < day2->daytime) {
		return -1;
	} else if (day1->daytime > day2->daytime) {
		return 1;
	} else {
		return 0;
	}
}

GSList *mmgui_trafficdb_get_traffic_list_for_month(mmgui_trafficdb_t trafficdb, guint month, guint year)
{
	GDBM_FILE db;
	time_t begtime, endtime;
	time_t daytime;
	GSList *list;
	mmgui_day_traffic_t daytraffic;
	datum key, data;
	gchar dayid[64];
	struct tm *timespec;
	gboolean currentstatscorrected;
	
	if (trafficdb == NULL) return NULL;
	if (trafficdb->filepath == NULL) return NULL;
	
	begtime = mmgui_trafficdb_get_month_begin_timestamp(month, year);
	endtime = mmgui_trafficdb_get_month_end_timestamp(month, year);
	
	list = NULL;
	
	currentstatscorrected = FALSE;
	
	db = gdbm_open((gchar *)trafficdb->filepath, 0, GDBM_READER, 0755, 0);
	
	if (db != NULL) {
		/*Get data from database if possible*/
		key = gdbm_firstkey(db);
		
		if (key.dptr != NULL) {
			do {
				memset(dayid, 0, sizeof(dayid));
				strncpy(dayid, key.dptr, key.dsize);
				daytime = (time_t)strtoull(dayid, NULL, 10);
				if ((difftime(endtime, daytime) >= 0.0) && (difftime(daytime, begtime) > 0.0)) {
					data = gdbm_fetch(db, key);
					if (data.dptr != NULL) {
						daytraffic = mmgui_trafficdb_xml_parse(data.dptr, data.dsize);
						if ((daytime == trafficdb->presdaytime) && (trafficdb->sessactive)) {
							/*Today statistics correction*/
							daytraffic->dayrxbytes = trafficdb->dayrxbytes;
							daytraffic->daytxbytes = trafficdb->daytxbytes;
							daytraffic->dayduration = trafficdb->dayduration;
							daytraffic->sessrxbytes = trafficdb->sessrxbytes;
							daytraffic->sesstxbytes = trafficdb->sesstxbytes;
							daytraffic->sessduration = trafficdb->sessduration;
							currentstatscorrected = TRUE;
						}
						list = g_slist_prepend(list, daytraffic);
					}
				}
				key = gdbm_nextkey(db, key);
			} while (key.dptr != NULL);
		}
		gdbm_close(db);
	}
	
	/*If statistics for current day must be included*/
	daytime = time(NULL);
	timespec = localtime((const time_t *)&daytime);
	
	if ((month == timespec->tm_mon) && (!currentstatscorrected)) {
		if ((trafficdb->dayduration + trafficdb->sessduration) > 0) {
			daytraffic = g_new(struct _mmgui_day_traffic, 1);
			daytraffic->daytime = trafficdb->presdaytime;
			daytraffic->dayrxbytes = trafficdb->dayrxbytes;
			daytraffic->daytxbytes = trafficdb->daytxbytes;
			daytraffic->dayduration = trafficdb->dayduration;
			daytraffic->sesstime = trafficdb->sesstime;
			daytraffic->sessrxbytes = trafficdb->sessrxbytes;
			daytraffic->sesstxbytes = trafficdb->sesstxbytes;
			daytraffic->sessduration = trafficdb->sessduration;
			list = g_slist_prepend(list, daytraffic);
		}
	}
	
	if (list != NULL) {
		list = g_slist_sort(list, mmgui_trafficdb_traffic_list_sort_compare);
	} 
	
	return list;
}

void mmgui_trafficdb_free_traffic_list_for_month(GSList *trafficlist)
{
	if (trafficlist == NULL) return;
	
	g_slist_foreach(trafficlist, (GFunc)g_free, NULL);
	g_slist_free(trafficlist);
}

mmgui_day_traffic_t mmgui_trafficdb_day_traffic_read(mmgui_trafficdb_t trafficdb, time_t daytime)
{
	GDBM_FILE db;
	gchar dayid[64];
	gint idlen;
	datum key, data;
	mmgui_day_traffic_t traffic;
		
	if (trafficdb == NULL) return NULL;
	if (trafficdb->filepath == NULL) return NULL;
	
	//Open database
	db = gdbm_open((gchar *)trafficdb->filepath, 0, GDBM_READER, 0755, 0);
	
	if (db == NULL) return NULL;
	
	traffic = NULL;
	
	memset(dayid, 0, sizeof(dayid));
	idlen = snprintf(dayid, sizeof(dayid), "%" G_GUINT64_FORMAT "", (guint64)mmgui_trafficdb_truncate_day_timesatmp(daytime));
	key.dptr = (gchar *)dayid;
	key.dsize = idlen;
	
	if (gdbm_exists(db, key)) {
		data = gdbm_fetch(db, key);
		if (data.dptr != NULL) {
			traffic = mmgui_trafficdb_xml_parse(data.dptr, data.dsize);
		}
	}
	
	gdbm_close(db);
	
	return traffic;
}

static mmgui_day_traffic_t mmgui_trafficdb_xml_parse(gchar *xml, gsize size)
{
	mmgui_day_traffic_t traffic;
	GMarkupParser mp;
	GMarkupParseContext *mpc;
	GError *error = NULL;
	
	traffic = g_new(struct _mmgui_day_traffic, 1);
	
	traffic->daytime = 0;
	traffic->dayrxbytes = 0;
	traffic->daytxbytes = 0;
	traffic->dayduration = 0;
	traffic->sesstime = 0;
	traffic->sessrxbytes = 0;
	traffic->sesstxbytes = 0;
	traffic->sessduration = 0;
	
	mp.start_element = mmgui_trafficdb_xml_get_element;
	mp.end_element = mmgui_trafficdb_xml_end_element;
	mp.text = mmgui_trafficdb_xml_get_value;
	mp.passthrough = NULL;
	mp.error = NULL;
	
	mpc = g_markup_parse_context_new(&mp, 0, (gpointer)traffic, NULL);
	g_markup_parse_context_parse(mpc, xml, size, &error);
	if (error != NULL) {
		g_free(traffic);
		g_error_free(error);
		g_markup_parse_context_free(mpc);
		return NULL;
	}
	g_markup_parse_context_free(mpc);
	
	return traffic;
}

static void mmgui_trafficdb_xml_get_element(GMarkupParseContext *context, const gchar *element, const gchar **attr_names, const gchar **attr_values, gpointer data, GError **error)
{
	if (g_str_equal(element, "daytime")) {
		mmgui_trafficdb_xml_parameter = TRAFFICDB_XML_PARAM_DAY_TIME;
	} else if (g_str_equal(element, "dayrxbytes")) {
		mmgui_trafficdb_xml_parameter = TRAFFICDB_XML_PARAM_DAY_RX;
	} else if (g_str_equal(element, "daytxbytes")) {
		mmgui_trafficdb_xml_parameter = TRAFFICDB_XML_PARAM_DAY_TX;
	} else if (g_str_equal(element, "dayduration")) {
		mmgui_trafficdb_xml_parameter = TRAFFICDB_XML_PARAM_DAY_DURATION;
	} else if (g_str_equal(element, "sesstime")) {
		mmgui_trafficdb_xml_parameter = TRAFFICDB_XML_PARAM_SESSION_TIME;
	} else if (g_str_equal(element, "sessrxbytes")) {
		mmgui_trafficdb_xml_parameter = TRAFFICDB_XML_PARAM_SESSION_RX;
	} else if (g_str_equal(element, "sesstxbytes")) {
		mmgui_trafficdb_xml_parameter = TRAFFICDB_XML_PARAM_SESSION_TX;
	} else if (g_str_equal(element, "sessduration")) {
		mmgui_trafficdb_xml_parameter = TRAFFICDB_XML_PARAM_SESSION_DURATION;
	} else {
		mmgui_trafficdb_xml_parameter = TRAFFICDB_XML_PARAM_NULL;
	}
}

static void mmgui_trafficdb_xml_get_value(GMarkupParseContext *context, const gchar *text, gsize size, gpointer data, GError **error)
{
	mmgui_day_traffic_t daytraffic;
		
	daytraffic = (mmgui_day_traffic_t)data;
	
	if (mmgui_trafficdb_xml_parameter == TRAFFICDB_XML_PARAM_NULL) return;
	
	switch (mmgui_trafficdb_xml_parameter) {
		case TRAFFICDB_XML_PARAM_DAY_TIME:
			daytraffic->daytime = (guint64)strtoull(text, NULL, 10);
			break;
		case TRAFFICDB_XML_PARAM_DAY_RX:
			daytraffic->dayrxbytes = (guint64)strtoull(text, NULL, 10);
			break;
		case TRAFFICDB_XML_PARAM_DAY_TX:
			daytraffic->daytxbytes = (guint64)strtoull(text, NULL, 10);
			break;
		case TRAFFICDB_XML_PARAM_DAY_DURATION:
			daytraffic->dayduration = (guint)strtoul(text, NULL, 10);
			break;
		case TRAFFICDB_XML_PARAM_SESSION_TIME:
			daytraffic->sesstime = (guint64)strtoull(text, NULL, 10);
			break;
		case TRAFFICDB_XML_PARAM_SESSION_RX:
			daytraffic->sessrxbytes = (guint64)strtoull(text, NULL, 10);
			break;
		case TRAFFICDB_XML_PARAM_SESSION_TX:
			daytraffic->sesstxbytes = (guint64)strtoull(text, NULL, 10);
			break;
		case TRAFFICDB_XML_PARAM_SESSION_DURATION:
			daytraffic->sessduration = (guint)strtoul(text, NULL, 10);
			break;
		default:
			break;
	}
}

static void mmgui_trafficdb_xml_end_element(GMarkupParseContext *context, const gchar *element, gpointer data, GError **error)
{
	if (!g_str_equal(element, "traffic")) {
		mmgui_trafficdb_xml_parameter = TRAFFICDB_XML_PARAM_NULL;
	}
}
