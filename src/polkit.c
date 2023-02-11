/*
 *      polkit.c
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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gio/gio.h>
#include <stdlib.h>

#include "polkit.h"

static guint64 mmgui_polkit_get_process_start_time(void);
static void mmgui_polkit_action_destroy_key(gpointer data);
static void mmgui_polkit_action_destroy_value(gpointer data);


static guint64 mmgui_polkit_get_process_start_time(void)
{
	pid_t curpid;
	guint64 starttime;
	gchar *statfilepath;
	gchar *statfilecont;
	gsize statfilelen;
	GError *error;
	gchar *valueendptr;
	gchar **statfiletokens;
	gint statfiletokennum;
		
	starttime = 0;
	statfilecont = NULL;
	/*Current process PID*/
	curpid = getpid();
	
	/*Name of procfs file with process information*/
	statfilepath = g_strdup_printf("/proc/%u/stat", (guint)curpid);
		
	/*Getting contents of procfs file*/
	error = NULL;
	
	if (!g_file_get_contents((const gchar *)statfilepath, &statfilecont, &statfilelen, &error)) {
		if (error != NULL) {
			g_debug("Unable to read procfs file: %s", error->message);
			g_error_free(error);
		}
		g_free(statfilepath);
		return starttime;
	}
	
	g_free(statfilepath);
	
	statfiletokens = g_strsplit (statfilecont, " ", -1);
	if (statfiletokens != NULL) {
		statfiletokennum = 0;
		while (statfiletokens[statfiletokennum] != NULL) {
			statfiletokennum++;
		}
		if (statfiletokennum >= 51) {
			if (curpid == strtoul(statfiletokens[0], &valueendptr, 10)) {
				starttime = strtoull(statfiletokens[statfiletokennum - 31], &valueendptr, 10);	
			}
		}
		g_strfreev(statfiletokens);
	}
	
	g_free(statfilecont);
	
	return starttime;
}

static void mmgui_polkit_action_destroy_key(gpointer data)
{
	gchar *key;
	
	if (data == NULL) return;
	
	key = (gchar *)data;
	
	g_free(key);
}

static void mmgui_polkit_action_destroy_value(gpointer data)
{
	mmgui_polkit_action_t action;
	
	if (data == NULL) return;
	
	action = (mmgui_polkit_action_t)data;
	
	if (action->implyactions != NULL) {
		g_strfreev(action->implyactions);
	}
	
	g_free(action);
}

