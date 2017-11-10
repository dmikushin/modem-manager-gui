/*
 *      svcmanager.c
 *
 *      Copyright 2015-2017 Alex <alex@linuxonly.ru>
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
#include <glib/gprintf.h>
#include <gio/gio.h>
#include <stdlib.h>

#include "svcmanager.h"
#include "polkit.h"

/*DBus constants*/
#define MMGUI_DBUS_START_REPLY_SUCCESS          1
#define MMGUI_DBUS_START_REPLY_ALREADY_RUNNING  2
/*Timeouts in ms*/
#define MMGUI_SVCMANAGER_ACTIVATION_TIMEOUT     15000

static void mmgui_svcmanager_set_last_error(mmgui_svcmanager_t svcmanager, gchar *error);
static void mmgui_svcmanager_service_destroy_key(gpointer data);
static void mmgui_svcmanager_service_destroy_value(gpointer data);
static void mmgui_svcmanager_interface_destroy_key(gpointer data);
static void mmgui_svcmanager_interface_destroy_value(gpointer data);
static void mmgui_transition_destroy(mmgui_svcmanager_transition_t transition);
static void mmgui_transition_queue_destroy(GQueue *transqueue);
static gboolean mmgui_svcmanager_open_systemd_manager_interface(mmgui_svcmanager_t svcmanager);
static gboolean mmgui_svcmanager_open_dbus_activation_interface(mmgui_svcmanager_t svcmanager);
static gboolean mmgui_svcmanager_services_enable(mmgui_svcmanager_t svcmanager, mmgui_svcmanager_transition_t transition);
static gboolean mmgui_svcmanager_services_activation_service_timeout(gpointer userdata);
static void mmgui_svcmanager_services_activation_service_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer userdata);
static void mmgui_svcmanager_services_activation_interface_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer userdata);
static gboolean mmgui_svcmanager_services_activation_chain(mmgui_svcmanager_t svcmanager);


gchar *mmgui_svcmanager_get_last_error(mmgui_svcmanager_t svcmanager)
{
	return svcmanager->errormsg;
}

static void mmgui_svcmanager_set_last_error(mmgui_svcmanager_t svcmanager, gchar *error)
{
	if ((svcmanager == NULL) || (error == NULL)) return;
	
	if (svcmanager->errormsg != NULL) {
		g_free(svcmanager->errormsg);
	}
	
	svcmanager->errormsg = g_strdup(error);	
}

static void mmgui_svcmanager_service_destroy_key(gpointer data)
{
	gchar *key;

	if (data == NULL) return;

	key = (gchar *)data;

	g_free(key);
}

static void mmgui_svcmanager_service_destroy_value(gpointer data)
{
	mmgui_svcmanager_service_t service;

	if (data == NULL) return;

	service = (mmgui_svcmanager_service_t)data;
	
	g_free(service);
}

static void mmgui_svcmanager_interface_destroy_key(gpointer data)
{
	gchar *key;
	
	if (data == NULL) return;
	
	key = (gchar *)data;
	
	g_free(key);
}

static void mmgui_svcmanager_interface_destroy_value(gpointer data)
{
	mmgui_svcmanager_interface_t interface;
	
	if (data == NULL) return;
	
	interface = (mmgui_svcmanager_interface_t)data;
	
	g_free(interface);
}

static void mmgui_transition_destroy(mmgui_svcmanager_transition_t transition)
{
	if (transition == NULL) return;
	
	if (transition->modname != NULL) {
		g_free(transition->modname);
	}
		
	if (transition->job != NULL) {
		g_free(transition->job);
	}
	
	g_free(transition);
}

static void mmgui_transition_queue_destroy(GQueue *transqueue)
{
	mmgui_svcmanager_transition_t transition;
	
	if (transqueue == NULL) return;
	
	do {
		/*Remove first transition object*/
		transition = g_queue_pop_head(transqueue);
		
		if (transition != NULL) {
			/*Free transistion object*/
			if (transition->modname != NULL) {
				g_free(transition->modname);
			}
						
			if (transition->job != NULL) {
				g_free(transition->job);
			}
			
			g_free(transition);
		}
	} while (transition != NULL);
	
	/*Free queue*/
	g_queue_free(transqueue);
}

