/*
 *      ayatana.c
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gmodule.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../resources.h"
#include "ayatana.h"

static void mmgui_ayatana_indicator_server_clicked_signal(MessagingMenuApp *server, guint timestamp, gpointer user_data)
{
	mmgui_ayatana_t ayatana;
	
	ayatana = (mmgui_ayatana_t)user_data;
	
	if (ayatana == NULL) return;
	
	if (ayatana->eventcb != NULL) {
		(ayatana->eventcb)(MMGUI_AYATANA_EVENT_SERVER, ayatana, NULL, ayatana->userdata);
	}
}

static void mmgui_ayatana_indicator_client_clicked_signal(MessagingMenuApp *indicator, guint timestamp, gpointer user_data)
{
	mmgui_ayatana_t ayatana;
	
	ayatana = (mmgui_ayatana_t)user_data;
	
	if (ayatana == NULL) return;
	
	if (ayatana->eventcb != NULL) {
		(ayatana->eventcb)(MMGUI_AYATANA_EVENT_CLIENT, ayatana, NULL, ayatana->userdata);
	}
}

static void mmgui_ayatana_indicator_server_show_signal(IndicateServer *arg0, guint arg1, gpointer user_data)
{
	mmgui_ayatana_t ayatana;
	
	ayatana = (mmgui_ayatana_t)user_data;
	
	if (ayatana == NULL) return;
	
	if (ayatana->eventcb != NULL) {
		(ayatana->eventcb)(MMGUI_AYATANA_EVENT_SERVER, ayatana, NULL, ayatana->userdata);
	}
}

static void mmgui_ayatana_indicator_client_show_signal(IndicateServer *arg0, guint arg1, gpointer user_data)
{
	mmgui_ayatana_t ayatana;
	
	ayatana = (mmgui_ayatana_t)user_data;
	
	if (ayatana == NULL) return;
	
	if (ayatana->eventcb != NULL) {
		(ayatana->eventcb)(MMGUI_AYATANA_EVENT_CLIENT, ayatana, NULL, ayatana->userdata);
	}
}

static gchar *mmgui_ayatana_get_desktiop_file_id(gchar *filepath)
{
	guint pathlen, sym;
	
	if (filepath == NULL) return NULL;
	
	pathlen = strlen(filepath);
	
	if (pathlen == 0) return NULL;
	
	for (sym = pathlen; sym >= 0; sym--) {
		if (filepath[sym] == '/') {
			break;
		}
	}
	
	return filepath+sym+1;
}

static gboolean mmgui_ayatana_setup_menu(mmgui_ayatana_t ayatana)
{
	if (ayatana == NULL) return FALSE;
	
	if (ayatana->library == MMGUI_AYATANA_LIB_MESSAGINGMENU) {
		/*setup server*/
		ayatana->backend.mmenu.server = (ayatana->backend.mmenu.messaging_menu_app_new)(mmgui_ayatana_get_desktiop_file_id(RESOURCE_DESKTOP_FILE));
		if (ayatana->backend.mmenu.server == NULL) {
			return FALSE;
		}
		(ayatana->backend.mmenu.messaging_menu_app_register)(ayatana->backend.mmenu.server);
		g_signal_connect(G_OBJECT(ayatana->backend.mmenu.server), "activate-source", G_CALLBACK(mmgui_ayatana_indicator_server_clicked_signal), ayatana);
		return TRUE;
	} else if (ayatana->library == MMGUI_AYATANA_LIB_INDICATE) {
		/*setup server*/
		ayatana->backend.ind.server = (ayatana->backend.ind.indicate_server_ref_default)();
		if (ayatana->backend.ind.server == NULL) {
			return FALSE;
		}
		(ayatana->backend.ind.indicate_server_set_type)(ayatana->backend.ind.server, "message.im");
		(ayatana->backend.ind.indicate_server_set_desktop_file)(ayatana->backend.ind.server, RESOURCE_DESKTOP_FILE);
		(ayatana->backend.ind.indicate_server_show)(ayatana->backend.ind.server);
		g_signal_connect(G_OBJECT(ayatana->backend.ind.server), "server-display", G_CALLBACK(mmgui_ayatana_indicator_server_show_signal), ayatana);
		/*setup client*/
		ayatana->backend.ind.client = (ayatana->backend.ind.indicate_indicator_new_with_server)(ayatana->backend.ind.server);
		if (ayatana->backend.ind.client != NULL) {
			(ayatana->backend.ind.indicate_indicator_set_property)(ayatana->backend.ind.client, "subtype", "im");
			(ayatana->backend.ind.indicate_indicator_set_property)(ayatana->backend.ind.client, "sender", _("Unread SMS"));
			(ayatana->backend.ind.indicate_indicator_set_property)(ayatana->backend.ind.client, "draw_attention", "false");
			(ayatana->backend.ind.indicate_indicator_set_property)(ayatana->backend.ind.client, "count", "");
			(ayatana->backend.ind.indicate_indicator_show)(ayatana->backend.ind.client);
			g_signal_connect(G_OBJECT(ayatana->backend.ind.client), "user-display", G_CALLBACK(mmgui_ayatana_indicator_client_show_signal), ayatana);
		}
		return TRUE;
	} else {
		return FALSE;
	}
}