mmgui_polkit_t mmgui_polkit_open(void)
{
	mmgui_polkit_t polkit;
	GError *error;
	gchar *locale;
	GVariant *actionsv;
	GVariantIter actionsiter, actionsiter2;
	GVariant *actionsnode, *actionsnode2;
	GVariant *actiondescv;
	GVariantIter actiondesciter;
	gchar *actiondesckey, *actiondescvalue;
	mmgui_polkit_action_t action;
	gchar *actionname;
	
	polkit = g_new0(struct _mmgui_polkit, 1);
	
	error = NULL;
	/*DBus system bus connection*/
	polkit->connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    
    if (polkit->connection == NULL) {
		if (error != NULL) {
			g_debug("Error getting system bus connection: %s", error->message);
			g_error_free(error);
		}
		g_free(polkit);
		return NULL;
	}
	/*Polkit proxy object*/
	polkit->proxy = g_dbus_proxy_new_sync(polkit->connection,
											G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
											NULL,
											"org.freedesktop.PolicyKit1",
											"/org/freedesktop/PolicyKit1/Authority",
											"org.freedesktop.PolicyKit1.Authority",
											NULL,
											&error);
	
	if (polkit->proxy == NULL) {
		if (error != NULL) {
			g_debug("Error creating DBus proxy object for polkit interface: %s", error->message);
			g_error_free(error);
		}
		g_object_unref(polkit->connection);
		g_free(polkit);
		return NULL;
	}
	/*Process start time in special procfs format*/
	polkit->starttime = mmgui_polkit_get_process_start_time();
	
	if (polkit->starttime == 0) {
		g_debug("Error getting process start time from /proc filesystem");
		g_object_unref(polkit->proxy);
		g_object_unref(polkit->connection);
		g_free(polkit);
		return NULL;
	}
	/*Get system locale*/
	locale = getenv("LANG");
	if (locale == NULL) {
		locale = "C";
	}
	/*Get polkit actions enumeration*/
	actionsv = g_dbus_proxy_call_sync(polkit->proxy,
									"EnumerateActions",
									g_variant_new("(s)", locale),
									0,
									-1,
									NULL,
									&error);
	
	if (actionsv == NULL) {
		if (error != NULL) {
			g_debug("Error while enumerating polkit actions: %s", error->message);
			g_error_free(error);
		}
		g_object_unref(polkit->proxy);
		g_object_unref(polkit->connection);
		g_free(polkit);
		return NULL;
	}
	/*Actions hash table*/
	polkit->actions = g_hash_table_new_full(g_str_hash, g_str_equal, mmgui_polkit_action_destroy_key, mmgui_polkit_action_destroy_value);
	/*Fill table*/
	g_variant_iter_init(&actionsiter, actionsv);
	while ((actionsnode = g_variant_iter_next_value(&actionsiter)) != NULL) {
		g_variant_iter_init(&actionsiter2, actionsnode);
		while ((actionsnode2 = g_variant_iter_next_value(&actionsiter2)) != NULL) {
			/*New action*/
			action = g_new0(struct _mmgui_polkit_action, 1);
			g_variant_get(actionsnode2, "(ssssssuuua{ss})", &actionname, NULL, NULL, NULL, NULL, NULL, &action->anyauth, &action->inactiveauth, &action->activeauth, NULL);
			/*Imply section of action description*/
			actiondescv = g_variant_get_child_value(actionsnode2, 9);
			if (g_variant_n_children(actiondescv) > 0) {
				g_variant_iter_init(&actiondesciter, actiondescv);
				while (g_variant_iter_loop(&actiondesciter, "{ss}", &actiondesckey, &actiondescvalue)) {
					if (g_str_equal(actiondesckey, "org.freedesktop.policykit.imply")) {
						action->implyactions = g_strsplit(actiondescvalue, " ", -1);
					}
				}
			}
			/*Insert action into hash table*/
			g_hash_table_insert(polkit->actions, g_strdup(actionname), action);
			g_variant_unref(actionsnode2);
		}
		g_variant_unref(actionsnode);
    }
	g_variant_unref(actionsv);
	
	
	return polkit;
}

void mmgui_polkit_close(mmgui_polkit_t polkit)
{
	if (polkit == NULL) return;
	
	/*Polkit actions hash table*/
	if (polkit->actions != NULL) {
		g_hash_table_destroy(polkit->actions);
		polkit->actions = NULL;
	}
	/*Polkit proxy object*/
	if (polkit->proxy != NULL) {
		g_object_unref(polkit->proxy);
		polkit->proxy = NULL;
	}
	/*DBus system bus connection*/
	if (polkit->connection != NULL) {
		g_object_unref(polkit->connection);
		polkit->connection = NULL;
	}
	
	g_free(polkit);
}

gboolean mmgui_polkit_action_needed(mmgui_polkit_t polkit, const gchar *actionname, gboolean strict)
{
	mmgui_polkit_action_t action;
	gint i;
	gboolean res;
	
	if ((polkit == NULL) || (actionname == NULL)) return FALSE;
	
	res = FALSE;
	
	if (polkit->actions == NULL) {
		g_debug("Polkit actions table is empty");
		return res;
	}
	
	action = g_hash_table_lookup(polkit->actions, actionname);
	
	if (action == NULL) {
		g_debug("No such action in polkit actions table");
		return res;
	}
	
	if (action->implyactions == NULL) {
		/*Action is implemented*/
		res = TRUE;
	} else {
		/*Action contains imply list*/
		res = strict;
		i = 0;
		while (action->implyactions[i] != NULL) {
			if (strict) {
				/*All actions must be available*/
				res = res && (g_hash_table_lookup(polkit->actions, action->implyactions[i]) != NULL);
				if (!res) break;
			} else {
				/*At least one action must be available*/
				res = res || (g_hash_table_lookup(polkit->actions, action->implyactions[i]) != NULL);
				if (res) break;
			}
			i++;
		}
	}
	
	return res;
}