static gboolean mmgui_svcmanager_open_systemd_manager_interface(mmgui_svcmanager_t svcmanager)
{
	GError *error;
	GVariant *svclistanswer;
	GVariant *svclistv;
	GVariantIter svciter;
	gchar *svcname, *svcloadedstr, *svcactivestr, *svcrunningstr, *svcunitname, *svcenabledstr;
	mmgui_svcmanager_service_t service;

	if (svcmanager == NULL) return FALSE;
	
	/*Test if systemd can be used*/
	if (!mmgui_polkit_action_needed(svcmanager->polkit, "ru.linuxonly.modem-manager-gui.manage-services", TRUE)) {
		g_debug("Systemd doesnt work with polkit\n");
		return FALSE;
	}

	/*Create systemd proxy object*/
    error = NULL;
    svcmanager->managerproxy = g_dbus_proxy_new_sync(svcmanager->connection,
													G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
													NULL,
													"org.freedesktop.systemd1",
													"/org/freedesktop/systemd1",
													"org.freedesktop.systemd1.Manager",
													NULL,
													&error);
	
	if ((svcmanager->managerproxy == NULL) && (error != NULL)) {
		g_debug("Unable create proxy object: %s\n", error->message);
		g_error_free(error);
		return FALSE;
	}
	/*Request systemd service list*/
	error = NULL;
	svclistanswer = g_dbus_proxy_call_sync(svcmanager->managerproxy,
											"ListUnits",
											NULL,
											G_DBUS_CALL_FLAGS_NONE,
											-1,
											NULL,
											&error);

	if ((svclistanswer == NULL) && (error != NULL)) {
		g_debug("Unable to get Systemd services list: %s\n", error->message);
		g_error_free(error);
		g_object_unref(svcmanager->managerproxy);
		return FALSE;
	}
	/*Services hash table*/
	svcmanager->services = g_hash_table_new_full(g_str_hash, g_str_equal, mmgui_svcmanager_service_destroy_key, mmgui_svcmanager_service_destroy_value);
	/*Enumerate services*/
	if (g_variant_n_children(svclistanswer) > 0) {
		svclistv = g_variant_get_child_value(svclistanswer, 0);
		g_variant_iter_init(&svciter, svclistv);
		while (g_variant_iter_loop(&svciter, "(ssssssouso)", &svcname, NULL, &svcloadedstr, &svcactivestr, &svcrunningstr, NULL, NULL, NULL, NULL, NULL)) {
			/*Only services*/
			if (g_strrstr(svcname, ".service") != NULL) {
				service = g_new0(struct _mmgui_svcmanager_service, 1);
				service->name = g_strdup(svcname);
				service->loaded = g_str_equal(svcloadedstr, "loaded");
				service->active = g_str_equal(svcactivestr, "active");
				service->running = g_str_equal(svcrunningstr, "running");
				service->enabled = FALSE;
				g_hash_table_insert(svcmanager->services, service->name, service);
				g_debug("New Systemd service: %s (%u:%u:%u)\n", service->name, service->loaded, service->active, service->running);
			}
		}
	}
	g_variant_unref(svclistanswer);
	
	/*Request systemd unit files list*/
	error = NULL;
	svclistanswer = g_dbus_proxy_call_sync(svcmanager->managerproxy,
											"ListUnitFiles",
											NULL,
											G_DBUS_CALL_FLAGS_NONE,
											-1,
											NULL,
											&error);
	
	if ((svclistanswer == NULL) && (error != NULL)) {
		g_debug("Unable to get Systemd unit files list: %s\n", error->message);
		g_error_free(error);
		g_object_unref(svcmanager->managerproxy);
		return FALSE;
	}
	/*Enumerate units*/
	if (g_variant_n_children(svclistanswer) > 0) {
		svclistv = g_variant_get_child_value(svclistanswer, 0);
		g_variant_iter_init(&svciter, svclistv);
		while (g_variant_iter_loop(&svciter, "(ss)", &svcunitname, &svcenabledstr, NULL)) {
			/*Only services*/
			if (g_strrstr(svcunitname, ".service") != NULL) {
				svcname = strrchr(svcunitname, '/');
				if (svcname != NULL) {
					service = g_hash_table_lookup(svcmanager->services, svcname + 1);
					if (service == NULL) {
						service = g_new0(struct _mmgui_svcmanager_service, 1);
						service->name = g_strdup(svcname + 1);
						service->loaded = FALSE;
						service->active = FALSE;
						service->running = FALSE;
						service->enabled = g_str_equal(svcenabledstr, "enabled");
						g_hash_table_insert(svcmanager->services, service->name, service);
						g_debug("New Systemd service \'%s\' enabled: %u\n", svcname + 1, service->enabled);
					} else {
						service->enabled = g_str_equal(svcenabledstr, "enabled");
						g_debug("Known Systemd service \'%s\' enabled: %u\n", svcname + 1, service->enabled);
					} 
				}
			}
		}
	}
	g_variant_unref(svclistanswer);

	return TRUE;
}

