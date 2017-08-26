/*
 *      svcmanager.h
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

#ifndef __SVCMANAGER_H__
#define __SVCMANAGER_H__

#include <glib.h>
#include <gio/gio.h>

#include "polkit.h"

/*Events*/
enum _mmgui_svcmanser_event {
	MMGUI_SVCMANGER_EVENT_STARTED          = 0,
	MMGUI_SVCMANGER_EVENT_ENTITY_ACTIVATED = 1,
	MMGUI_SVCMANGER_EVENT_ENTITY_CHANGED   = 2,
	MMGUI_SVCMANGER_EVENT_ENTITY_ERROR     = 3,
	MMGUI_SVCMANGER_EVENT_FINISHED         = 4,
	MMGUI_SVCMANGER_EVENT_AUTH_ERROR       = 5,
	MMGUI_SVCMANGER_EVENT_OTHER_ERROR      = 6
};

/*Event callback*/
typedef void (*mmgui_svcmanager_event_callback)(gpointer svcmanager, gint event, gpointer subject, gpointer userdata);

/*Activation technologies*/
enum _mmgui_svcmanager_activation_tech {
	MMGUI_SVCMANGER_ACTIVATION_TECH_NA = 0,
	MMGUI_SVCMANGER_ACTIVATION_TECH_SYSTEMD,
	MMGUI_SVCMANGER_ACTIVATION_TECH_DBUS
};

/*Entity types*/
enum _mmgui_svcmanser_entity_type {
	MMGUI_SVCMANGER_TYPE_SERVICE    = 0,
	MMGUI_SVCMANGER_TYPE_INTERFACE  = 1
};

/*DBus interface*/
struct _mmgui_svcmanager_interface {
	gchar *name;
	gboolean active;
	gboolean activatable;
};

typedef struct _mmgui_svcmanager_interface *mmgui_svcmanager_interface_t;

/*Systemd service*/
struct _mmgui_svcmanager_service {
	gchar *name;
	gboolean loaded;
	gboolean active;
	gboolean running;
	gboolean enabled;
};

typedef struct _mmgui_svcmanager_service *mmgui_svcmanager_service_t;

/*State transition*/
struct _mmgui_svcmanager_transition {
	gint type;
	gchar *modname;
	gchar *job;
	guint timer;
	gboolean enable;
	union {
		mmgui_svcmanager_service_t service;
		mmgui_svcmanager_interface_t interface;
	} entity; 
};

typedef struct _mmgui_svcmanager_transition *mmgui_svcmanager_transition_t;

struct _mmgui_svcmanager {
	/*Service activation interface*/
	GDBusConnection *connection;
	/*Systemd management*/
	gboolean systemdtech;
	GDBusProxy *managerproxy;
	GHashTable *services;
	/*Standard DBus activation*/
	gboolean dbustech;
	GDBusProxy *dbusproxy;
	GHashTable *interfaces;
	/*Polkit interface*/
	mmgui_polkit_t polkit;
	/*Transitions queue*/
	GQueue *transqueue;
	gboolean svcsinqueue;
	gboolean intsinqueue;
	/*User data and callback*/
	gpointer userdata;
	mmgui_svcmanager_event_callback callback;
	/*Last error message*/
	gchar *errormsg;
};

typedef struct _mmgui_svcmanager *mmgui_svcmanager_t;

gchar *mmgui_svcmanager_get_last_error(mmgui_svcmanager_t svcmanager);
mmgui_svcmanager_t mmgui_svcmanager_open(mmgui_polkit_t polkit, mmgui_svcmanager_event_callback callback, gpointer userdata);
void mmgui_svcmanager_close(mmgui_svcmanager_t svcmanager);
gboolean mmgui_svcmanager_get_service_state(mmgui_svcmanager_t svcmanager, gchar *svcname, gchar *svcint);
gint mmgui_svcmanager_get_service_activation_tech(mmgui_svcmanager_t svcmanager, gchar *svcname, gchar *svcint);
gboolean mmgui_svcmanager_schedule_start_service(mmgui_svcmanager_t svcmanager, gchar *svcname, gchar *svcint, gchar *modname, gboolean enable);
gboolean mmgui_svcmanager_start_services_activation(mmgui_svcmanager_t svcmanager);
gchar *mmgui_svcmanager_get_transition_module_name(mmgui_svcmanager_transition_t transition);

#endif /* __SVCMANAGER_H__ */
