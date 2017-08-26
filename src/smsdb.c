/*
 *      smsdb.c
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

#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <string.h>
#include <gdbm.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "encoding.h"
#include "smsdb.h"

#define MMGUI_SMSDB_SMS_MESSAGE_XML "<sms>\n\t<number>%s</number>\n\t<time>%lu</time>\n\t<binary>%u</binary>\n\t<servicenumber>%s</servicenumber>\n\t<text>%s</text>\n\t<read>%u</read>\n\t<folder>%u</folder>\n</sms>\n\n"

#define MMGUI_SMSDB_READ_TAG        "\n\t<read>"
#define MMGUI_SMSDB_TRAILER_TAG     "\n</sms>\n\n"
#define MMGUI_SMSDB_TRAILER_PARAMS  "\n\t<read>%u</read>\n\t<folder>%u</folder>\n</sms>\n\n"

#define MMGUI_SMSDB_ACCESS_MASK     0755

enum _mmgui_smsdb_xml_elements {
	MMGUI_SMSDB_XML_PARAM_NUMBER = 0,
	MMGUI_SMSDB_XML_PARAM_TIME,
	MMGUI_SMSDB_XML_PARAM_BINARY,
	MMGUI_SMSDB_XML_PARAM_SERVICENUMBER,
	MMGUI_SMSDB_XML_PARAM_TEXT,
	MMGUI_SMSDB_XML_PARAM_READ,
	MMGUI_SMSDB_XML_PARAM_FOLDER,
	MMGUI_SMSDB_XML_PARAM_NULL
};

static gint mmgui_smsdb_xml_parameter = MMGUI_SMSDB_XML_PARAM_NULL;

static gint mmgui_smsdb_sms_message_sort_compare(gconstpointer a, gconstpointer b);
static void mmgui_smsdb_free_sms_list_foreach(gpointer data, gpointer user_data);
static mmgui_sms_message_t mmgui_smsdb_xml_parse(gchar *xml, gsize size);
static void mmgui_smsdb_xml_get_element(GMarkupParseContext *context, const gchar *element, const gchar **attr_names, const gchar **attr_values, gpointer data, GError **error);
static void mmgui_smsdb_xml_get_value(GMarkupParseContext *context, const gchar *text, gsize size, gpointer data, GError **error);
static void mmgui_smsdb_xml_end_element(GMarkupParseContext *context, const gchar *element, gpointer data, GError **error);

smsdb_t mmgui_smsdb_open(const gchar *persistentid, const gchar *internalid)
{
	smsdb_t smsdb;
	const gchar *newfilepath;
	const gchar *newfilename;
	gchar filename[64];
	const gchar *oldfilename;
	
	if (persistentid == NULL) return NULL;
	
	//Form path using XDG standard
	newfilepath = g_build_path(G_DIR_SEPARATOR_S, g_get_user_data_dir(), "modem-manager-gui", "devices", persistentid, NULL);
	
	if (newfilepath == NULL) return NULL;
	
	//If directory structure not exists, create it
	if (!g_file_test(newfilepath, G_FILE_TEST_IS_DIR)) {
		if (g_mkdir_with_parents(newfilepath, S_IRUSR|S_IWUSR|S_IXUSR|S_IXGRP|S_IXOTH) == -1) {
			g_warning("Failed to make XDG data directory: %s", newfilepath);
		}
	}
	
	//Form file name
	newfilename = g_build_filename(newfilepath, "sms.gdbm", NULL);
	
	g_free((gchar *)newfilepath);
	
	if (newfilename == NULL) return NULL;
	
	//If file already exists, just work with it
	if ((!g_file_test(newfilename, G_FILE_TEST_EXISTS)) && (internalid != NULL)) {
		//Form old-style file path
		memset(filename, 0, sizeof(filename));
		g_snprintf(filename, sizeof(filename), "sms-%s.gdbm", internalid);
		
		oldfilename = g_build_filename(g_get_home_dir(), ".config", "modem-manager-gui", filename, NULL);	
		
		if (oldfilename != NULL) {
			//If file exists in old location, move it
			if (g_file_test(oldfilename, G_FILE_TEST_EXISTS)) {
				if (g_rename(oldfilename, newfilename) == -1) {
					g_warning("Failed to move file into XDG data directory: %s -> %s", oldfilename, newfilename);
				}
			}
		}
		
		g_free((gchar *)oldfilename);
	}
	
	smsdb = g_new(struct _smsdb, 1);
	
	smsdb->filepath = newfilename;
	smsdb->unreadmessages = 0;
	
	return smsdb;
}

gboolean mmgui_smsdb_close(smsdb_t smsdb)
{
	if (smsdb == NULL) return FALSE;
	
	if (smsdb->filepath != NULL) {
		g_free((gchar *)smsdb->filepath);
	}
	smsdb->unreadmessages = 0;
	
	g_free(smsdb);
	
	return TRUE;
}

mmgui_sms_message_t mmgui_smsdb_message_create(void)
{
	mmgui_sms_message_t message;
	
	message = g_new(struct _mmgui_sms_message, 1);
	message->timestamp = time(NULL);
	message->read = FALSE;
	message->binary = FALSE;
	message->folder = MMGUI_SMSDB_SMS_FOLDER_INCOMING;
	message->number = NULL;
	message->svcnumber = NULL;
	message->idents = NULL;
	message->text = NULL;
	
	return message;
}

void mmgui_smsdb_message_free(mmgui_sms_message_t message)
{
	if (message == NULL) return;
	
	if (message->number != NULL) {
		g_free(message->number);
	}
	
	if (message->svcnumber != NULL) {
		g_free(message->svcnumber);
	}
	
	if (message->idents != NULL) {
		g_array_free(message->idents, TRUE);
	}
	
	if (message->text != NULL) {
		g_string_free(message->text, TRUE);
	}
		
	g_free(message);
}

gboolean mmgui_smsdb_message_set_number(mmgui_sms_message_t message, const gchar *number)
{
	gchar *escnumber;
	gsize srclen;
	
	if ((message == NULL) || (number == NULL)) return FALSE;
	
	srclen = strlen(number);
	
	if (srclen == 0) return FALSE;
	
	escnumber = encoding_clear_special_symbols(g_strdup(number), srclen);
	
	if (escnumber != NULL) {
		if (message->number != NULL) {
			g_free(message->number);
		}
		message->number = escnumber;
		return TRUE;
	} else {
		return FALSE;
	}
}

const gchar *mmgui_smsdb_message_get_number(mmgui_sms_message_t message)
{
	if (message == NULL) return NULL;
	
	return message->number;
}

gboolean mmgui_smsdb_message_set_service_number(mmgui_sms_message_t message, const gchar *number)
{
	gchar *escnumber;
	gsize srclen;
	
	if ((message == NULL) || (number == NULL)) return FALSE;
	
	srclen = strlen(number);
	
	if (srclen == 0) return FALSE;
	
	escnumber = encoding_clear_special_symbols(g_strdup(number), srclen);
	
	if (escnumber != NULL) {
		if (message->svcnumber != NULL) {
			g_free(message->svcnumber);
		}
		message->svcnumber = escnumber;
		return TRUE;
	} else {
		return FALSE;
	}
}

const gchar *mmgui_smsdb_message_get_service_number(mmgui_sms_message_t message)
{
	if (message == NULL) return NULL;
	
	return message->svcnumber;
}

gboolean mmgui_smsdb_message_set_text(mmgui_sms_message_t message, const gchar *text, gboolean append)
{
	if ((message == NULL) || (text == NULL)) return FALSE;
	
	if (message->binary) return FALSE;
	
	if (!append) {
		if (message->text != NULL) {
			g_string_free(message->text, TRUE);
		}
		message->text = g_string_new(text);
	} else {
		if (message->text != NULL) {
			message->text = g_string_append_c(message->text, ' ');
			message->text = g_string_append(message->text, text);
		} else {
			message->text = g_string_new(text);
		}
	}
	
	return TRUE;
}

const gchar *mmgui_smsdb_message_get_text(mmgui_sms_message_t message)
{
	if (message == NULL) return NULL;
	
	if (message->text != NULL) {
		return message->text->str;
	} else {
		return NULL;
	}
}

gboolean mmgui_smsdb_message_set_data(mmgui_sms_message_t message, const gchar *data, gsize len, gboolean append)
{
	guint srclen, index;
	
	if ((message == NULL) || (data == NULL) || (len == 0)) return FALSE;
	
	if (!message->binary) return FALSE;
	
	if (!append) {
		if (message->text != NULL) {
			g_string_free(message->text, TRUE);
		}
		message->text = g_string_new_len(NULL, len*2+1);
		for (index=0; index<len; index++) {
			if ((guchar)data[index] < 0x10) {
				g_sprintf(message->text->str+(index*2), "0%1x", (guchar)data[index]);
			} else {
				g_sprintf(message->text->str+(index*2), "%2x", (guchar)data[index]);
			}
		}
		message->text->str[len*2] = '\0';
	} else {
		if (message->text != NULL) {
			message->text = g_string_append(message->text, "00");
			srclen = message->text->len-1;
			message->text = g_string_set_size(message->text, srclen+len*2+1);
			for (index=0; index<len; index++) {
				if ((guchar)data[index] < 0x10) {
					g_sprintf(message->text->str+(srclen+index*2), "0%1x", (guchar)data[index]);
				} else {
					g_sprintf(message->text->str+(srclen+index*2), "%2x", (guchar)data[index]);
				}
			}
			message->text->str[srclen+len*2] = '\0';
		} else {
			message->text = g_string_new_len(NULL, len*2+1);
			for (index=0; index<len; index++) {
				if ((guchar)data[index] < 0x10) {
					g_sprintf(message->text->str+(index*2), "0%1x", (guchar)data[index]);
				} else {
					g_sprintf(message->text->str+(index*2), "%2x", (guchar)data[index]);
				}
			}
			message->text->str[len*2] = '\0';
		}
	}
	
	return TRUE;
}

gboolean mmgui_smsdb_message_set_identifier(mmgui_sms_message_t message, guint ident, gboolean append)
{
	if (message == NULL) return FALSE;
	
	if (!append) {
		if (message->idents != NULL) {
			g_array_free(message->idents, TRUE);
		}
		message->idents = g_array_new(FALSE, TRUE, sizeof(guint));
		g_array_append_val(message->idents, ident);
	} else {
		if (message->idents != NULL) {
			g_array_append_val(message->idents, ident);
		} else {
			message->idents = g_array_new(FALSE, TRUE, sizeof(guint));
			g_array_append_val(message->idents, ident);
		}
	}
	
	return TRUE;
}

guint mmgui_smsdb_message_get_identifier(mmgui_sms_message_t message)
{
	guint ident;
	
	if (message == NULL) return 0;
	
	if (message->idents != NULL) {
		ident = g_array_index(message->idents, guint, 0);
	} else {
		ident = 0;
	}
	
	return ident;
}

gboolean mmgui_smsdb_message_set_timestamp(mmgui_sms_message_t message, time_t timestamp)
{
	if (message == NULL) return FALSE;
	
	message->timestamp = timestamp;
	
	return TRUE;
}

time_t mmgui_smsdb_message_get_timestamp(mmgui_sms_message_t message)
{
	if (message == NULL) return 0;
	
	return message->timestamp;
}

gboolean mmgui_smsdb_message_set_read(mmgui_sms_message_t message, gboolean read)
{
	if (message == NULL) return FALSE;
	
	message->read = read;
	
	return TRUE;
}

gboolean mmgui_smsdb_message_get_read(mmgui_sms_message_t message)
{
	if (message == NULL) return FALSE;
	
	return message->read;
}

gboolean mmgui_smsdb_message_set_folder(mmgui_sms_message_t message, enum _mmgui_smsdb_sms_folder folder)
{
	if (message == NULL) return FALSE;
	
	switch (folder) {
		case MMGUI_SMSDB_SMS_FOLDER_INCOMING:
			message->folder = MMGUI_SMSDB_SMS_FOLDER_INCOMING;
			break;
		case MMGUI_SMSDB_SMS_FOLDER_SENT:
			message->folder = MMGUI_SMSDB_SMS_FOLDER_SENT;
			break;
		case MMGUI_SMSDB_SMS_FOLDER_DRAFTS:
			message->folder = MMGUI_SMSDB_SMS_FOLDER_DRAFTS;
			break;
		default:
			message->folder = MMGUI_SMSDB_SMS_FOLDER_INCOMING;
			break;
	}
	
	return TRUE;
}

enum _mmgui_smsdb_sms_folder mmgui_smsdb_message_get_folder(mmgui_sms_message_t message)
{
	if (message == NULL) return MMGUI_SMSDB_SMS_FOLDER_INCOMING;
	
	return message->folder;
}

gboolean mmgui_smsdb_message_set_binary(mmgui_sms_message_t message, gboolean binary)
{
	if (message == NULL) return FALSE;
	
	message->binary = binary;
	
	return TRUE;
}

gboolean mmgui_smsdb_message_get_binary(mmgui_sms_message_t message)
{
	if (message == NULL) return FALSE;
	
	return message->binary;
}

gulong mmgui_smsdb_message_get_db_identifier(mmgui_sms_message_t message)
{
	if (message == NULL) return 0;
	
	return message->dbid;
}

guint mmgui_smsdb_get_unread_messages(smsdb_t smsdb)
{
	if (smsdb == NULL) return 0;
	
	return smsdb->unreadmessages;
}

gboolean mmgui_smsdb_add_sms(smsdb_t smsdb, mmgui_sms_message_t message)
{
	GDBM_FILE db;
	gchar smsid[64];
	gulong idvalue;
	gint idlen;
	datum key, data;
	gchar *smsnumber;
	gchar *smstext;
	gchar *smsxml;
	
	if ((smsdb == NULL) || (message == NULL)) return FALSE;
	if (smsdb->filepath == NULL) return FALSE;
	if ((message->number == NULL) || ((message->text->str == NULL))) return FALSE;
		
	db = gdbm_open((gchar *)smsdb->filepath, 0, GDBM_WRCREAT, MMGUI_SMSDB_ACCESS_MASK, 0);
	
	if (db == NULL) return FALSE;
	
	do {
		idvalue = (gulong)random();
		memset(smsid, 0, sizeof(smsid));
		idlen = snprintf(smsid, sizeof(smsid), "%lu", idvalue);
		key.dptr = (gchar *)smsid;
		key.dsize = idlen;
	} while (gdbm_exists(db, key));
	
	message->dbid = idvalue;
	
	smsnumber = g_markup_escape_text(message->number, -1);
	
	if (smsnumber == NULL) {
		g_warning("Unable to convert SMS number string");
		gdbm_close(db);
		return FALSE;
	}
	
	smstext = g_markup_escape_text(message->text->str, -1);
	
	if (smstext == NULL) {
		g_warning("Unable to convert SMS text string");
		g_free(smsnumber);
		gdbm_close(db);
		return FALSE;
	}
	
	smsxml = g_strdup_printf(MMGUI_SMSDB_SMS_MESSAGE_XML,
								smsnumber,
								message->timestamp,
								message->binary,
								message->svcnumber,
								smstext,
								message->read,
								message->folder);
	
	data.dptr = smsxml;
	data.dsize = strlen(smsxml);
	
	if (gdbm_store(db, key, data, GDBM_REPLACE) == -1) {
		g_warning("Unable to write to database");
		gdbm_close(db);
		g_free(smsxml);
		return FALSE;
	}
	
	gdbm_sync(db);
	gdbm_close(db);
	
	if (!message->read) {
		smsdb->unreadmessages++;
	}
	
	g_free(smsxml);
	g_free(smsnumber);
	g_free(smstext);
	
	return TRUE;
}

static gint mmgui_smsdb_sms_message_sort_compare(gconstpointer a, gconstpointer b)
{
	mmgui_sms_message_t sms1, sms2;
		
	sms1 = (mmgui_sms_message_t)a;
	sms2 = (mmgui_sms_message_t)b;
	
	if (sms1->timestamp < sms2->timestamp) {
		return -1;
	} else if (sms1->timestamp > sms2->timestamp) {
		return 1;
	} else {
		return 0;
	}
}

GSList *mmgui_smsdb_read_sms_list(smsdb_t smsdb)
{
	GDBM_FILE db;
	GSList *list;
	mmgui_sms_message_t message;
	datum key, data;
	gchar smsid[64];
	
	if (smsdb == NULL) return NULL;
	if (smsdb->filepath == NULL) return NULL;
	
	db = gdbm_open((gchar *)smsdb->filepath, 0, GDBM_READER, MMGUI_SMSDB_ACCESS_MASK, 0);
	
	if (db == NULL) return NULL;
	
	smsdb->unreadmessages = 0;
	
	list = NULL;
	
	key = gdbm_firstkey(db);
	
	if (key.dptr != NULL) {
		do {
			data = gdbm_fetch(db, key);
			if (data.dptr != NULL) {
				message = mmgui_smsdb_xml_parse(data.dptr, data.dsize);
				if (message != NULL) {
					if (!message->read) {
						smsdb->unreadmessages++;
					}
					memset(smsid, 0, sizeof(smsid));
					strncpy(smsid, key.dptr, key.dsize);
					message->dbid = strtoul(smsid, NULL, 0);
					list = g_slist_prepend(list, message);
				}
			}
			key = gdbm_nextkey(db, key);
		} while (key.dptr != NULL);
	}
	
	gdbm_close(db);
	
	if (list != NULL) {
		list = g_slist_sort(list, mmgui_smsdb_sms_message_sort_compare);
	} 
	
	return list;
}

static void mmgui_smsdb_free_sms_list_foreach(gpointer data, gpointer user_data)
{
	mmgui_sms_message_t message;
	
	if (data != NULL) return;
	
	message = (mmgui_sms_message_t)data;
	
	if (message->number != NULL) {
		g_free(message->number);
	}
	
	if (message->svcnumber != NULL) {
		g_free(message->svcnumber);
	}
	
	if (message->idents != NULL) {
		g_array_free(message->idents, TRUE);
	}
	
	if (message->text != NULL) {
		g_string_free(message->text, TRUE);
	}
}

void mmgui_smsdb_message_free_list(GSList *smslist)
{
	if (smslist == NULL) return;
	
	g_slist_foreach(smslist, mmgui_smsdb_free_sms_list_foreach, NULL);
	g_slist_free(smslist);
}

mmgui_sms_message_t mmgui_smsdb_read_sms_message(smsdb_t smsdb, gulong idvalue)
{
	GDBM_FILE db;
	gchar smsid[64];
	gint idlen;
	datum key, data;
	mmgui_sms_message_t message;
		
	if (smsdb == NULL) return NULL;
	if (smsdb->filepath == NULL) return NULL;
	
	db = gdbm_open((gchar *)smsdb->filepath, 0, GDBM_READER, MMGUI_SMSDB_ACCESS_MASK, 0);
	
	if (db == NULL) return NULL;
	
	message = NULL;
	
	memset(smsid, 0, sizeof(smsid));
	idlen = snprintf(smsid, sizeof(smsid), "%lu", idvalue);
	key.dptr = (gchar *)smsid;
	key.dsize = idlen;
	
	if (gdbm_exists(db, key)) {
		data = gdbm_fetch(db, key);
		if (data.dptr != NULL) {
			message = mmgui_smsdb_xml_parse(data.dptr, data.dsize);
			message->dbid = idvalue;
		}
	}
	
	gdbm_close(db);
	
	return message;
}

gboolean mmgui_smsdb_remove_sms_message(smsdb_t smsdb, gulong idvalue)
{
	GDBM_FILE db;
	gchar smsid[64];
	gint idlen, unreaddelta;
	datum key, data;
	gchar *node;
	
	if (smsdb == NULL) return FALSE;
	if (smsdb->filepath == NULL) return FALSE;
	
	db = gdbm_open((gchar *)smsdb->filepath, 0, GDBM_WRCREAT, MMGUI_SMSDB_ACCESS_MASK, 0);
	
	if (db == NULL) return FALSE;
	
	memset(smsid, 0, sizeof(smsid));
	idlen = g_snprintf(smsid, sizeof(smsid), "%lu", idvalue);
	key.dptr = (gchar *)smsid;
	key.dsize = idlen;
	
	unreaddelta = 0;
	
	if (gdbm_exists(db, key)) {
		data = gdbm_fetch(db, key);
		if (data.dptr != NULL) {
			node = strstr(data.dptr, MMGUI_SMSDB_READ_TAG);
			if (node != NULL) {
				if ((node-data.dptr > 8) && (isdigit(node[8]))) {
					if (node[8] == '0') {
						unreaddelta = -1;
					} else {
						unreaddelta = 0;
					}
				}
			} else {
				unreaddelta = -1;
			}
			free(data.dptr);
		}
		
		if (gdbm_delete(db, key) == 0) {
			smsdb->unreadmessages += unreaddelta;
			gdbm_sync(db);
			gdbm_close(db);
			return TRUE;
		}
	}
	
	gdbm_close(db);
	
	return FALSE;
}

gboolean mmgui_smsdb_set_message_read_status(smsdb_t smsdb, gulong idvalue, gboolean readflag)
{
	GDBM_FILE db;
	gint unreaddelta;
	gchar smsid[64];
	gint idlen;
	datum key, data;
	gchar *node, /* *trailer,*/ *newmsg;
	gchar newtrailer[64];
	gint newtrailerlen;
	gboolean res;
			
	if (smsdb == NULL) return FALSE;
	if (smsdb->filepath == NULL) return FALSE;
	
	db = gdbm_open((gchar *)smsdb->filepath, 0, GDBM_WRITER, MMGUI_SMSDB_ACCESS_MASK, 0);
	
	if (db == NULL) return FALSE;
	
	memset(smsid, 0, sizeof(smsid));
	idlen = snprintf(smsid, sizeof(smsid), "%lu", idvalue);
	key.dptr = (gchar *)smsid;
	key.dsize = idlen;
	
	res = FALSE;
	
	unreaddelta = 0;
	
	if (gdbm_exists(db, key)) {
		data = gdbm_fetch(db, key);
		if (data.dptr != NULL) {
			node = strstr(data.dptr, MMGUI_SMSDB_READ_TAG);
			if (node != NULL) {
				if ((node-data.dptr > 8) && (isdigit(node[8]))) {
					if ((readflag) && (node[8] == '0')) {
						unreaddelta = -1;
						node[8] = '1';
					} else if ((!readflag) && (node[8] == '1')) {
						unreaddelta = 1;
						node[8] = '0';
					}
					
					if (gdbm_store(db, key, data, GDBM_REPLACE) == 0) {
						smsdb->unreadmessages += unreaddelta;
						res = TRUE;
					}
					
					free(data.dptr);
				}
			} else {
				if (strstr(data.dptr, MMGUI_SMSDB_TRAILER_TAG) != NULL) {
					memset(newtrailer, 0, sizeof(newtrailer));
					newtrailerlen = g_snprintf(newtrailer, sizeof(newtrailer), MMGUI_SMSDB_TRAILER_PARAMS, readflag, MMGUI_SMSDB_SMS_FOLDER_INCOMING);
					
					newmsg = g_malloc0(data.dsize-9+newtrailerlen+1);
					memcpy(newmsg, data.dptr, data.dsize-9);
					memcpy(newmsg+data.dsize-9, newtrailer, newtrailerlen);
					
					free(data.dptr);
					
					data.dptr = newmsg;
					data.dsize = data.dsize-9+newtrailerlen;
					
					if (readflag) {
						unreaddelta = -1;
					} else {
						unreaddelta = 0;
					}
					
					if (gdbm_store(db, key, data, GDBM_REPLACE) == 0) {
						smsdb->unreadmessages += unreaddelta;
						res = TRUE;
					}
					
					g_free(newmsg);
				}
			}
		}
	}
	
	gdbm_close(db);
	
	return res;
}

