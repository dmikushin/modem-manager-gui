/*
 *      historyshm.c
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
#include <gmodule.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <gdbm.h>

#include "historyshm.h"
#include "../smsdb.h"
#include "../encoding.h"

enum _mmgui_history_shm_client_xml_elements {
	MMGUI_HISTORY_SHM_XML_PARAM_LOCALTIME = 0,
	MMGUI_HISTORY_SHM_XML_PARAM_REMOTETIME,
	MMGUI_HISTORY_SHM_XML_PARAM_DRIVER,
	MMGUI_HISTORY_SHM_XML_PARAM_SENDER,
	MMGUI_HISTORY_SHM_XML_PARAM_TEXT,
	MMGUI_HISTORY_SHM_XML_PARAM_NULL
};

static gint mmgui_history_shm_client_xml_parameter = MMGUI_HISTORY_SHM_XML_PARAM_NULL;

static mmgui_sms_message_t mmgui_history_client_xml_parse(gchar *xml, gsize size);
static void mmgui_history_client_xml_get_element(GMarkupParseContext *context, const gchar *element, const gchar **attr_names, const gchar **attr_values, gpointer data, GError **error);
static void mmgui_history_client_xml_get_value(GMarkupParseContext *context, const gchar *text, gsize size, gpointer data, GError **error);
static void mmgui_history_client_xml_end_element(GMarkupParseContext *context, const gchar *element, gpointer data, GError **error);

mmgui_history_shm_client_t mmgui_history_client_new(void)
{
	GDBM_FILE db;
	mmgui_history_shm_client_t client;
	
	db = gdbm_open(MMGUI_HISTORY_SHM_DB_FILE, 0, GDBM_READER|GDBM_NOLOCK, MMGUI_HISTORY_SHM_DB_PERM, 0);
	
	if (db == NULL) return NULL;
	
	client = (mmgui_history_shm_client_t)g_new0(struct _mmgui_history_shm_client, 1);
	
	client->db = db;
	
	return client;
	
}

void mmgui_history_client_close(mmgui_history_shm_client_t client)
{
	if (client == NULL) return;
	
	if (client->db != NULL) {
		gdbm_close(client->db);
	}
}

gboolean mmgui_history_client_open_device(mmgui_history_shm_client_t client, const gchar *devpath)
{
	gchar *drivername;
	gint identifier;
	gchar shmname[64];
	
	if ((client == NULL) || (devpath == NULL)) return FALSE;
	if (client->devopened) return FALSE;
	
	/*Driver name and identifier*/
	drivername = mmgui_history_parse_driver_string(devpath, &identifier);
	
	if (drivername == NULL) return FALSE;
	
	/*Shared memory segment name*/
	memset(shmname, 0, sizeof(shmname));
	snprintf(shmname, sizeof(shmname), MMGUI_HISTORY_SHM_SEGMENT_NAME, drivername);
	/*Shared memory object*/
	client->shmid = shm_open(shmname, O_RDWR, 0);
	if (client->shmid == -1) {
		client->devopened = FALSE;
		client->shmaddr = NULL;
		g_free(drivername);
		return FALSE;
	}
	/*Map shared memory fragment*/
	client->shmaddr = (mmgui_history_shm_t)mmap(0, sizeof(struct _mmgui_history_shm), PROT_WRITE|PROT_READ, MAP_SHARED, client->shmid, 0);
	if ((char *)client->shmaddr == (char*)-1) {
		client->devopened = FALSE;
		client->shmaddr = NULL;
		close(client->shmid);
		g_free(drivername);
		return FALSE;
	}
	/*Set device identifier*/
	client->shmaddr->identifier = identifier;
	
	client->drivername = drivername;
	
	/*Device opened*/
	client->devopened = TRUE;
	
	return TRUE;
}

gboolean mmgui_history_client_close_device(mmgui_history_shm_client_t client)
{
	if (client == NULL) return FALSE;
	if (!client->devopened) return FALSE;
	
	/*Drop device identifier*/
	client->shmaddr->identifier = -1;
	
	/*Remove shared memory segment*/
	if (client->shmaddr != NULL) {
		/*Remove memory segment*/
		munmap(client->shmaddr, sizeof(struct _mmgui_history_shm));
		close(client->shmid);
	}
	
	if (client->drivername != NULL) {
		g_free(client->drivername);
	}
	
	/*Device closed*/
	client->devopened = FALSE;
	
	return TRUE;
}

