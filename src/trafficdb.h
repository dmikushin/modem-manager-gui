/*
 *      trafficdb.h
 *      
 *      Copyright 2012-2013 Alex <alex@linuxonly.ru>
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

#ifndef __TRAFFICDB_H__
#define __TRAFFICDB_H__

enum _mmgui_trafficdb_session_state {
	MMGUI_TRAFFICDB_SESSION_STATE_UNKNOWN = 0,
	MMGUI_TRAFFICDB_SESSION_STATE_NEW,
	MMGUI_TRAFFICDB_SESSION_STATE_OLD
};
	

struct _mmgui_trafficdb {
	const gchar *filepath;
	gboolean sessactive;
	gboolean sessinitialized;
	gboolean monthendstomorrow;
	gboolean yearendstomorrow;
	time_t presdaytime;
	time_t nextdaytime;
	time_t sesstime;
	guint sessstate;
	guint64 dayrxbytes;
	guint64 daytxbytes;
	guint dayduration;
	guint64 sessrxbytes;
	guint64 sesstxbytes;
	guint64 sessduration;
	guint64 monthrxbytes;
	guint64 monthtxbytes;
	guint64 monthduration;
	guint64 yearrxbytes;
	guint64 yeartxbytes;
	guint64 yearduration;
};

typedef struct _mmgui_trafficdb *mmgui_trafficdb_t;

struct _mmgui_day_traffic {
	/*Day statistics*/
	guint64 daytime;
	guint64 dayrxbytes;
	guint64 daytxbytes;
	guint dayduration;
	/*Last session statistics*/
	guint64 sesstime;
	guint64 sessrxbytes;
	guint64 sesstxbytes;
	guint sessduration;
};

typedef struct _mmgui_day_traffic *mmgui_day_traffic_t;

struct _mmgui_traffic_update {
	guint64 fullrxbytes;
	guint64 fulltxbytes;
	guint64 fulltime;
	guint deltarxbytes;
	guint deltatxbytes;
	guint deltaduration;
};

typedef struct _mmgui_traffic_update *mmgui_traffic_update_t;

time_t mmgui_trafficdb_get_new_day_timesatmp(time_t currenttime, gboolean *monthsend, gboolean *yearsend);
mmgui_trafficdb_t mmgui_trafficdb_open(const gchar *persistentid, const gchar *internalid);
gboolean mmgui_trafficdb_close(mmgui_trafficdb_t trafficdb);
gboolean mmgui_trafficdb_traffic_update(mmgui_trafficdb_t trafficdb, mmgui_traffic_update_t update);
gboolean mmgui_trafficdb_session_new(mmgui_trafficdb_t trafficdb, time_t starttime);
gboolean mmgui_trafficdb_session_close(mmgui_trafficdb_t trafficdb);
gboolean mmgui_trafficdb_session_get_day_traffic(mmgui_trafficdb_t trafficdb, mmgui_day_traffic_t daytraffic);
gboolean mmgui_trafficdb_day_traffic_write(mmgui_trafficdb_t trafficdb, mmgui_day_traffic_t daytraffic);
GSList *mmgui_trafficdb_get_traffic_list_for_month(mmgui_trafficdb_t trafficdb, guint month, guint year);
void mmgui_trafficdb_free_traffic_list_for_month(GSList *trafficlist);
mmgui_day_traffic_t mmgui_trafficdb_day_traffic_read(mmgui_trafficdb_t trafficdb, time_t daytime);

#endif /* __SMSDB_H__ */
