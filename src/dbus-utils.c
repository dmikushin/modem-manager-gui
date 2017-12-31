/*
 *      dbus-utils.c
 *      
 *      Copyright 2017 Alex <alex@linuxonly.ru>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>


static void mmgui_dbus_utils_session_list_service_interfaces_xml_get_element(GMarkupParseContext *context, const gchar *element, const gchar **attr_names, const gchar **attr_values, gpointer data, GError **error);


gboolean mmgui_dbus_utils_session_service_activate(gchar *interface, guint *status)
{
	GDBusConnection *connection;
	GDBusProxy *proxy;
	gboolean res;
	GVariant *statusv;
	GError *error;

	if (interface == NULL) return FALSE;

	error = NULL;

	connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

	if ((connection == NULL) && (error != NULL)) {
		g_debug("Core error: %s\n", error->message);
		g_error_free(error);
		return FALSE;
	}

	error = NULL;

	proxy = g_dbus_proxy_new_sync(connection,
									G_DBUS_PROXY_FLAGS_NONE,
									NULL,
									"org.freedesktop.DBus",
									"/org/freedesktop/DBus",
									"org.freedesktop.DBus",
									NULL,
									&error);

	if ((proxy == NULL) && (error != NULL)) {
		g_debug("Core error: %s\n", error->message);
		g_error_free(error);
		g_object_unref(connection);
		return FALSE;
	}

	error = NULL;

	statusv = g_dbus_proxy_call_sync(proxy,
									"StartServiceByName",
									g_variant_new("(su)", interface, 0),
									0,
									-1,
									NULL,
									&error);

	if ((statusv == NULL) && (error != NULL)) {
		g_debug("Core error: %s\n", error->message);
		g_error_free(error);
		res = FALSE;
	} else {
		if (status != NULL) {
			g_variant_get(statusv, "(u)", status);
			g_debug("Service start status: %u\n", *status);
		}
		res = TRUE;
	}

	g_object_unref(proxy);
	g_object_unref(connection);

	return res;
}

static void mmgui_dbus_utils_session_list_service_interfaces_xml_get_element(GMarkupParseContext *context, const gchar *element, const gchar **attr_names, const gchar **attr_values, gpointer data, GError **error)
{
	GHashTable *hash;
		
	hash = (GHashTable *)data;
	
	if (hash == NULL) return;
	
	/*We interested only in interfaces*/
	if (g_str_equal(element, "interface")) {
		if ((attr_names[0] != NULL) && (attr_values[0] != NULL)) {
			if (g_str_equal(attr_names[0], "name")) {
				g_hash_table_add(hash, g_strdup(attr_values[0]));
			}
		}
	}
}

GHashTable *mmgui_dbus_utils_list_service_interfaces(GDBusConnection *connection, gchar *servicename, gchar *servicepath)
{
	GDBusProxy *proxy;
	GVariant *xmlv;
	GError *error;
	gchar *xml;
	GMarkupParser mp;
	GMarkupParseContext *mpc;
	GHashTable *hash;
	
	if ((connection == NULL) || (servicename == NULL) || (servicepath == NULL)) return NULL;
	
	hash = NULL;
	
	/*Create proxy for requested service*/
	error = NULL;
	proxy = g_dbus_proxy_new_sync(connection,
									G_DBUS_PROXY_FLAGS_NONE,
									NULL,
									servicename,
									servicepath,
									"org.freedesktop.DBus.Introspectable",
									NULL,
									&error);
									
	if (proxy == NULL) {
		if (error != NULL) {
			g_debug("Core error: %s\n", error->message);
			g_error_free(error);
		}
		return NULL;
	}
	
	/*Call introspect method*/
	error = NULL;
	xmlv = g_dbus_proxy_call_sync(proxy,
								"Introspect",
								NULL,
								0,
								-1,
								NULL,
								&error);
	
	g_object_unref(proxy);
	
	if (xmlv != NULL) {
		g_variant_get(xmlv, "(s)", &xml);
		if ((xml != NULL) && (xml[0] != '\0')) {
			/*Create hash table*/
			hash = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)g_free, NULL);
			/*Set callbacks*/
			mp.start_element = mmgui_dbus_utils_session_list_service_interfaces_xml_get_element;
			mp.end_element = NULL;
			mp.text = NULL;
			mp.passthrough = NULL;
			mp.error = NULL;
			/*Create markup context*/
			mpc = g_markup_parse_context_new(&mp, 0, hash, NULL);
			/*Parse*/
			if (!g_markup_parse_context_parse(mpc, xml, strlen(xml), &error)) {
				if (error != NULL) {
					g_debug("Parser error: %s\n", error->message);
					g_error_free(error);
				}
				g_markup_parse_context_free(mpc);
				g_hash_table_destroy(hash);
				return NULL;
			}
			g_markup_parse_context_free(mpc);
		}
		g_variant_unref(xmlv);
	} else {
		if (error != NULL) {
			g_debug("Core error: %s\n", error->message);
			g_error_free(error);
		}
		return NULL;
	}
	
	return hash;
}
