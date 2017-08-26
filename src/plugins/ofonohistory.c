/*
 *      ofonohistory.c
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

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <gdbm.h>

#define OFONO_API_SUBJECT_TO_CHANGE
#include <ofono/plugin.h>
#include <ofono/log.h>
#include <ofono/history.h>
#include <ofono/types.h>
#include <ofono/modem.h>

#include "../modules/historyshm.h"

#define MMGUI_HISTORY_SHM_DB_MESSAGE "<message>\n\t<localtime>%" G_GUINT64_FORMAT "</localtime>\n\t<remotetime>%" G_GUINT64_FORMAT "</remotetime>\n\t<driver>%s</driver>\n\t<sender>%s</sender>\n\t<text>%s</text>\n</message>\n\n"

/*Driver*/
struct _mmgui_history_driver
{
	/*Name*/
	gchar *name;
	/*Reference count*/
	guint refcount;
	/*Shared memory segment*/
	gint shmid;
	mmgui_history_shm_t shmaddr;
};

typedef struct _mmgui_history_driver *mmgui_history_driver_t;

/*Modem*/
struct _mmgui_history_modem
{
	/*Modem pointer*/
	gpointer ofonoid;
	/*Modem identifier*/
	gint identifier;
	/*Ofono driver name*/
	mmgui_history_driver_t driver;
};

typedef struct _mmgui_history_modem *mmgui_history_modem_t;

/*Plugin internal data structure*/
struct _mmgui_history_data
{
	/*Database*/
	GDBM_FILE db;
	/*Current modems data*/
	GHashTable *modems;
	/*Current drivers data*/
	GHashTable *drivers;
};

typedef struct _mmgui_history_data *mmgui_history_data_t;


/*Global data*/
static mmgui_history_data_t historydata = NULL;


guint64 mmgui_history_get_driver_from_key(const gchar *key, const gsize keylen, gchar *buf, gsize buflen)
{
	gchar *drvstr, *timestr;
	gsize drvlen;
	guint64 localts;
	
	if ((key == NULL) || (keylen == 0) || (buf == NULL) || (buflen == 0)) return 0;
	drvstr = strchr(key, '@');
	
	if (drvstr == NULL) return 0;
	
	timestr = strchr(drvstr+1, '@');
	
	if (timestr == NULL) return 0;
		
	drvlen = timestr - drvstr - 1;
	
	if (drvlen > buflen) {
		drvlen = buflen;
	}
	
	localts = atol(timestr+1);
	
	memset(buf, 0, buflen);
	
	if (strncpy(buf, drvstr+1, drvlen) != NULL) {
		return localts;
	} else {
		return 0;
	}
}

gchar *mmgui_history_parse_driver_string(const gchar *path, gint *identifier)
{
	gsize pathlen, driverlen;
	guint idpos;
	gchar *drivername;
	
	if (path == NULL) return NULL;
	if ((path[0] != '/') || (strchr(path+1, '_') == NULL)) return NULL;
	
	pathlen = strlen(path);
	driverlen = 0;
	
	/*Find delimiter (driver string is in format: /driver_identifier)*/
	for (idpos=pathlen; idpos>0; idpos--) {
		if (path[idpos] == '_') {
			driverlen = idpos-1;
		}
	}
	
	if (driverlen > 0) {
		/*Allocate driver name buffer and copy name*/
		drivername = g_try_malloc0(driverlen+1);
		if (drivername != NULL) {
			memcpy(drivername, path+1, driverlen);
			/*Return identifier if needed*/
			if (identifier != NULL) {
				*identifier = atoi(path+idpos+1);
			}
		}
		
		return drivername;
	}
	
	return NULL;
}

static void mmgui_history_remove_driver(gpointer data)
{
	mmgui_history_driver_t driver;
	
	if (data == NULL) return;
	
	driver = (mmgui_history_driver_t)data;
	
	if (driver->name != NULL) {
		g_free(driver->name);
	}
	
	g_free(driver);
}