static mmgui_sms_message_t mmgui_smsdb_xml_parse(gchar *xml, gsize size)
{
	mmgui_sms_message_t message;
	GMarkupParser mp;
	GMarkupParseContext *mpc;
	GError *error = NULL;
	
	message = g_new(struct _mmgui_sms_message, 1);
	
	message->timestamp = time(NULL);
	message->read = FALSE;
	message->folder = MMGUI_SMSDB_SMS_FOLDER_INCOMING;
	message->binary = FALSE;
	message->number = NULL;
	message->svcnumber = NULL;
	message->idents = NULL;
	message->text = NULL;
		
	mp.start_element = mmgui_smsdb_xml_get_element;
	mp.end_element = mmgui_smsdb_xml_end_element;
	mp.text = mmgui_smsdb_xml_get_value;
	mp.passthrough = NULL;
	mp.error = NULL;
	
	mpc = g_markup_parse_context_new(&mp, 0, (gpointer)message, NULL);
	g_markup_parse_context_parse(mpc, xml, size, &error);
	if (error != NULL) {
		//g_warning(error->message);
		g_error_free(error);
		g_markup_parse_context_free(mpc);
		return NULL;
	}
	g_markup_parse_context_free(mpc);
	
	return message;
}

static void mmgui_smsdb_xml_get_element(GMarkupParseContext *context, const gchar *element, const gchar **attr_names, const gchar **attr_values, gpointer data, GError **error)
{
	if (g_str_equal(element, "number")) {
		mmgui_smsdb_xml_parameter = MMGUI_SMSDB_XML_PARAM_NUMBER;
	} else if (g_str_equal(element, "time")) {
		mmgui_smsdb_xml_parameter = MMGUI_SMSDB_XML_PARAM_TIME;
	} else if (g_str_equal(element, "binary")) {
		mmgui_smsdb_xml_parameter = MMGUI_SMSDB_XML_PARAM_BINARY;
	} else if (g_str_equal(element, "servicenumber")) {
		mmgui_smsdb_xml_parameter = MMGUI_SMSDB_XML_PARAM_SERVICENUMBER;
	} else if (g_str_equal(element, "text")) {
		mmgui_smsdb_xml_parameter = MMGUI_SMSDB_XML_PARAM_TEXT;
	} else if (g_str_equal(element, "read")) {
		mmgui_smsdb_xml_parameter = MMGUI_SMSDB_XML_PARAM_READ;
	} else if (g_str_equal(element, "folder")) {
		mmgui_smsdb_xml_parameter = MMGUI_SMSDB_XML_PARAM_FOLDER;
	} else {
		mmgui_smsdb_xml_parameter = MMGUI_SMSDB_XML_PARAM_NULL;
	}
}

