/*
 *      notifications.h
 *      
 *      Copyright 2013-2017 Alex <alex@linuxonly.ru>
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

#ifndef __MMGUINOTIFICATIONS_H__
#define __MMGUINOTIFICATIONS_H__

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <gmodule.h>

#include "libpaths.h"

typedef enum {
	MMGUI_NOTIFICATIONS_URGENCY_LOW,
	MMGUI_NOTIFICATIONS_URGENCY_NORMAL,
	MMGUI_NOTIFICATIONS_URGENCY_CRITICAL,

} NotificationsUrgency;

typedef void (*NotifyActionCallback)(gpointer notification, gchar *action, gpointer userdata);

/*libnotify function prototypes*/
typedef gboolean (*notify_init_func)(const gchar * app_name);
typedef GList *(*notify_get_server_caps_func)(void);
typedef gpointer (*notify_notification_new_func)(const char *summary, const char *body, const char *icon);
typedef void (*notify_notification_set_timeout_func)(gpointer notification, gint timeout);
typedef void (*notify_notification_set_hint_func)(gpointer notification, const char *key, GVariant *value);
typedef void (*notify_notification_set_image_from_pixbuf_func)(gpointer notification, gpointer pixbuf);
typedef void (*notify_notification_set_category_func)(gpointer notification, const char *category);
typedef void (*notify_notification_set_urgency_func)(gpointer notification, NotificationsUrgency urgency);
typedef void (*notify_notification_add_action_func)(gpointer notification, const gchar *action, const gchar *label, NotifyActionCallback callback, gpointer userdata, GFreeFunc freefunc);
typedef void (*notify_notification_show_func)(gpointer notification, GError **error);

enum _mmgui_notifications_sound {
	MMGUI_NOTIFICATIONS_SOUND_NONE = 0,
	MMGUI_NOTIFICATIONS_SOUND_INFO,
	MMGUI_NOTIFICATIONS_SOUND_MESSAGE
};

struct _mmgui_notifications {
	/*Modules*/
	GModule *notifymodule;
	/*libnotify functions*/
	notify_init_func notify_init;
	notify_get_server_caps_func notify_get_server_caps;
	notify_notification_new_func notify_notification_new;
	notify_notification_set_timeout_func notify_notification_set_timeout;
	notify_notification_set_hint_func notify_notification_set_hint;
	notify_notification_set_image_from_pixbuf_func notify_notification_set_image_from_pixbuf;
	notify_notification_set_category_func notify_notification_set_category;
	notify_notification_set_urgency_func notify_notification_set_urgency;
	notify_notification_add_action_func notify_notification_add_action;
	notify_notification_show_func notify_notification_show;
	/*notifications action support*/
	gboolean supportsaction;
	/*notifications icon*/
	GdkPixbuf *notifyicon;
};

typedef struct _mmgui_notifications *mmgui_notifications_t;

mmgui_notifications_t mmgui_notifications_new(mmgui_libpaths_cache_t libcache);
gboolean mmgui_notifications_show(mmgui_notifications_t notifications, gchar *caption, gchar *text, enum _mmgui_notifications_sound sound, NotifyActionCallback defcallback, gpointer userdata);
void mmgui_notifications_close(mmgui_notifications_t notifications);

#endif /* __MMGUI_NOTIFICATIONS_H__ */