static void mmgui_history_remove_modem(gpointer data)
{
	mmgui_history_modem_t modem;
	
	if (data == NULL) return;
	
	modem = (mmgui_history_modem_t)data;
	
	g_free(modem);
}

static gboolean mmgui_history_create_driver_shm(mmgui_history_driver_t driver)
{
	gchar shmname[64];
	mode_t old_umask;
	
	if (driver == NULL) return FALSE;
	
	/*Shared memory segment name*/
	memset(shmname, 0, sizeof(shmname));
	snprintf(shmname, sizeof(shmname), MMGUI_HISTORY_SHM_SEGMENT_NAME, driver->name);
	/*Shared memory object*/
	old_umask = umask(0);
	driver->shmid = shm_open(shmname, O_CREAT|O_RDWR|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH/*MMGUI_HISTORY_SHM_SEGMENT_PERM*/);
	umask(old_umask);
	if (driver->shmid == -1) {
		driver->shmaddr = NULL;
		return FALSE;
	}
	/*Set shared memory fragment size*/
	if (ftruncate(driver->shmid, sizeof(struct _mmgui_history_shm)) == -1 ) {
		driver->shmaddr = NULL;
		close(driver->shmid);
		shm_unlink(shmname);
		return FALSE;
	}
	/*Map shared memory fragment*/
	driver->shmaddr = (mmgui_history_shm_t)mmap(0, sizeof(struct _mmgui_history_shm), PROT_WRITE|PROT_READ, MAP_SHARED, driver->shmid, 0);
	if ((char *)driver->shmaddr == (char*)-1) {
		driver->shmaddr = NULL;
		close(driver->shmid);
		shm_unlink(shmname);
		return FALSE;
	}
	/*Set initial values*/
	driver->shmaddr->flags = MMGUI_HISTORY_SHM_FLAGS_NONE;
	driver->shmaddr->identifier = -1;
	driver->shmaddr->synctime = 0;
	
	return TRUE;
}

static void mmgui_history_remove_synchronized_messages_foreach(gpointer data, gpointer user_data)
{
	gchar *rmkeystr;
	datum key;
	
	rmkeystr = (gchar *)data;
	
	key.dptr  = rmkeystr;
	key.dsize = strlen(rmkeystr);
	
	if (gdbm_exists(historydata->db, key)) {
		if (gdbm_delete(historydata->db, key) == 0) {
			ofono_debug("[HISTORY PLUGIN] Removed syncronized message: %s", rmkeystr);
		}
	}
}

static void mmgui_history_remove_driver_shm(mmgui_history_driver_t driver)
{
	gchar shmname[64];
	GSList *rmkeys;
	gchar *rmkeystr; 
	datum key;
	gchar drvstr[128];
	guint64 localts;
	
	if (driver == NULL) return;
	
	/*Shared memory segment name*/
	memset(shmname, 0, sizeof(shmname));
	snprintf(shmname, sizeof(shmname), MMGUI_HISTORY_SHM_SEGMENT_NAME, driver->name);
	/*Remove shared memory segment*/
	if (driver->shmaddr != NULL) {
		/*Remove messages if data is synchronized*/
		if (driver->shmaddr->flags & MMGUI_HISTORY_SHM_FLAGS_SYNC) {
			/*Form list of keys to remove*/
			rmkeys = NULL;
			key = gdbm_firstkey(historydata->db);
			if (key.dptr != NULL) {
				do {
					localts = mmgui_history_get_driver_from_key(key.dptr, key.dsize, (gchar *)&drvstr, sizeof(drvstr));
					if (localts != 0) {
						if ((g_str_equal(drvstr, driver->name)) && ((driver->shmaddr->synctime == 0) || ((driver->shmaddr->synctime != 0) && (localts <= driver->shmaddr->synctime)))) {
							rmkeystr = g_try_malloc0(key.dsize+1);
							memcpy(rmkeystr, key.dptr, key.dsize);
							if (rmkeystr != NULL) {
								rmkeys = g_slist_prepend(rmkeys, rmkeystr);
							}
						}
					}
					key = gdbm_nextkey(historydata->db, key);
				} while (key.dptr != NULL);
			}
			/*Work with list*/
			if (rmkeys != NULL) {
				/*Remove messages from database*/
				g_slist_foreach(rmkeys, mmgui_history_remove_synchronized_messages_foreach, NULL);
				/*Free list*/
				g_slist_foreach(rmkeys, (GFunc)g_free, NULL);
				/*Reorganize*/
				gdbm_reorganize(historydata->db);
				ofono_debug("[HISTORY PLUGIN] Messages removed for driver: %s", driver->name);
			}
		}
		/*Remove memory segment*/
		munmap(driver->shmaddr, sizeof(struct _mmgui_history_shm));
		close(driver->shmid);
		shm_unlink(shmname);
	}
}