gboolean mmgui_polkit_is_authorized(mmgui_polkit_t polkit, const gchar *actionname)
{
	mmgui_polkit_action_t action;
	
	if ((polkit == NULL) || (actionname == NULL)) return FALSE;
	
	if (polkit->actions == NULL) {
		g_debug("Polkit actions table is empty");
		return FALSE;
	}
	
	action = g_hash_table_lookup(polkit->actions, actionname);
	
	if (action == NULL) {
		g_debug("No such action in polkit actions table");
		return FALSE;
	}
	
	return (action->activeauth == MMGUI_POLKIT_ADMINISTRATOR_AUTHENTICATION_REQUIRED_RETAINED);
}

gboolean mmgui_polkit_request_password(mmgui_polkit_t polkit, const gchar *actionname)
{
	GVariantBuilder builder;
	GVariant *variant;
	guint32 curpid;
	guint32 curuid;
	GVariant *requestsubject;
	GVariant *requestdetails;
	GVariant *request;
	GVariant *answer;
	GError *error;
	gboolean authstatus;
	
	if ((polkit == NULL) || (actionname == NULL)) return FALSE;
	
	/*Test action*/
	if (g_hash_table_lookup(polkit->actions, actionname) == NULL) {
		g_debug("Action not found in polkit actions table");
		return FALSE;
	}
	/*Form request*/
	g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
	/*Process PID*/
	curpid = (guint32)getpid();
	variant = g_variant_new("u", curpid);
	g_variant_builder_add(&builder, "{sv}", "pid", variant);
	/*Process start time*/
	variant = g_variant_new("t", polkit->starttime);
	g_variant_builder_add(&builder, "{sv}", "start-time", variant);
	curuid = (guint32)getuid();
	variant = g_variant_new("u", curuid);
	g_variant_builder_add(&builder, "{sv}", "uid", variant);
	/*Polkit request subject*/
	requestsubject = g_variant_new("(sa{sv})", "unix-process", &builder);
	g_variant_builder_init(&builder, G_VARIANT_TYPE ("a{ss}"));
	/*Polkit request details*/
	requestdetails = g_variant_new("a{ss}", &builder);
	
	request = g_variant_new("(@(sa{sv})s@a{ss}us)",
							requestsubject,
							actionname,
							requestdetails,
							1,
							"");
	/*Send request and receive answer*/
	error = NULL;
	answer = g_dbus_proxy_call_sync(polkit->proxy,
									"CheckAuthorization",
									request,
									G_DBUS_CALL_FLAGS_NONE,
									-1,
									NULL,
									&error);
									
	if (answer == NULL) {
		if (error != NULL) {
			g_debug("Unable to request authorization: %s\n", error->message);
			g_error_free(error);
		}
		return FALSE;
	}
	
	g_variant_get(answer, "((bba{ss}))", &authstatus, NULL, NULL);
	g_variant_unref(answer);
	
	return authstatus;
}

gboolean mmgui_polkit_revoke_authorization(mmgui_polkit_t polkit, const gchar *actionname)
{
	GError *error;
	
	if ((polkit == NULL) || (actionname == NULL)) return FALSE;
	
	/*Test action*/
	if (g_hash_table_lookup(polkit->actions, actionname) == NULL) {
		g_debug("Action not found in polkit actions table");
		return FALSE;
	}
	
	/*Send request and receive answer*/
	error = NULL;
	g_dbus_proxy_call_sync(polkit->proxy,
							"RevokeTemporaryAuthorizationById",
							g_variant_new("(s)", actionname),
							G_DBUS_CALL_FLAGS_NONE,
							-1,
							NULL,
							&error);
									
	if (error != NULL) {
		g_debug("Unable to revoke authorization: %s\n", error->message);
		g_error_free(error);
		return FALSE;
	}
	
	return TRUE;
}