static gboolean mmgui_svcmanager_open_dbus_activation_interface(mmgui_svcmanager_t svcmanager)
{
	GError *error;
	GVariant *intlistanswer;
	GVariant *intlistv;
	GVariantIter intiter;
	gchar *intname;
	mmgui_svcmanager_interface_t interface;
	
	if (svcmanager == NULL) return FALSE;
	
	/*Create DBus proxy object*/
    error = NULL;
    svcmanager->dbusproxy = g_dbus_proxy_new_sync(svcmanager->connection,
													G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
													NULL,
													"org.freedesktop.DBus",
													"/org/freedesktop/DBus",
													"org.freedesktop.DBus",
													NULL,
													&error);
	
	if ((svcmanager->dbusproxy == NULL) && (error != NULL)) {
		g_debug("Unable create proxy object: %s\n", error->message);
		g_error_free(error);
		return FALSE;
	}
	/*Request activatable interfaces list*/
	error = NULL;
	intlistanswer = g_dbus_proxy_call_sync(svcmanager->dbusproxy,
											"ListActivatableNames",
											NULL,
											G_DBUS_CALL_FLAGS_NONE,
											-1,
											NULL,
											&error);

	if ((intlistanswer == NULL) && (error != NULL)) {
		g_debug("Unable to get activatable Dbus interfaces list: %s\n", error->message);
		g_error_free(error);
		g_object_unref(svcmanager->dbusproxy);
		return FALSE;
	}
	/*Interfaces hash table*/
	svcmanager->interfaces = g_hash_table_new_full(g_str_hash, g_str_equal, mmgui_svcmanager_interface_destroy_key, mmgui_svcmanager_interface_destroy_value);
	/*Enumerate interfaces*/
	if (g_variant_n_children(intlistanswer) > 0) {
		intlistv = g_variant_get_child_value(intlistanswer, 0);
		g_variant_iter_init(&intiter, intlistv);
		while (g_variant_iter_loop(&intiter, "s", &intname, NULL)) {
			/*Interfaces that can be activated*/
			interface = g_new0(struct _mmgui_svcmanager_interface, 1);
			interface->name = g_strdup(intname);
			interface->active = FALSE;
			interface->activatable = TRUE;
			g_hash_table_insert(svcmanager->interfaces, interface->name, interface);
			g_debug("New activatable DBus interface: %s\n", interface->name);
		}
	}
	g_variant_unref(intlistanswer);
	
	/*Request active interfaces list*/
	error = NULL;
	intlistanswer = g_dbus_proxy_call_sync(svcmanager->dbusproxy,
											"ListNames",
											NULL,
											G_DBUS_CALL_FLAGS_NONE,
											-1,
											NULL,
											&error);

	if ((intlistanswer == NULL) && (error != NULL)) {
		g_debug("Unable to get active Dbus interfaces list: %s\n", error->message);
		g_error_free(error);
		g_object_unref(svcmanager->dbusproxy);
		g_hash_table_destroy(svcmanager->interfaces);
		return FALSE;
	}
	/*Enumerate active interfaces*/
	if (g_variant_n_children(intlistanswer) > 0) {
		intlistv = g_variant_get_child_value(intlistanswer, 0);
		g_variant_iter_init(&intiter, intlistv);
		while (g_variant_iter_loop(&intiter, "s", &intname, NULL)) {
			/*Interfaces that already activated*/
			if (intname[0] != ':') {
				interface = g_hash_table_lookup(svcmanager->interfaces, intname);
				if (interface == NULL) {
					interface = g_new0(struct _mmgui_svcmanager_interface, 1);
					interface->name = g_strdup(intname);
					interface->active = TRUE;
					interface->activatable = FALSE;
					g_hash_table_insert(svcmanager->interfaces, interface->name, interface);
					g_debug("New active DBus interface: %s\n", interface->name);
				} else {
					interface->active = TRUE;
					g_debug("Known active DBus interface: %s\n", interface->name);
				}
			}
		}
	}
	g_variant_unref(intlistanswer);
	
	return TRUE;
}