static int mmgui_history_probe(struct ofono_history_context *context)
{
	const gchar *path;
	mmgui_history_modem_t modem;
	mmgui_history_driver_t driver;
	gchar *drivername;
	
	if (context->modem == NULL) return 0;
	
	path = ofono_modem_get_path(context->modem);
		
	if ((path != NULL) && (historydata != NULL)) {
		if (historydata->modems != NULL) {
			modem = g_try_new0(struct _mmgui_history_modem, 1);
			if (modem != NULL) {
				/*New modem*/
				modem->ofonoid = context->modem;
				/*Driver*/
				drivername = mmgui_history_parse_driver_string(path, &(modem->identifier));
				driver = g_hash_table_lookup(historydata->modems, context->modem);
				if (driver == NULL) {
					/*New driver*/
					driver = g_try_new0(struct _mmgui_history_driver, 1);
					if (driver != NULL) {
						driver->name = drivername;
						driver->refcount = 0;
						/*Create shared memory segment*/
						mmgui_history_create_driver_shm(driver);
						/*Connect modem with driver*/
						modem->driver = driver;
						driver->refcount++;
						/*Insert driver into hash table*/
						g_hash_table_insert(historydata->drivers, driver->name, driver);
					} else {
						modem->driver = NULL;
						g_free(drivername);
					}
				} else {
					/*Existing driver*/
					modem->driver = driver;
					driver->refcount++;
					g_free(drivername);
				}
				/*Insert modem into hash table*/
				g_hash_table_insert(historydata->modems, modem->ofonoid, modem);
				/*Debug output*/
				ofono_debug("[HISTORY PLUGIN] Probe for modem: %p (%s - %i)", modem->ofonoid, modem->driver->name, modem->identifier);
			}
		}
	}
	
	return 0;
}

static void mmgui_history_remove(struct ofono_history_context *context)
{
	mmgui_history_modem_t modem;
	
	if (historydata != NULL) {
		if (historydata->modems != NULL) {
			modem = g_hash_table_lookup(historydata->modems, context->modem);
			if (modem != NULL) {
				/*Modem exists*/
				if (modem->driver != NULL) {
					/*Debug output*/
					ofono_debug("[HISTORY PLUGIN] Remove modem: %p (%s)", modem->ofonoid, modem->driver->name);
					
					modem->driver->refcount--;
					if (modem->driver->refcount == 0) {
						/*Remove shared memory segment*/
						mmgui_history_remove_driver_shm(modem->driver);
						/*Remove driver*/
						g_hash_table_remove(historydata->drivers, modem->driver->name);
					} 
				}
				/*Remove from hash table*/
				g_hash_table_remove(historydata->modems, context->modem);
			}
		}
	}
}

static void mmgui_history_call_ended(struct ofono_history_context *context, const struct ofono_call *call, time_t start, time_t end)
{
	ofono_debug("[HISTORY PLUGIN] Call Ended on modem: %p", context->modem);
}