static gboolean mmgui_ayatana_clean_menu(mmgui_ayatana_t ayatana)
{
	if (ayatana == NULL) return FALSE;
	
	if (ayatana->library == MMGUI_AYATANA_LIB_MESSAGINGMENU) {
		if (ayatana->backend.mmenu.server != NULL) {
			(ayatana->backend.mmenu.messaging_menu_app_remove_source)(ayatana->backend.mmenu.server, "sms");
			(ayatana->backend.mmenu.messaging_menu_app_unregister)(ayatana->backend.mmenu.server);
		}
		return TRUE;
	} else if (ayatana->library == MMGUI_AYATANA_LIB_INDICATE) {
		if (ayatana->backend.ind.server != NULL) {
			if (ayatana->backend.ind.client != NULL) {
				(ayatana->backend.ind.indicate_indicator_hide)(ayatana->backend.ind.client);
				g_object_unref(G_OBJECT(ayatana->backend.ind.client));
			}
			(ayatana->backend.ind.indicate_server_hide)(ayatana->backend.ind.server);
			g_object_unref(G_OBJECT(ayatana->backend.ind.server));
		}
		return TRUE;
	} else {
		return FALSE;
	}
}

mmgui_ayatana_t mmgui_ayatana_new(mmgui_libpaths_cache_t libcache, mmgui_ayatana_event_callback callback, gpointer userdata)
{
	mmgui_ayatana_t ayatana;
	gboolean libopened;
	
	ayatana = g_new0(struct _mmgui_ayatana, 1);
	
	/*Initialization*/
	ayatana->module = NULL;
	ayatana->library = MMGUI_AYATANA_LIB_NULL;
	ayatana->eventcb = callback;
	ayatana->userdata = userdata;
	
	/*Open module for libmessaging-menu*/
	ayatana->module = g_module_open(mmgui_libpaths_cache_get_library_full_path(libcache, "libmessaging-menu"), G_MODULE_BIND_LAZY);
	
	/*Initialize local flag*/
	libopened = FALSE;
	
	if (ayatana->module != NULL) {
		libopened = TRUE;
		libopened = libopened && g_module_symbol(ayatana->module, "messaging_menu_app_new", (gpointer *)&(ayatana->backend.mmenu.messaging_menu_app_new));
		libopened = libopened && g_module_symbol(ayatana->module, "messaging_menu_app_register", (gpointer *)&(ayatana->backend.mmenu.messaging_menu_app_register));
		libopened = libopened && g_module_symbol(ayatana->module, "messaging_menu_app_unregister", (gpointer *)&(ayatana->backend.mmenu.messaging_menu_app_unregister));
		libopened = libopened && g_module_symbol(ayatana->module, "messaging_menu_app_append_source", (gpointer *)&(ayatana->backend.mmenu.messaging_menu_app_append_source));
		libopened = libopened && g_module_symbol(ayatana->module, "messaging_menu_app_remove_source", (gpointer *)&(ayatana->backend.mmenu.messaging_menu_app_remove_source));
		libopened = libopened && g_module_symbol(ayatana->module, "messaging_menu_app_set_source_count", (gpointer *)&(ayatana->backend.mmenu.messaging_menu_app_set_source_count));
		libopened = libopened && g_module_symbol(ayatana->module, "messaging_menu_app_draw_attention", (gpointer *)&(ayatana->backend.mmenu.messaging_menu_app_draw_attention));
		libopened = libopened && g_module_symbol(ayatana->module, "messaging_menu_app_remove_attention", (gpointer *)&(ayatana->backend.mmenu.messaging_menu_app_remove_attention));
		libopened = libopened && g_module_symbol(ayatana->module, "messaging_menu_app_has_source", (gpointer *)&(ayatana->backend.mmenu.messaging_menu_app_has_source));
		/*Try to set up menu*/
		if (libopened) {
			ayatana->library = MMGUI_AYATANA_LIB_MESSAGINGMENU;
			if (!mmgui_ayatana_setup_menu(ayatana)) {
				ayatana->library = MMGUI_AYATANA_LIB_NULL;
			}
		}
		/*If some functions not exported or menu not set up, close library*/
		if ((!libopened) || (ayatana->library == MMGUI_AYATANA_LIB_NULL)) {
			ayatana->backend.mmenu.messaging_menu_app_new = NULL;
			ayatana->backend.mmenu.messaging_menu_app_register = NULL;
			ayatana->backend.mmenu.messaging_menu_app_unregister = NULL;
			ayatana->backend.mmenu.messaging_menu_app_append_source = NULL;
			ayatana->backend.mmenu.messaging_menu_app_remove_source = NULL;
			ayatana->backend.mmenu.messaging_menu_app_set_source_count = NULL;
			ayatana->backend.mmenu.messaging_menu_app_draw_attention = NULL;
			ayatana->backend.mmenu.messaging_menu_app_remove_attention = NULL;
			/*Close module*/
			g_module_close(ayatana->module);
			ayatana->module = NULL;
			ayatana->library = MMGUI_AYATANA_LIB_NULL;
		}
	}
	
	if ((ayatana->library == MMGUI_AYATANA_LIB_NULL) && (ayatana->module == NULL)) {
		/*Open module for libindicate*/
		ayatana->module = g_module_open(mmgui_libpaths_cache_get_library_full_path(libcache, "libindicate"), G_MODULE_BIND_LAZY);
		
		if (ayatana->module != NULL) {
			libopened = TRUE;
			libopened = libopened && g_module_symbol(ayatana->module, "indicate_server_ref_default", (gpointer *)&(ayatana->backend.ind.indicate_server_ref_default));
			libopened = libopened && g_module_symbol(ayatana->module, "indicate_server_set_type", (gpointer *)&(ayatana->backend.ind.indicate_server_set_type));
			libopened = libopened && g_module_symbol(ayatana->module, "indicate_server_set_desktop_file", (gpointer *)&(ayatana->backend.ind.indicate_server_set_desktop_file));
			libopened = libopened && g_module_symbol(ayatana->module, "indicate_server_show", (gpointer *)&(ayatana->backend.ind.indicate_server_show));
			libopened = libopened && g_module_symbol(ayatana->module, "indicate_server_hide", (gpointer *)&(ayatana->backend.ind.indicate_server_hide));
			libopened = libopened && g_module_symbol(ayatana->module, "indicate_indicator_new_with_server", (gpointer *)&(ayatana->backend.ind.indicate_indicator_new_with_server));
			libopened = libopened && g_module_symbol(ayatana->module, "indicate_indicator_set_property", (gpointer *)&(ayatana->backend.ind.indicate_indicator_set_property));
			libopened = libopened && g_module_symbol(ayatana->module, "indicate_indicator_show", (gpointer *)&(ayatana->backend.ind.indicate_indicator_show));
			libopened = libopened && g_module_symbol(ayatana->module, "indicate_indicator_hide", (gpointer *)&(ayatana->backend.ind.indicate_indicator_hide));
			/*Try to set up menu*/
			if (libopened) {
				ayatana->library = MMGUI_AYATANA_LIB_INDICATE;
				if (!mmgui_ayatana_setup_menu(ayatana)) {
					ayatana->library = MMGUI_AYATANA_LIB_NULL;
				}
			}
			/*If some functions not exported or menu not set up, close library*/
			if ((!libopened) || (ayatana->library == MMGUI_AYATANA_LIB_NULL)) {
				ayatana->backend.ind.indicate_server_ref_default = NULL;
				ayatana->backend.ind.indicate_server_set_type = NULL;
				ayatana->backend.ind.indicate_server_set_desktop_file = NULL;
				ayatana->backend.ind.indicate_server_show = NULL;
				ayatana->backend.ind.indicate_server_hide = NULL;
				ayatana->backend.ind.indicate_indicator_new_with_server = NULL;
				ayatana->backend.ind.indicate_indicator_set_property = NULL;
				ayatana->backend.ind.indicate_indicator_show = NULL;
				ayatana->backend.ind.indicate_indicator_hide = NULL;
				/*Close module*/
				g_module_close(ayatana->module);
				ayatana->module = NULL;
				ayatana->library = MMGUI_AYATANA_LIB_NULL;
			}
		}
	}
	
	if ((!libopened) || (ayatana->library == MMGUI_AYATANA_LIB_NULL)) {
		g_free(ayatana);
		return NULL;
	}
	
	return ayatana;
}

