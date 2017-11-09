/*
 *      notifications.c
 *      
 *      Copyright 2013-2014 Alex <alex@linuxonly.ru>
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
#include <stdio.h>
#include <glib.h>
#include <gmodule.h>
#include <gtk/gtk.h>

#include "notifications.h"
#include "../resources.h"


mmgui_notifications_t mmgui_notifications_new(mmgui_libpaths_cache_t libcache)
{
	mmgui_notifications_t notifications;
	GError *error;
	gboolean libopened;
	GtkSettings *gtksettings;
	gchar *gtksoundtheme;
	GList *capabilities, *iterator;
		
	notifications = g_new0(struct _mmgui_notifications, 1);
	
	//libnotify
	notifications->notifymodule = NULL;
	
	//Open module
	notifications->notifymodule = g_module_open(mmgui_libpaths_cache_get_library_full_path(libcache, "libnotify"), G_MODULE_BIND_LAZY);
	
	if (notifications->notifymodule != NULL) {
		libopened = TRUE;
		libopened = libopened && g_module_symbol(notifications->notifymodule, "notify_init", (gpointer *)&(notifications->notify_init));
		libopened = libopened && g_module_symbol(notifications->notifymodule, "notify_get_server_caps", (gpointer *)&(notifications->notify_get_server_caps));
		libopened = libopened && g_module_symbol(notifications->notifymodule, "notify_notification_new", (gpointer *)&(notifications->notify_notification_new));
		libopened = libopened && g_module_symbol(notifications->notifymodule, "notify_notification_set_timeout", (gpointer *)&(notifications->notify_notification_set_timeout));
		libopened = libopened && g_module_symbol(notifications->notifymodule, "notify_notification_set_hint", (gpointer *)&(notifications->notify_notification_set_hint));
		libopened = libopened && g_module_symbol(notifications->notifymodule, "notify_notification_set_image_from_pixbuf", (gpointer *)&(notifications->notify_notification_set_image_from_pixbuf));
		libopened = libopened && g_module_symbol(notifications->notifymodule, "notify_notification_set_category", (gpointer *)&(notifications->notify_notification_set_category));
		libopened = libopened && g_module_symbol(notifications->notifymodule, "notify_notification_set_urgency", (gpointer *)&(notifications->notify_notification_set_urgency));
		libopened = libopened && g_module_symbol(notifications->notifymodule, "notify_notification_add_action", (gpointer *)&(notifications->notify_notification_add_action));
		libopened = libopened && g_module_symbol(notifications->notifymodule, "notify_notification_show", (gpointer *)&(notifications->notify_notification_show));
		//If some functions not exported, close library
		if (!libopened) {
			notifications->notify_init = NULL;
			notifications->notify_get_server_caps = NULL;
			notifications->notify_notification_new = NULL;
			notifications->notify_notification_set_timeout = NULL;
			notifications->notify_notification_set_hint = NULL;
			notifications->notify_notification_set_image_from_pixbuf = NULL;
			notifications->notify_notification_set_category = NULL;
			notifications->notify_notification_set_urgency = NULL;
			notifications->notify_notification_add_action = NULL;
			notifications->notify_notification_show = NULL;
			//Close module
			g_module_close(notifications->notifymodule);
			notifications->notifymodule = NULL;
		} else {
			/*Initialize libnotify*/
			(notifications->notify_init)("Modem Manager GUI");
			/*Handle capabilities*/
			notifications->supportsaction = FALSE;
			capabilities = (notifications->notify_get_server_caps)();
			if (capabilities != NULL) {
				for(iterator=capabilities; iterator!=NULL; iterator=iterator->next) {
					if (g_str_equal((gchar *)iterator->data, "actions")) {
						notifications->supportsaction = TRUE;					
						break;
					}					
					g_list_foreach(capabilities, (GFunc)g_free, NULL);
					g_list_free(capabilities);
				}
			}
			/*Load icon for notifications*/
			error = NULL;
			notifications->notifyicon = gdk_pixbuf_new_from_file(RESOURCE_MAINWINDOW_ICON, &error);
			if ((notifications->notifyicon == NULL) && (error != NULL)) {
				g_debug("Error loadig application icon: %s\n", error->message);
				g_error_free(error);
			}
		}
	}
	
	//libcanberra
	notifications->canberramodule = NULL;
	notifications->cacontext = NULL;
	
	//Open module
	notifications->canberramodule = g_module_open(mmgui_libpaths_cache_get_library_full_path(libcache, "libcanberra"), G_MODULE_BIND_LAZY);
	
	if (notifications->canberramodule != NULL) {
		libopened = TRUE;
		libopened = libopened && g_module_symbol(notifications->canberramodule, "ca_context_create", (gpointer *)&(notifications->ca_context_create));
		libopened = libopened && g_module_symbol(notifications->canberramodule, "ca_context_destroy", (gpointer *)&(notifications->ca_context_destroy));
		libopened = libopened && g_module_symbol(notifications->canberramodule, "ca_context_play", (gpointer *)&(notifications->ca_context_play));
		libopened = libopened && g_module_symbol(notifications->canberramodule, "ca_context_change_props", (gpointer *)&(notifications->ca_context_change_props));
		libopened = libopened && g_module_symbol(notifications->canberramodule, "ca_proplist_create", (gpointer *)&(notifications->ca_proplist_create));
		libopened = libopened && g_module_symbol(notifications->canberramodule, "ca_proplist_destroy", (gpointer *)&(notifications->ca_proplist_destroy));
		libopened = libopened && g_module_symbol(notifications->canberramodule, "ca_proplist_sets", (gpointer *)&(notifications->ca_proplist_sets));
		libopened = libopened && g_module_symbol(notifications->canberramodule, "ca_context_play_full", (gpointer *)&(notifications->ca_context_play_full));
		//If some functions not exported, close library
		if (!libopened) {
			notifications->ca_context_create = NULL;
			notifications->ca_context_destroy = NULL;
			notifications->ca_context_play = NULL;
			notifications->ca_context_change_props = NULL;
			notifications->ca_proplist_create = NULL;
			notifications->ca_proplist_destroy = NULL;
			notifications->ca_proplist_sets = NULL;
			notifications->ca_context_play_full = NULL;
			//Close module
			g_module_close(notifications->canberramodule);
			notifications->canberramodule = NULL;
			notifications->cacontext = NULL;
		} else {
			//Initialize libnotify
			(notifications->ca_context_create)(&(notifications->cacontext));
			gtksettings = gtk_settings_get_default();
			gtksoundtheme = NULL;
			g_object_get(gtksettings, "gtk-sound-theme-name", &gtksoundtheme, NULL);
			if (gtksoundtheme != NULL) {
				(notifications->ca_context_change_props)(notifications->cacontext, "canberra.xdg-theme.name", gtksoundtheme, NULL);
				g_free(gtksoundtheme);
			}
		}
	}
	
	return notifications;
}