static void mmgui_history_call_missed(struct ofono_history_context *context, const struct ofono_call *call, time_t when)
{
	ofono_debug("[HISTORY PLUGIN] Call Missed on modem: %p", context->modem);
}

static void mmgui_history_sms_received(struct ofono_history_context *context, const struct ofono_uuid *uuid, const char *from, const struct tm *remote, const struct tm *local, const char *text)
{
	mmgui_history_modem_t modem;
	datum key, data;
	const gchar *uuidstr;
	gchar msgid[128];
	gsize msgidlen;
	guint64 localts, remotets; 
	gchar *messagexml;
		
	if (historydata == NULL) return;
	if (historydata->modems == NULL) return;
	
	modem = g_hash_table_lookup(historydata->modems, context->modem);
	
	if (modem == NULL) return;
	
	if ((modem->driver != NULL) && (historydata->db != NULL)) {
		/*Test if modem already opened with application - no need to cache message*/
		if ((modem->driver->shmaddr->identifier != -1) && (modem->driver->shmaddr->identifier == modem->identifier) && (modem->driver->shmaddr->flags & MMGUI_HISTORY_SHM_FLAGS_SYNC)) {
			return;
		}
		/*Message identifier (uuid@driver@timestamp)*/
		uuidstr = ofono_uuid_to_str(uuid);
		localts = (guint64)time(NULL);
		memset(msgid, 0, sizeof(msgid));
		msgidlen = snprintf(msgid, sizeof(msgid), "%s@%s@%" G_GUINT64_FORMAT, uuidstr, modem->driver->name, localts);
		key.dptr = (gchar *)msgid;
		key.dsize = msgidlen;
		/*Timestamp*/
		localts = (guint64)mktime((struct tm *)local);
		remotets = (guint64)mktime((struct tm *)remote);
		/*Form XML*/
		messagexml = g_strdup_printf(MMGUI_HISTORY_SHM_DB_MESSAGE,
										localts,
										remotets,
										modem->driver->name,
										from,
										text);
		data.dptr = messagexml;
		data.dsize = strlen(messagexml);
		/*Save to database*/
		if (gdbm_store(historydata->db, key, data, GDBM_REPLACE) == -1) {
			gdbm_close(historydata->db);
			g_free(messagexml);
			return;
		}
		/*Synchronize database*/
		gdbm_sync(historydata->db);
		g_free(messagexml);
		/*Set driver state unsynchronized*/
		modem->driver->shmaddr->flags = MMGUI_HISTORY_SHM_FLAGS_NONE;
						
		ofono_debug("[HISTORY PLUGIN] Incoming SMS on modem: %p (%s)", context->modem, modem->driver->name);
	}
}

static void mmgui_history_sms_send_pending(struct ofono_history_context *context, const struct ofono_uuid *uuid, const char *to, time_t when, const char *text)
{
	char buf[128];

	ofono_debug("[HISTORY PLUGIN] Sending SMS on modem: %p", context->modem);
	ofono_debug("InternalMessageId: %s", ofono_uuid_to_str(uuid));
	ofono_debug("To: %s:", to);

	strftime(buf, 127, "%Y-%m-%dT%H:%M:%S%z", localtime(&when));
	buf[127] = '\0';
	ofono_debug("Local Time: %s", buf);
	ofono_debug("Text: %s", text);
}

