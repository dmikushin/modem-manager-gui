/*
 *      providersdb.h
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

#ifndef __PROVIDERSDB_H__
#define __PROVIDERSDB_H__

#include <glib.h>

struct _mmgui_providers_db_entry {
	gchar country[3];
	gchar *name;
	gchar *apn;
	GArray *id;
	guint tech;
	gchar *username;
	gchar *password;
	gchar *dns1;
	gchar *dns2;
};

typedef struct _mmgui_providers_db_entry *mmgui_providers_db_entry_t;

struct _mmgui_providers_db {
	GMappedFile *file;
	guint curparam;
	gchar curcountry[3];
	gchar *curname;
	gboolean gotname;
	GArray *curid;
	mmgui_providers_db_entry_t curentry;
	GSList *providers;
};

typedef struct _mmgui_providers_db *mmgui_providers_db_t;

mmgui_providers_db_t mmgui_providers_db_create(void);
void mmgui_providers_db_close(mmgui_providers_db_t db);


#endif /* __PROVIDERSDB_H__ */