mmgui_svcmanager_t mmgui_svcmanager_open(mmgui_polkit_t polkit, mmgui_svcmanager_event_callback callback, gpointer userdata)
{
	mmgui_svcmanager_t svcmanager;
	GError *error;

	/*Service manager object for system services activation*/
	svcmanager = g_new0(struct _mmgui_svcmanager, 1);
	svcmanager->systemdtech = FALSE;
	svcmanager->managerproxy = NULL;
	svcmanager->services = NULL;
	svcmanager->dbustech = FALSE;
	svcmanager->dbusproxy = NULL;
	svcmanager->interfaces = NULL;
	svcmanager->polkit = polkit;
	svcmanager->callback = callback;
	svcmanager->userdata = userdata;
	svcmanager->errormsg = NULL;

	error = NULL;
	/*DBus system bus connection*/
	svcmanager->connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if ((svcmanager->connection == NULL) && (error != NULL)) {
		g_debug("Error getting system bus connection: %s", error->message);
		g_error_free(error);
		g_free(svcmanager);
		return NULL;
	}
	
	/*Systemd manager interface*/
	svcmanager->systemdtech = mmgui_svcmanager_open_systemd_manager_interface(svcmanager);
	/*Dbus activation interface*/
	svcmanager->dbustech = mmgui_svcmanager_open_dbus_activation_interface(svcmanager);
	
	/*At least DBus interface must be available*/
	if ((!svcmanager->systemdtech) && (!svcmanager->dbustech)) {
		g_object_unref(svcmanager->connection);
		g_free(svcmanager);
		return NULL;
	}
	
	/*Initilize static transitions queue*/
	svcmanager->transqueue = g_queue_new();
	svcmanager->svcsinqueue = FALSE;
	svcmanager->intsinqueue = FALSE;

	return svcmanager;
}

void mmgui_svcmanager_close(mmgui_svcmanager_t svcmanager)
{
	if (svcmanager == NULL) return;

	/*Systemd services hash table*/
	if (svcmanager->services != NULL) {
		g_hash_table_destroy(svcmanager->services);
		svcmanager->services = NULL;
	}
	/*Systemd proxy object*/
	if (svcmanager->managerproxy != NULL) {
		g_object_unref(svcmanager->managerproxy);
		svcmanager->managerproxy = NULL;
	}
	/*DBus interfaces hash table*/
	if (svcmanager->interfaces != NULL) {
		g_hash_table_destroy(svcmanager->interfaces);
		svcmanager->interfaces = NULL;
	}
	/*DBus proxy object*/
	if (svcmanager->dbusproxy != NULL) {
		g_object_unref(svcmanager->dbusproxy);
		svcmanager->dbusproxy = NULL;
	}
	/*DBus system bus connection*/
	if (svcmanager->connection != NULL) {
		g_object_unref(svcmanager->connection);
		svcmanager->connection = NULL;
	}
	/*Last error message*/
	if (svcmanager->errormsg != NULL) {
		g_free(svcmanager->errormsg);
		svcmanager->errormsg = NULL;
	}

	g_free(svcmanager);
}

gboolean mmgui_svcmanager_get_service_state(mmgui_svcmanager_t svcmanager, gchar *svcname, gchar *svcint)
{
	mmgui_svcmanager_service_t service;
	mmgui_svcmanager_interface_t interface;
	
	if ((svcmanager == NULL) || ((svcname == NULL) && (svcint == NULL))) return FALSE;
	
	/*Systemd service state*/
	if ((svcmanager->systemdtech) && (svcname != NULL)) {
		service = g_hash_table_lookup(svcmanager->services, svcname);
		if (service != NULL) {
			return service->running;
		}
	}
	
	/*DBus interface state*/
	if ((svcmanager->dbustech) && (svcint != NULL)) {
		interface = g_hash_table_lookup(svcmanager->interfaces, svcint);
		if (interface != NULL) {
			return interface->active;
		}
	}
	
	return FALSE;
}