GSList *mmgui_history_client_enum_messages(mmgui_history_shm_client_t client)
{
	GSList *messages;
	mmgui_sms_message_t message;
	datum key, data;
	gchar drvstr[128];
	guint64 localts, maxts;
	
	if (client == NULL) return NULL;
	if ((!client->devopened) || (client->db == NULL) || (client->shmaddr == NULL) || (client->drivername == NULL)) return NULL;
	
	messages = NULL;
	maxts = 0;
	
	key = gdbm_firstkey(client->db);
		
	if (key.dptr != NULL) {
		do {
			localts = mmgui_history_get_driver_from_key(key.dptr, key.dsize, (gchar *)&drvstr, sizeof(drvstr));
			if (localts != 0) {
				if ((g_str_equal(drvstr, client->drivername)) && ((client->shmaddr->synctime == 0) || ((client->shmaddr->synctime != 0) && (localts > client->shmaddr->synctime)))) {
					data = gdbm_fetch(client->db, key);
					if (data.dptr != NULL) {
						message = mmgui_history_client_xml_parse(data.dptr, data.dsize);
						if (message != NULL) {
							messages = g_slist_prepend(messages, message);
							if (localts > maxts) {
								maxts = localts;
							}
						}
					}
				}
			}
			key = gdbm_nextkey(client->db, key);
		} while (key.dptr != NULL);
	}
	/*Maximum timestamp for next synchronizations*/
	if (messages != NULL) {
		client->shmaddr->synctime = maxts;
	}
	/*Set synchronized*/
	client->shmaddr->flags |= MMGUI_HISTORY_SHM_FLAGS_SYNC;
		
	return messages;
}

static mmgui_sms_message_t mmgui_history_client_xml_parse(gchar *xml, gsize size)
{
	mmgui_sms_message_t message;
	GMarkupParser mp;
	GMarkupParseContext *mpc;
	GError *error = NULL;
	
	message = mmgui_smsdb_message_create();
	
	mp.start_element = mmgui_history_client_xml_get_element;
	mp.end_element = mmgui_history_client_xml_end_element;
	mp.text = mmgui_history_client_xml_get_value;
	mp.passthrough = NULL;
	mp.error = NULL;
	
	mpc = g_markup_parse_context_new(&mp, 0, (gpointer)message, NULL);
	g_markup_parse_context_parse(mpc, xml, size, &error);
	if (error != NULL) {
		g_debug("Error parsing XML: %s", error->message);
		g_error_free(error);
		g_markup_parse_context_free(mpc);
		mmgui_smsdb_message_free(message);
		return NULL;
	}
	g_markup_parse_context_free(mpc);
	
	return message;
}

static void mmgui_history_client_xml_get_element(GMarkupParseContext *context, const gchar *element, const gchar **attr_names, const gchar **attr_values, gpointer data, GError **error)
{
	if (g_str_equal(element, "localtime")) {
		mmgui_history_shm_client_xml_parameter = MMGUI_HISTORY_SHM_XML_PARAM_LOCALTIME;
	} else if (g_str_equal(element, "remotetime")) {
		mmgui_history_shm_client_xml_parameter = MMGUI_HISTORY_SHM_XML_PARAM_REMOTETIME;
	} else if (g_str_equal(element, "driver")) {
		mmgui_history_shm_client_xml_parameter = MMGUI_HISTORY_SHM_XML_PARAM_DRIVER;
	} else if (g_str_equal(element, "sender")) {
		mmgui_history_shm_client_xml_parameter = MMGUI_HISTORY_SHM_XML_PARAM_SENDER;
	} else if (g_str_equal(element, "text")) {
		mmgui_history_shm_client_xml_parameter = MMGUI_HISTORY_SHM_XML_PARAM_TEXT;
	} else {
		mmgui_history_shm_client_xml_parameter = MMGUI_HISTORY_SHM_XML_PARAM_NULL;
	}
}

static void mmgui_history_client_xml_get_value(GMarkupParseContext *context, const gchar *text, gsize size, gpointer data, GError **error)
{
	mmgui_sms_message_t message;
	time_t timestamp;
	gchar *escstr;
	
	message = (mmgui_sms_message_t)data;
	
	if (mmgui_history_shm_client_xml_parameter == MMGUI_HISTORY_SHM_XML_PARAM_NULL) return;
	
	switch (mmgui_history_shm_client_xml_parameter) {
		case MMGUI_HISTORY_SHM_XML_PARAM_LOCALTIME:
			timestamp = (time_t)atol(text);
			mmgui_smsdb_message_set_timestamp(message, timestamp);
			break;
		case MMGUI_HISTORY_SHM_XML_PARAM_REMOTETIME:
			//not needed now
			break;
		case MMGUI_HISTORY_SHM_XML_PARAM_DRIVER:
			//not needed now
			break;
		case MMGUI_HISTORY_SHM_XML_PARAM_SENDER:
			escstr = encoding_unescape_xml_markup(text, size);
			if (escstr != NULL) {
				mmgui_smsdb_message_set_number(message, (const gchar *)escstr);
				g_free(escstr);
			} else {
				mmgui_smsdb_message_set_number(message, text);
			}
			break;
		case MMGUI_HISTORY_SHM_XML_PARAM_TEXT:
			escstr = encoding_unescape_xml_markup(text, size);
			if (escstr != NULL) {
				mmgui_smsdb_message_set_text(message, (const gchar *)escstr, FALSE);
				g_free(escstr);
			} else {
				mmgui_smsdb_message_set_text(message, text, FALSE);
			}
			break;
		default:
			break;
	}
}

static void mmgui_history_client_xml_end_element(GMarkupParseContext *context, const gchar *element, gpointer data, GError **error)
{
	if (!g_str_equal(element, "message")) {
		mmgui_history_shm_client_xml_parameter = MMGUI_HISTORY_SHM_XML_PARAM_NULL;
	}
}

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
