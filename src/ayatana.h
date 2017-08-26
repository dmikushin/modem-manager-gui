/*
 *      ayatana.h
 *      
 *      Copyright 2013 Alex <alex@linuxonly.ru>
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

#ifndef __AYATANA_H__
#define __AYATANA_H__

#include <glib.h>
#include <gio/gio.h>

#include "libpaths.h"

enum _mmgui_aytana_library {
	MMGUI_AYATANA_LIB_NULL = 0,
	MMGUI_AYATANA_LIB_INDICATE,
	MMGUI_AYATANA_LIB_MESSAGINGMENU
};

enum _mmgui_ayatana_event {
	MMGUI_AYATANA_EVENT_SERVER = 0,
	MMGUI_AYATANA_EVENT_CLIENT
};

/*libindiacate staructures*/
typedef struct _IndicateServer IndicateServer;
typedef struct _IndicateIndicator IndicateIndicator;
/*libmessaging-menu structures*/
typedef struct _MessagingMenuApp MessagingMenuApp;

/*libindicate functions*/
typedef IndicateServer *(*indicate_server_ref_default_func)(void);
typedef void (*indicate_server_set_type_func)(IndicateServer *server, const gchar *type);
typedef void (*indicate_server_set_desktop_file_func)(IndicateServer *server, const gchar *path);
typedef void (*indicate_server_show_func)(IndicateServer *server);
typedef void (*indicate_server_hide_func)(IndicateServer *server);
typedef IndicateIndicator *(*indicate_indicator_new_with_server_func)(IndicateServer *server);
typedef void (*indicate_indicator_set_property_func)(IndicateIndicator *indicator, const gchar *key, const gchar *data);
typedef void (*indicate_indicator_show_func)(IndicateIndicator *indicator);
typedef void (*indicate_indicator_hide_func)(IndicateIndicator *indicator);

/*libmessaging-menu functions*/
typedef MessagingMenuApp *(*messaging_menu_app_new_func)(const gchar *desktop_id);
typedef void (*messaging_menu_app_register_func)(MessagingMenuApp *app);
typedef void (*messaging_menu_app_unregister_func)(MessagingMenuApp *app);
typedef void (*messaging_menu_app_append_source_func)(MessagingMenuApp *app, const gchar *id, GIcon *icon, const gchar *label);
typedef void (*messaging_menu_app_remove_source_func)(MessagingMenuApp *app, const gchar *source_id);
typedef void (*messaging_menu_app_set_source_count_func)(MessagingMenuApp *app, const gchar *source_id, guint count);
typedef void (*messaging_menu_app_draw_attention_func)(MessagingMenuApp *app, const gchar *source_id);
typedef void (*messaging_menu_app_remove_attention_func)(MessagingMenuApp *app, const gchar *source_id);
typedef gboolean (*messaging_menu_app_has_source_func)(MessagingMenuApp *app, const gchar *source_id);


struct _mmgui_ayatana_libindicate {
	IndicateServer *server;
	IndicateIndicator *client;
	indicate_server_ref_default_func indicate_server_ref_default;
	indicate_server_set_type_func indicate_server_set_type;
	indicate_server_set_desktop_file_func indicate_server_set_desktop_file;
	indicate_server_show_func indicate_server_show;
	indicate_server_hide_func indicate_server_hide;
	indicate_indicator_new_with_server_func indicate_indicator_new_with_server;
	indicate_indicator_set_property_func indicate_indicator_set_property;
	indicate_indicator_show_func indicate_indicator_show;
	indicate_indicator_hide_func indicate_indicator_hide;
};

struct _mmgui_ayatana_libmessagingmenu {
	MessagingMenuApp *server;
	messaging_menu_app_new_func messaging_menu_app_new;
	messaging_menu_app_register_func messaging_menu_app_register;
	messaging_menu_app_unregister_func messaging_menu_app_unregister;
	messaging_menu_app_append_source_func messaging_menu_app_append_source;
	messaging_menu_app_remove_source_func messaging_menu_app_remove_source;
	messaging_menu_app_set_source_count_func messaging_menu_app_set_source_count;
	messaging_menu_app_draw_attention_func messaging_menu_app_draw_attention;
	messaging_menu_app_remove_attention_func messaging_menu_app_remove_attention;
	messaging_menu_app_has_source_func messaging_menu_app_has_source;
};

/*Ayatana event callback*/
typedef void (*mmgui_ayatana_event_callback)(enum _mmgui_ayatana_event event, gpointer ayatana, gpointer data, gpointer userdata);

struct _mmgui_ayatana {
	/*Module*/
	GModule *module;
	/*Used library*/
	enum _mmgui_aytana_library library;
	/*Event callback*/
	mmgui_ayatana_event_callback eventcb;
	gpointer userdata;
	/*Functions union*/
	union {
		struct _mmgui_ayatana_libindicate ind;
		struct _mmgui_ayatana_libmessagingmenu mmenu;
	} backend;
};

typedef struct _mmgui_ayatana *mmgui_ayatana_t;


mmgui_ayatana_t mmgui_ayatana_new(mmgui_libpaths_cache_t libcache, mmgui_ayatana_event_callback callback, gpointer userdata);
void mmgui_ayatana_close(mmgui_ayatana_t ayatana);
void mmgui_ayatana_set_unread_messages_number(mmgui_ayatana_t ayatana, guint number);

#endif /* __AYATANA_H__ */