static void mmgui_smsdb_xml_get_value(GMarkupParseContext *context, const gchar *text, gsize size, gpointer data, GError **error)
{
	mmgui_sms_message_t message;
	gchar *numescstr, *textescstr;
		
	message = (mmgui_sms_message_t)data;
	
	if (mmgui_smsdb_xml_parameter == MMGUI_SMSDB_XML_PARAM_NULL) return;
	
	switch (mmgui_smsdb_xml_parameter) {
		case MMGUI_SMSDB_XML_PARAM_NUMBER:
			numescstr = encoding_unescape_xml_markup(text, size);
			if (numescstr != NULL) {
				message->number = encoding_clear_special_symbols((gchar *)numescstr, strlen(numescstr));
			} else {
				message->number = encoding_clear_special_symbols((gchar *)text, size);
			}
			break;
		case MMGUI_SMSDB_XML_PARAM_TIME:
			message->timestamp = (time_t)atol(text);
			break;
		case MMGUI_SMSDB_XML_PARAM_BINARY:
			if (atoi(text)) {
				message->binary = TRUE;
			} else {
				message->binary = FALSE;
			}
			break;
		case MMGUI_SMSDB_XML_PARAM_SERVICENUMBER:
			message->svcnumber = g_strdup(text);
			break;
		case MMGUI_SMSDB_XML_PARAM_TEXT:
			textescstr = encoding_unescape_xml_markup(text, size);
			if (textescstr != NULL) {
				message->text = g_string_new(textescstr);
				g_free(textescstr);
			} else {
				message->text = g_string_new(text);
			}
			break;
		case MMGUI_SMSDB_XML_PARAM_READ:
			if (atoi(text)) {
				message->read = TRUE;
			} else {
				message->read = FALSE;
			}
			break;
		case MMGUI_SMSDB_XML_PARAM_FOLDER:
			message->folder = atoi(text);
			break;
		default:
			break;
	}
}

static void mmgui_smsdb_xml_end_element(GMarkupParseContext *context, const gchar *element, gpointer data, GError **error)
{
	if (!g_str_equal(element, "sms")) {
		mmgui_smsdb_xml_parameter = MMGUI_SMSDB_XML_PARAM_NULL;
	}
}
