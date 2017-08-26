/*
 *      historyshm.h
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

#ifndef __HISTORYSHM_H__
#define __HISTORYSHM_H__

#include <glib.h>
#include <gdbm.h>

#define MMGUI_HISTORY_SHM_DB_PATH       "/var/lib/modem-manager-gui/"
#define MMGUI_HISTORY_SHM_DB_FILE       "/var/lib/modem-manager-gui/history.db"
#define MMGUI_HISTORY_SHM_DB_PERM       0755

#define MMGUI_HISTORY_SHM_SEGMENT_NAME  "mmgui_%s"
#define MMGUI_HISTORY_SHM_SEGMENT_PERM  0777

/*Shm flags*/
enum _mmgui_history_shm_flags {
	MMGUI_HISTORY_SHM_FLAGS_NONE = 0x00,
	MMGUI_HISTORY_SHM_FLAGS_SYNC = 0x01
};

/*Shm structure*/
struct _mmgui_history_shm {
	/*Driver flags (syncronized)*/
	gint flags;
	/*Current device identifier*/
	gint identifier;
	/*Synchronization timesatamp*/
	guint64 synctime;
};

typedef struct _mmgui_history_shm *mmgui_history_shm_t;

struct _mmgui_history_shm_client {
	/*Database*/
	GDBM_FILE db;
	/*Current device*/
	gchar *drivername;
	gboolean devopened;
	/*Memory segment*/
	gint shmid;
	mmgui_history_shm_t shmaddr;
};

typedef struct _mmgui_history_shm_client *mmgui_history_shm_client_t;


mmgui_history_shm_client_t mmgui_history_client_new(void);
void mmgui_history_client_close(mmgui_history_shm_client_t client);
gboolean mmgui_history_client_open_device(mmgui_history_shm_client_t client, const gchar *devpath);
gboolean mmgui_history_client_close_device(mmgui_history_shm_client_t client);
GSList *mmgui_history_client_enum_messages(mmgui_history_shm_client_t client);
/*General purpose functions*/
guint64 mmgui_history_get_driver_from_key(const gchar *key, const gsize keylen, gchar *buf, gsize buflen);
gchar *mmgui_history_parse_driver_string(const gchar *path, gint *identifier);

#endif /* __HISTORYSHM_H__ */