gint mmgui_svcmanager_get_service_activation_tech(mmgui_svcmanager_t svcmanager, gchar *svcname, gchar *svcint)
{
	mmgui_svcmanager_service_t service;
	mmgui_svcmanager_interface_t interface;
	
	if ((svcmanager == NULL) || ((svcname == NULL) && (svcint == NULL))) return MMGUI_SVCMANGER_ACTIVATION_TECH_DBUS;
	
	/*Systemd activation*/
	if ((svcmanager->systemdtech) && (svcname != NULL)) {
		service = g_hash_table_lookup(svcmanager->services, svcname);
		if (service != NULL) {
			if (!service->running) {
				return MMGUI_SVCMANGER_ACTIVATION_TECH_SYSTEMD;
			}
		}
	}
	
	/*DBus activation*/
	if ((svcmanager->dbustech) && (svcint != NULL)) {
		interface = g_hash_table_lookup(svcmanager->interfaces, svcint);
		if (interface != NULL) {
			if (interface->activatable) {
				if (!interface->active) {
					return MMGUI_SVCMANGER_ACTIVATION_TECH_DBUS;
				}
			}
		}
	}
	
	return MMGUI_SVCMANGER_ACTIVATION_TECH_NA;
}

gboolean mmgui_svcmanager_schedule_start_service(mmgui_svcmanager_t svcmanager, gchar *svcname, gchar *svcint, gchar *modname, gboolean enable)
{
	mmgui_svcmanager_transition_t transition;
	mmgui_svcmanager_service_t service;
	mmgui_svcmanager_interface_t interface;
	
	if ((svcmanager == NULL) || ((svcname == NULL) && (svcint == NULL))) return FALSE;
	
	/*Systemd activation queue*/
	if ((svcmanager->systemdtech) && (svcname != NULL)) {
		service = g_hash_table_lookup(svcmanager->services, svcname);
		if (service != NULL) {
			if (!service->running) {
				transition = g_new0(struct _mmgui_svcmanager_transition, 1);
				transition->type = MMGUI_SVCMANGER_TYPE_SERVICE;
				transition->modname = g_strdup(modname);
				transition->job = NULL;
				transition->timer = 0;
				transition->enable = enable;
				transition->entity.service = service;
				g_queue_push_head(svcmanager->transqueue, transition);
				svcmanager->svcsinqueue = TRUE;
				g_debug("Systemd service \'%s\' start scheduled\n", svcname);
			} else {
				g_debug("Systemd service \'%s\' already running\n", svcname);
			}
			return TRUE;
		}
		g_printf("Can\'t use Systemd for \'%s\' service activation\n", svcname);
	}
	
	/*DBus activation queue*/
	if ((svcmanager->dbustech) && (svcint != NULL)) {
		interface = g_hash_table_lookup(svcmanager->interfaces, svcint);
		if (interface != NULL) {
			if (interface->activatable) {
				if (!interface->active) {
					transition = g_new0(struct _mmgui_svcmanager_transition, 1);
					transition->type = MMGUI_SVCMANGER_TYPE_INTERFACE;
					transition->modname = g_strdup(modname);
					transition->job = NULL;
					transition->timer = 0;
					transition->enable = enable;
					transition->entity.interface = interface;
					g_queue_push_tail(svcmanager->transqueue, transition);
					svcmanager->intsinqueue = TRUE;
					g_debug("DBus interface \'%s\' activation scheduled\n", svcint);
				} else {
					g_debug("DBus interface \'%s\' already active\n", svcint);
				}
				return TRUE;
			}
		}
		g_printf("Can\'t use DBus activation mechanism for \'%s\' interface\n", svcint);
	}
	
	return FALSE;
}

static gboolean mmgui_svcmanager_services_enable(mmgui_svcmanager_t svcmanager, mmgui_svcmanager_transition_t transition)
{
	gchar **services;
	GVariant *request;
    GVariant *answer;
    GError *error;
	
	if ((svcmanager == NULL) || (transition == NULL)) return FALSE;
	if ((!svcmanager->systemdtech) || (transition->type != MMGUI_SVCMANGER_TYPE_SERVICE) || (!transition->enable)) return FALSE;
	
	/*Systemd services array to enable*/
	services = g_malloc0(sizeof(gchar *) * 2);
	services[0] = g_strdup(transition->entity.service->name);
	services[1] = NULL;
	
	/*Request with array and flags for persistent enablement and symlinks repalcement*/
	request	= g_variant_new("(^asbb)", services, FALSE, TRUE);
	
	error = NULL;
	answer = g_dbus_proxy_call_sync(svcmanager->managerproxy,
									"EnableUnitFiles",
									request,
									G_DBUS_CALL_FLAGS_NONE,
									-1,
									NULL,
									&error);
	
	if ((answer == NULL) && (error != NULL)) {
		/*Free services array*/
		g_strfreev(services);
		/*Return error*/
		g_debug("Unable to enable Systemd service \'%s\': %s\n", transition->entity.service->name, error->message);
		mmgui_svcmanager_set_last_error(svcmanager, error->message);
		g_error_free(error);
		return FALSE;
	}
	
	/*Not really interested in result*/
	g_variant_unref(answer);
	
	/*Free services array*/
	g_strfreev(services);
	
	return TRUE;
}