static void mmgui_history_sms_send_status(struct ofono_history_context *context, const struct ofono_uuid *uuid, time_t when, enum ofono_history_sms_status s)
{
	char buf[128];

	strftime(buf, 127, "%Y-%m-%dT%H:%M:%S%z", localtime(&when));
	buf[127] = '\0';

	switch (s) {
	case OFONO_HISTORY_SMS_STATUS_PENDING:
		break;
	case OFONO_HISTORY_SMS_STATUS_SUBMITTED:
		ofono_debug("SMS %s submitted successfully",
					ofono_uuid_to_str(uuid));
		ofono_debug("Submission Time: %s", buf);
		break;
	case OFONO_HISTORY_SMS_STATUS_SUBMIT_FAILED:
		ofono_debug("Sending SMS %s failed", ofono_uuid_to_str(uuid));
		ofono_debug("Failure Time: %s", buf);
		break;
	case OFONO_HISTORY_SMS_STATUS_SUBMIT_CANCELLED:
		ofono_debug("Submission of SMS %s was canceled",
					ofono_uuid_to_str(uuid));
		ofono_debug("Cancel time: %s", buf);
		break;
	case OFONO_HISTORY_SMS_STATUS_DELIVERED:
		ofono_debug("SMS delivered, msg_id: %s, time: %s",
					ofono_uuid_to_str(uuid), buf);
		break;
	case OFONO_HISTORY_SMS_STATUS_DELIVER_FAILED:
		ofono_debug("SMS undeliverable, msg_id: %s, time: %s",
					ofono_uuid_to_str(uuid), buf);
		break;
	default:
		break;
	}
}

static struct ofono_history_driver mmgui_driver = {
	.name = "MMGUI SMS History",
	.probe = mmgui_history_probe,
	.remove = mmgui_history_remove,
	.call_ended = mmgui_history_call_ended,
	.call_missed = mmgui_history_call_missed,
	.sms_received = mmgui_history_sms_received,
	.sms_send_pending = mmgui_history_sms_send_pending,
	.sms_send_status = mmgui_history_sms_send_status,
};

static int mmgui_history_init(void)
{
	int res;
	
	ofono_debug("[HISTORY PLUGIN] Init");
	
	if (historydata == NULL) {
		/*Global data segment*/
		historydata = g_try_new0(struct _mmgui_history_data, 1);
		if (historydata == NULL) {
			return -ENOMEM;
		}
		/*Create database directory if not exists*/
		if (g_mkdir_with_parents(MMGUI_HISTORY_SHM_DB_PATH, MMGUI_HISTORY_SHM_DB_PERM) != 0) {
			ofono_debug("Error while creating database directory: %s", strerror(errno));
			return -ENOENT;
		}
		/*Open database*/
		historydata->db = gdbm_open(MMGUI_HISTORY_SHM_DB_FILE, 0, GDBM_WRCREAT, MMGUI_HISTORY_SHM_DB_PERM, 0);
		if (historydata->db == NULL) {
			ofono_debug("Error while opening database");
			return -ENOENT;
		}
		/*Create modems hash table*/
		historydata->modems = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, mmgui_history_remove_modem);
		/*Create drivers hash table*/
		historydata->drivers = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, mmgui_history_remove_driver);
	}
	
	res = ofono_history_driver_register(&mmgui_driver);
	
	return res;
}

static void mmgui_history_exit(void)
{
	ofono_debug("[HISTORY PLUGIN] Exit");
	
	if (historydata != NULL) {
		/*Close database*/
		if (historydata->db != NULL) {
			gdbm_sync(historydata->db);
			gdbm_close(historydata->db);
		}
		/*Destroy modems hash table*/
		if (historydata->modems != NULL) {
			g_hash_table_destroy(historydata->modems);
		}
		/*Destroy drivers hash table*/
		if (historydata->drivers != NULL) {
			g_hash_table_destroy(historydata->drivers);
		}
		/*Free memory segment*/
		g_free(historydata);
		historydata = NULL;
	}
	
	ofono_history_driver_unregister(&mmgui_driver);
}

OFONO_PLUGIN_DEFINE(mmgui_history, 
					"MMGUI SMS History Plugin",
					OFONO_VERSION,
					OFONO_PLUGIN_PRIORITY_DEFAULT,
					mmgui_history_init,
					mmgui_history_exit)
