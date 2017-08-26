/*
 *      smsdb.h
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

#ifndef __SMSDB_H__
#define __SMSDB_H__

enum _mmgui_smsdb_sms_folder {
	MMGUI_SMSDB_SMS_FOLDER_INCOMING = 0,
	MMGUI_SMSDB_SMS_FOLDER_SENT,
	MMGUI_SMSDB_SMS_FOLDER_DRAFTS
};

struct _smsdb {
	const gchar *filepath;
	guint unreadmessages;
};

typedef struct _smsdb *smsdb_t;

struct _mmgui_sms_message {
	gchar *number;
	gchar *svcnumber;
	GArray *idents;
	GString *text;
	gulong dbid;
	gboolean read;
	gboolean binary;
	guint folder;
	time_t timestamp;
};

typedef struct _mmgui_sms_message *mmgui_sms_message_t;

smsdb_t mmgui_smsdb_open(const gchar *persistentid, const gchar *internalid);
gboolean mmgui_smsdb_close(smsdb_t smsdb);
/*Message functions*/
mmgui_sms_message_t mmgui_smsdb_message_create(void);
void mmgui_smsdb_message_free(mmgui_sms_message_t message);
gboolean mmgui_smsdb_message_set_number(mmgui_sms_message_t message, const gchar *number);
const gchar *mmgui_smsdb_message_get_number(mmgui_sms_message_t message);
gboolean mmgui_smsdb_message_set_service_number(mmgui_sms_message_t message, const gchar *number);
const gchar *mmgui_smsdb_message_get_service_number(mmgui_sms_message_t message);
gboolean mmgui_smsdb_message_set_text(mmgui_sms_message_t message, const gchar *text, gboolean append);
const gchar *mmgui_smsdb_message_get_text(mmgui_sms_message_t message);
gboolean mmgui_smsdb_message_set_data(mmgui_sms_message_t message, const gchar *data, gsize len, gboolean append);
gboolean mmgui_smsdb_message_set_identifier(mmgui_sms_message_t message, guint ident, gboolean append);
guint mmgui_smsdb_message_get_identifier(mmgui_sms_message_t message);
gboolean mmgui_smsdb_message_set_timestamp(mmgui_sms_message_t message, time_t timestamp);
time_t mmgui_smsdb_message_get_timestamp(mmgui_sms_message_t message);
gboolean mmgui_smsdb_message_set_read(mmgui_sms_message_t message, gboolean read);
gboolean mmgui_smsdb_message_get_read(mmgui_sms_message_t message);
gboolean mmgui_smsdb_message_set_folder(mmgui_sms_message_t message, enum _mmgui_smsdb_sms_folder folder);
enum _mmgui_smsdb_sms_folder mmgui_smsdb_message_get_folder(mmgui_sms_message_t message);
gboolean mmgui_smsdb_message_set_binary(mmgui_sms_message_t message, gboolean binary);
gboolean mmgui_smsdb_message_get_binary(mmgui_sms_message_t message);
gulong mmgui_smsdb_message_get_db_identifier(mmgui_sms_message_t message);
/*General functions*/
guint mmgui_smsdb_get_unread_messages(smsdb_t smsdb);
gboolean mmgui_smsdb_add_sms(smsdb_t smsdb, mmgui_sms_message_t message);
GSList *mmgui_smsdb_read_sms_list(smsdb_t smsdb);
void mmgui_smsdb_message_free_list(GSList *smslist);
mmgui_sms_message_t mmgui_smsdb_read_sms_message(smsdb_t smsdb, gulong idvalue);
gboolean mmgui_smsdb_remove_sms_message(smsdb_t smsdb, gulong idvalue);
gboolean mmgui_smsdb_set_message_read_status(smsdb_t smsdb, gulong idvalue, gboolean readflag);

#endif /* __SMSDB_H__ */