static gboolean mmgui_svcmanager_services_activation_service_timeout(gpointer userdata)
{
	mmgui_svcmanager_t svcmanager;
	mmgui_svcmanager_transition_t transition;
	
	svcmanager = (mmgui_svcmanager_t)userdata;
	
	if (svcmanager == NULL) return G_SOURCE_REMOVE;
	
	/*Remove transition object from queue*/
	transition = g_queue_pop_head(svcmanager->transqueue);
	/*Signal error*/
	g_debug("Timeout while starting Systemd service \'%s\' for module \'%s\'", transition->entity.service->name, transition->modname);
	mmgui_svcmanager_set_last_error(svcmanager, _("Timeout"));
	if (svcmanager->callback != NULL) {
		(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_ENTITY_ERROR, transition, svcmanager->userdata);
	}
	/*Destroy transition*/
	mmgui_transition_destroy(transition);
	/*Free transition queue*/
	mmgui_transition_queue_destroy(svcmanager->transqueue);
	
	return G_SOURCE_REMOVE;
}

static void mmgui_svcmanager_services_activation_service_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer userdata)
{
	mmgui_svcmanager_t svcmanager;
	mmgui_svcmanager_transition_t transition;
	guint jobid;
	gchar *jobpath, *svcname, *svcstate, *statedescr;
	
	svcmanager = (mmgui_svcmanager_t)userdata;
	
	if (svcmanager == NULL) return;
	
	transition = g_queue_peek_head(svcmanager->transqueue);
	
	/*Queue mustn't be empty*/
	if (transition != NULL) {
		/*We're looking for job termination*/
		if (g_str_equal(signal_name, "JobRemoved")) {
			/*Job status description*/
			g_variant_get(parameters, "(uoss)", &jobid, &jobpath, &svcname, &svcstate);
			/*Compare job path and check status*/
			if (g_str_equal(jobpath, transition->job)) {
				/*Remove transition object from queue and stop timer*/
				transition = g_queue_pop_head(svcmanager->transqueue);
				g_source_remove(transition->timer);
				if (g_str_equal(svcstate, "done")) {
					/*Enable service if requested*/
					if (transition->enable) {
						mmgui_svcmanager_services_enable(svcmanager, transition);
					}
					/*Service successfully started*/
					g_debug("Systemd service \'%s\' for module \'%s\' successfully started\n", transition->entity.service->name, transition->modname);
					if (svcmanager->callback != NULL) {
						(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_ENTITY_ACTIVATED, transition, svcmanager->userdata);
					}
					/*Destroy transition*/
					mmgui_transition_destroy(transition);
					/*And go to next entity*/
					mmgui_svcmanager_services_activation_chain(svcmanager);
				} else {
					/*Service not started*/
					if (g_str_equal(svcstate, "canceled")) {
						statedescr = _("Job canceled");
					} else if (g_str_equal(svcstate, "timeout")) {
						statedescr = _("Systemd timeout reached");
					} else if (g_str_equal(svcstate, "failed")) {
						statedescr = _("Service activation failed");
					} else if (g_str_equal(svcstate, "dependency")) {
						statedescr = _("Service depends on already failed service");
					} else if (g_str_equal(svcstate, "skipped")) {
						statedescr = _("Service skipped");
					} else {
						statedescr = _("Unknown error");
					}
					g_debug("Error while starting Systemd service \'%s\' for module \'%s\'. %s\n", transition->entity.service->name, transition->modname, statedescr);
					mmgui_svcmanager_set_last_error(svcmanager, statedescr);
					if (svcmanager->callback != NULL) {
						(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_ENTITY_ERROR, transition, svcmanager->userdata);
					}
					/*Destroy transition*/
					mmgui_transition_destroy(transition);
					/*Free transition queue*/
					mmgui_transition_queue_destroy(svcmanager->transqueue);
				}
			}
		}
	}
}

