/*
 *      polkit.h
 *      
 *      Copyright 2015 Alex <alex@linuxonly.ru>
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

#ifndef __POLKIT_H__
#define __POLKIT_H__

#include <glib.h>
#include <gio/gio.h>

enum _mmgui_polkit_auth {
	MMGUI_POLKIT_NOT_AUTHORIZED                                 = 0,
	MMGUI_POLKIT_AUTHENTICATION_REQUIRED                        = 1,
	MMGUI_POLKIT_ADMINISTRATOR_AUTHENTICATION_REQUIRED          = 2,
	MMGUI_POLKIT_AUTHENTICATION_REQUIRED_RETAINED               = 3,
	MMGUI_POLKIT_ADMINISTRATOR_AUTHENTICATION_REQUIRED_RETAINED = 4,
	MMGUI_POLKIT_AUTHORIZED                                     = 5
};

struct _mmgui_polkit_action {
	enum _mmgui_polkit_auth anyauth;
	enum _mmgui_polkit_auth inactiveauth;
	enum _mmgui_polkit_auth activeauth;
	gchar **implyactions;
};

typedef struct _mmgui_polkit_action *mmgui_polkit_action_t;

struct _mmgui_polkit {
	/*Polkit authentication interface*/
	GDBusConnection *connection;
	GDBusProxy *proxy;
	guint64 starttime;
	GHashTable *actions;
};

typedef struct _mmgui_polkit *mmgui_polkit_t;

mmgui_polkit_t mmgui_polkit_open(void);
void mmgui_polkit_close(mmgui_polkit_t polkit);
gboolean mmgui_polkit_action_needed(mmgui_polkit_t polkit, const gchar *actionname, gboolean strict);
gboolean mmgui_polkit_request_password(mmgui_polkit_t polkit, const gchar *actionname);
gboolean mmgui_polkit_revoke_authorization(mmgui_polkit_t polkit, const gchar *actionname);

#endif /* __POLKIT_H__ */