void mmgui_ayatana_close(mmgui_ayatana_t ayatana)
{
	if (ayatana == NULL) return;
	
	mmgui_ayatana_clean_menu(ayatana);
	
	if (ayatana->module != NULL) {
		g_module_close(ayatana->module);
	}
	
	g_free(ayatana);
}

void mmgui_ayatana_set_unread_messages_number(mmgui_ayatana_t ayatana, guint number)
{
	GFile *file;
	GIcon *icon;
	gchar numstr[32];
	
	if (ayatana == NULL) return;
	
	if (ayatana->library == MMGUI_AYATANA_LIB_MESSAGINGMENU) {
		if (ayatana->backend.mmenu.server != NULL) {
			if ((ayatana->backend.mmenu.messaging_menu_app_has_source)(ayatana->backend.mmenu.server, "sms")) {
				if (number > 0) {
					(ayatana->backend.mmenu.messaging_menu_app_set_source_count)(ayatana->backend.mmenu.server, "sms", number);
					(ayatana->backend.mmenu.messaging_menu_app_remove_attention)(ayatana->backend.mmenu.server, "sms");
				} else {
					(ayatana->backend.mmenu.messaging_menu_app_remove_source)(ayatana->backend.mmenu.server, "sms");
				}
			} else {
				if (number > 0) {
					file = g_file_new_for_path(RESOURCE_SMS_UNREAD);
					icon = g_file_icon_new(file);
					(ayatana->backend.mmenu.messaging_menu_app_append_source)(ayatana->backend.mmenu.server, "sms", icon, _("Unread messages"));
					(ayatana->backend.mmenu.messaging_menu_app_set_source_count)(ayatana->backend.mmenu.server, "sms", number);
					(ayatana->backend.mmenu.messaging_menu_app_draw_attention)(ayatana->backend.mmenu.server, "sms");
					g_signal_connect(G_OBJECT(ayatana->backend.mmenu.server), "activate-source::sms", G_CALLBACK(mmgui_ayatana_indicator_client_clicked_signal), ayatana);
					g_object_unref(icon);
					g_object_unref(file);
				}
			}
		}
	} else if (ayatana->library == MMGUI_AYATANA_LIB_INDICATE) {
		if (ayatana->backend.ind.server != NULL) {
			if (ayatana->backend.ind.client != NULL) {
				if (number > 0) {
					memset(numstr, 0, sizeof(numstr));
					snprintf(numstr, sizeof(numstr), "%u", number);
					(ayatana->backend.ind.indicate_indicator_set_property)(ayatana->backend.ind.client, "draw_attention", "true");
					(ayatana->backend.ind.indicate_indicator_set_property)(ayatana->backend.ind.client, "count", numstr);
				} else {
					(ayatana->backend.ind.indicate_indicator_set_property)(ayatana->backend.ind.client, "draw_attention", "false");
					(ayatana->backend.ind.indicate_indicator_set_property)(ayatana->backend.ind.client, "count", "");
				}
			}
		}
	}
	
}