static void mmgui_svcmanager_services_activation_interface_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer userdata)
{
	mmgui_svcmanager_t svcmanager;
	mmgui_svcmanager_transition_t transition;
	GError *error;
	GVariant *data;
	gint status;
	
	svcmanager = (mmgui_svcmanager_t)userdata;
	
	if (svcmanager == NULL) return;
	
	/*Remove transition object from queue*/
	transition = g_queue_pop_head(svcmanager->transqueue);
	
	error = NULL;
	
	data = g_dbus_proxy_call_finish(proxy, res, &error);
	
	if ((data == NULL) && (error != NULL)) {
		/*Error while activation*/
		g_printf("Error activating interface \'%s\' for module \'%s\'. %s\n", transition->entity.interface->name, transition->modname, error->message);
		mmgui_svcmanager_set_last_error(svcmanager, error->message);
		if (svcmanager->callback != NULL) {
			(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_ENTITY_ERROR, transition, svcmanager->userdata);
		}
		g_error_free(error);
		/*Destroy transition*/
		mmgui_transition_destroy(transition);
		/*Free transition queue*/
		mmgui_transition_queue_destroy(svcmanager->transqueue);
	} else {
		/*Get activation status - maybe meaningless*/
		g_variant_get(data, "(u)", &status);
		g_variant_unref(data);
		if ((status == MMGUI_DBUS_START_REPLY_SUCCESS) || (status == MMGUI_DBUS_START_REPLY_ALREADY_RUNNING)) {
			/*Interface is activated or already exists*/
			g_debug("Interface \'%s\' for module \'%s\' successfully activated. Status: %u\n", transition->entity.interface->name, transition->modname, status);
			if (svcmanager->callback != NULL) {
				(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_ENTITY_ACTIVATED, transition, svcmanager->userdata);
			}
			/*Destroy transition*/
			mmgui_transition_destroy(transition);
			/*And go to next entity*/
			mmgui_svcmanager_services_activation_chain(svcmanager);
		} else {
			/*Status is unknown*/
			g_debug("Interface \'%s\' for module \'%s\' not activated. Unknown activation status: %u\n", transition->entity.interface->name, transition->modname, status);
			mmgui_svcmanager_set_last_error(svcmanager, _("Unknown activation status"));
			if (svcmanager->callback != NULL) {
				(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_ENTITY_ERROR, transition, svcmanager->userdata);
			}
			/*Destroy transition*/
			mmgui_transition_destroy(transition);
			/*Free transition queue*/
			mmgui_transition_queue_destroy(svcmanager->transqueue);
		}
	}
}

static gboolean mmgui_svcmanager_services_activation_chain(mmgui_svcmanager_t svcmanager)
{
	mmgui_svcmanager_transition_t transition;
	GVariant *answer;
    GError *error;
	
	transition = g_queue_peek_head(svcmanager->transqueue);
	
	if (transition != NULL) {
		/*Work on next transistion*/
		if (transition->type == MMGUI_SVCMANGER_TYPE_SERVICE) {
			/*Schedule Systemd unit start*/
			error = NULL;
			answer = g_dbus_proxy_call_sync(svcmanager->managerproxy,
											"StartUnit",
											g_variant_new("(ss)", transition->entity.service->name, "replace"),
											G_DBUS_CALL_FLAGS_NONE,
											-1,
											NULL,
											&error);
			
			if ((answer == NULL) && (error != NULL)) {
				g_debug("Unable to schedule start Systemd service \'%s\' for module \'%s\'. %s\n", transition->entity.service->name, transition->modname, error->message);
				mmgui_svcmanager_set_last_error(svcmanager, error->message);
				if (svcmanager->callback != NULL) {
					(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_ENTITY_ERROR, transition, svcmanager->userdata);
				}
				g_error_free(error);
				/*Free transition queue*/
				mmgui_transition_queue_destroy(svcmanager->transqueue);
				return FALSE;
			}
			/*Watch for job completion*/
			g_variant_get(answer, "(o)", &transition->job);
			transition->job = g_strdup(transition->job);
			g_variant_unref(answer);
			g_debug("Systemd service activation started, job: %s\n", transition->job);
			/*Timer used as timeout source*/
			transition->timer = g_timeout_add(MMGUI_SVCMANAGER_ACTIVATION_TIMEOUT, mmgui_svcmanager_services_activation_service_timeout, svcmanager);
			if (svcmanager->callback != NULL) {
				(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_ENTITY_CHANGED, transition, svcmanager->userdata);
			}
		} else if (transition->type == MMGUI_SVCMANGER_TYPE_INTERFACE) {
			/*Schedule DBus service activation*/
			g_dbus_proxy_call(svcmanager->dbusproxy,
								"StartServiceByName",
								g_variant_new("(su)", transition->entity.interface->name, 0),
								G_DBUS_CALL_FLAGS_NONE,
								MMGUI_SVCMANAGER_ACTIVATION_TIMEOUT,
								NULL,
								(GAsyncReadyCallback)mmgui_svcmanager_services_activation_interface_handler,
								svcmanager);
			g_debug("Interface activation started");
			if (svcmanager->callback != NULL) {
				(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_ENTITY_CHANGED, transition, svcmanager->userdata);
			}
		} else {
			/*Unknown entity type*/
			g_debug("Unknown entity type: %u\n", transition->type);
			mmgui_svcmanager_set_last_error(svcmanager, _("Unknown entity type"));
			if (svcmanager->callback != NULL) {
				(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_ENTITY_ERROR, transition, svcmanager->userdata);
			}
			/*Free transition queue*/
			mmgui_transition_queue_destroy(svcmanager->transqueue);
			return FALSE;
		}
	} else {
		/*No more jobs in queue - finish process*/
		if (svcmanager->svcsinqueue) {
			/*Unsubscribe from Systemd events if needed*/
			error = NULL;
			g_dbus_proxy_call_sync(svcmanager->managerproxy,
									"Unsubscribe",
									NULL,
									G_DBUS_CALL_FLAGS_NONE,
									-1,
									NULL,
									&error);
			
			if (error != NULL) {
				/*Do not pay much attention to such errors*/
				g_printf("Unable to unsubscribe from Systemd events: %s\n", error->message);
				g_error_free(error);
			}
		}
		/*Drop state values*/
		svcmanager->svcsinqueue = FALSE;
		svcmanager->intsinqueue = FALSE;
		/*Signal process finish*/
		if (svcmanager->callback != NULL) {
			(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_FINISHED, NULL, svcmanager->userdata);
		}
	}
	
	return TRUE;
}