gboolean mmgui_notifications_show(mmgui_notifications_t notifications, gchar *caption, gchar *text, enum _mmgui_notifications_sound sound, NotifyActionCallback defcallback, gpointer userdata)
{
	gpointer notification;
	gint caresult;
	ca_proplist *caproplist;
		
	if (notifications == NULL) return FALSE;
	
	if ((caption != NULL) && (text != NULL) && (notifications->notifymodule != NULL)) {
		notification = (notifications->notify_notification_new)(caption, text, NULL);
		if (notification != NULL) {
			(notifications->notify_notification_set_timeout)(notification, 3000);
			(notifications->notify_notification_set_image_from_pixbuf)(notification, notifications->notifyicon);
			if ((notifications->supportsaction) && (defcallback != NULL)) {
				(notifications->notify_notification_add_action)(notification, "default", "Default action", (NotifyActionCallback)defcallback, userdata, NULL);
			}
			(notifications->notify_notification_show)(notification, NULL);
		}
	}
	
	if ((sound != MMGUI_NOTIFICATIONS_SOUND_NONE) && (notifications->canberramodule != NULL) && (notifications->cacontext != NULL)) {
		(notifications->ca_proplist_create)(&caproplist);
		switch (sound) {
			case MMGUI_NOTIFICATIONS_SOUND_MESSAGE:
				(notifications->ca_proplist_sets)(caproplist, "media.filename", RESOURCE_SOUND_MESSAGE);
				(notifications->ca_proplist_sets)(caproplist, "media.role", "event");
				break;
			case MMGUI_NOTIFICATIONS_SOUND_INFO:
				(notifications->ca_proplist_sets)(caproplist, "event.id", "dialog-info");
				(notifications->ca_proplist_sets)(caproplist, "media.role", "event");
				break;
			case MMGUI_NOTIFICATIONS_SOUND_NONE:
				break;
			default:
				(notifications->ca_proplist_sets)(caproplist, "event.id", "dialog-error");
				(notifications->ca_proplist_sets)(caproplist, "media.role", "event");
				break;
		}
		caresult = (notifications->ca_context_play_full)(notifications->cacontext, 0, caproplist, NULL, NULL);
		if (caresult != 0) {
			g_debug("Failed to play sound using libcanberra\n");
		}
		(notifications->ca_proplist_destroy)(caproplist);
	}
	
	return TRUE;
}

void mmgui_notifications_close(mmgui_notifications_t notifications)
{
	if (notifications == NULL) return;
	
	if (notifications->notifymodule != NULL) {
		//First close context
		if (notifications->cacontext != NULL) {
			(notifications->ca_context_destroy)(notifications->cacontext);
		}
		//Then unload module
		g_module_close(notifications->notifymodule);
		notifications->notifymodule = NULL;
		/*And free icon*/
		if (notifications->notifyicon != NULL) {
			g_object_unref(notifications->notifyicon);
		}
	}
	
	if (notifications->canberramodule != NULL) {
		//Only module unload needed
		g_module_close(notifications->canberramodule);
		notifications->canberramodule = NULL;
	}
	
	g_free(notifications);
}