gboolean mmgui_svcmanager_start_services_activation(mmgui_svcmanager_t svcmanager)
{
	GError *error;
	    	
	if (svcmanager == NULL) return FALSE;
	
	/*If activation queue is empty just return*/
	if (g_queue_get_length(svcmanager->transqueue) == 0) {
		g_debug("All services already active - nothing to do\n");
		if (svcmanager->callback != NULL) {
			(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_FINISHED, NULL, svcmanager->userdata);
		}
		return TRUE;
	}
	
	/*Request password for Systemd units activation*/
	if (svcmanager->svcsinqueue) {
		if (!mmgui_polkit_request_password(svcmanager->polkit, "ru.linuxonly.modem-manager-gui.manage-services")) {
			g_debug("Wrong credentials - unable to continue with services activation\n");
			if (svcmanager->callback != NULL) {
				(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_AUTH_ERROR, NULL, svcmanager->userdata);
			}
			return FALSE;
		}
		/*OK, authorization was successfull, now subscribe to signals*/
		error = NULL;
		g_dbus_proxy_call_sync(svcmanager->managerproxy,
								"Subscribe",
								NULL,
								G_DBUS_CALL_FLAGS_NONE,
								-1,
								NULL,
								&error);
		
		if (error != NULL) {
			g_debug("Unable to subscribe to signals: %s\n", error->message);
			mmgui_svcmanager_set_last_error(svcmanager, error->message);
			if (svcmanager->callback != NULL) {
				(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_OTHER_ERROR, NULL, svcmanager->userdata);
			}
			g_error_free(error);
			return FALSE;
		}
		/*And connect signal handler*/
		g_signal_connect(svcmanager->managerproxy, "g-signal", G_CALLBACK(mmgui_svcmanager_services_activation_service_handler), svcmanager);
	}
	
	/*Signal start services activation process*/
	if (svcmanager->callback != NULL) {
		(svcmanager->callback)(svcmanager, MMGUI_SVCMANGER_EVENT_STARTED, NULL, svcmanager->userdata);
	}
	
	/*And start activation*/
	mmgui_svcmanager_services_activation_chain(svcmanager);
	
	if (svcmanager->svcsinqueue) {
		/*Drop autorization*/
		mmgui_polkit_revoke_autorization(svcmanager->polkit, "ru.linuxonly.modem-manager-gui.manage-services");
	}
	
	return TRUE;
}

gchar *mmgui_svcmanager_get_transition_module_name(mmgui_svcmanager_transition_t transition)
{
	if (transition == NULL) return NULL;
	
	return transition->modname;
}
