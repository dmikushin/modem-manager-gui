/*
 *      mmguicore.c
 *      
 *      Copyright 2013-2018 Alex <alex@linuxonly.ru>
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
#include <gmodule.h>
#include <gio/gio.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <poll.h>

#include "mmguicore.h"
#include "smsdb.h"
#include "trafficdb.h"
#include "netlink.h"
#include "polkit.h"
#include "svcmanager.h"
#include "../resources.h"

/*Module*/
#define MMGUI_MODULE_FILE_PREFIX         '_'
#define MMGUI_MODULE_FILE_EXTENSION      ".so"
/*Cache file*/
#define MMGUICORE_CACHE_DIR              "modem-manager-gui"
#define MMGUICORE_CACHE_FILE             "modules.conf"
#define MMGUICORE_CACHE_PERM             0755
#define MMGUICORE_CACHE_VER              4
/*Cache file sections*/
#define MMGUICORE_FILE_ROOT_SECTION      "cache"
#define MMGUICORE_FILE_TIMESTAMP         "timestamp"
#define MMGUICORE_FILE_VERSION           "version"
/*Module entry*/
#define MMGUICORE_FILE_IDENTIFIER        "identifier"
#define MMGUICORE_FILE_TYPE              "type"
#define MMGUICORE_FILE_REQUIREMENT       "requirement"
#define MMGUICORE_FILE_PRIORITY          "priority"
#define MMGUICORE_FILE_FUNCTIONS         "functions"
#define MMGUICORE_FILE_DESCRIPTION       "description"
#define MMGUICORE_FILE_SERVICENAME       "servicename"
#define MMGUICORE_FILE_SYSTEMDNAME       "systemdname"
#define MMGUICORE_FILE_COMPATIBILITY     "compatibility"
/*SMS parameters*/
#define MMGUI_MIN_SMS_NUMBER_LENGTH      3
#define MMGUI_MAX_SMS_NUMBER_LENGTH      20
/*USSD parameters*/
#define MMGUI_MIN_USSD_REQUEST_LENGTH    1
#define MMGUI_MAX_USSD_REQUEST_LENGTH    160
/*Commands*/
#define MMGUI_THREAD_STOP_CMD            0x00
#define MMGUI_THREAD_REFRESH_CMD         0x01


static void mmguicore_event_callback(enum _mmgui_event event, gpointer mmguicore, gpointer data);
static void mmguicore_svcmanager_callback(gpointer svcmanager, gint event, gpointer subject, gpointer userdata);
static gint mmguicore_modules_sort(gconstpointer a, gconstpointer b);
static gchar *mmguicore_modules_extract_name(const gchar *filename, gchar *buffer, gsize *bufsize);
static gboolean mmguicore_modules_are_compatible(mmguimodule_t module1, mmguimodule_t module2);
static gboolean mmguicore_modules_cache_open(mmguicore_t mmguicore);
static gboolean mmguicore_modules_cache_close(mmguicore_t mmguicore);
static gboolean mmguicore_modules_cache_add_module(mmguicore_t mmguicore, mmguimodule_t module);
static gint mmguicore_modules_cache_get_enumeration_value(GKeyFile *keyfile, gchar *group, gchar *parameter, gint firstvalue, ...);
static gchar *mmguicore_modules_cache_get_string_value(GKeyFile *keyfile, gchar *group, gchar *parameter, gchar *target, gsize size);
static GSList *mmguicore_modules_cache_form_list(mmguicore_t mmguicore);
static GSList *mmguicore_modules_dir_form_list(mmguicore_t mmguicore, gboolean updatecache);
static gboolean mmguicore_modules_enumerate(mmguicore_t mmguicore);
static gboolean mmguicore_modules_prepare(mmguicore_t mmguicore);
static gboolean mmguicore_modules_free(mmguicore_t mmguicore);
static gboolean mmguicore_modules_mm_open(mmguicore_t mmguicore, mmguimodule_t mmguimodule);
static gboolean mmguicore_modules_cm_open(mmguicore_t mmguicore, mmguimodule_t mmguimodule);
static gboolean mmguicore_modules_close(mmguicore_t mmguicore);
static gboolean mmguicore_modules_select(mmguicore_t mmguicore);
static gint mmguicore_connections_compare(gconstpointer a, gconstpointer b);
static gint mmguicore_connections_name_compare(gconstpointer a, gconstpointer b);
static void mmguicore_connections_free_single(mmguiconn_t connection);
static void mmguicore_connections_free_foreach(gpointer data, gpointer user_data);
static void mmguicore_connections_free(mmguicore_t mmguicore);
static void mmguicore_devices_free_single(mmguidevice_t device);
static void mmguicore_devices_free_foreach(gpointer data, gpointer user_data);
static void mmguicore_devices_free(mmguicore_t mmguicore);
static gboolean mmguicore_devices_add(mmguicore_t mmguicore, mmguidevice_t device);
static gint mmguicore_devices_remove_compare(gconstpointer a, gconstpointer b);
static gboolean mmguicore_devices_remove(mmguicore_t mmguicore, guint deviceid);
static gint mmguicore_devices_open_compare(gconstpointer a, gconstpointer b);
static gboolean mmguicore_devices_close(mmguicore_t mmguicore);
static gint mmguicore_sms_sort_index_compare(gconstpointer a, gconstpointer b);
static gint mmguicore_sms_sort_timestamp_compare(gconstpointer a, gconstpointer b);
static void mmguicore_sms_merge_foreach(gpointer data, gpointer user_data);
static void mmguicore_networks_scan_free_foreach(gpointer data, gpointer user_data);
static void mmguicore_contacts_free_foreach(gpointer data, gpointer user_data);
static void mmguicore_contacts_free(GSList *contacts);
static guint mmguicore_contacts_enum(mmguicore_t mmguicore);
static gint mmguicore_contacts_get_compare(gconstpointer a, gconstpointer b);
static gint mmguicore_contacts_delete_compare(gconstpointer a, gconstpointer b);
static gboolean mmguicore_main(mmguicore_t mmguicore);
static gpointer mmguicore_work_thread(gpointer data);
static void mmguicore_traffic_count(mmguicore_t mmguicore, guint64 rxbytes, guint64 txbytes);
static void mmguicore_traffic_zero(mmguicore_t mmguicore);
static void mmguicore_traffic_limits(mmguicore_t mmguicore);
static void mmguicore_update_connection_status(mmguicore_t mmguicore, gboolean sendresult, gboolean result);


static void mmguicore_event_callback(enum _mmgui_event event, gpointer mmguicore, gpointer data)
{
	mmguicore_t mmguicorelc;
			
	mmguicorelc = (mmguicore_t)mmguicore;
	if (mmguicorelc == NULL) return;
	
	switch (event) {
		case MMGUI_EVENT_DEVICE_ADDED:
			mmguicore_devices_add(mmguicore, (mmguidevice_t)data);
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_DEVICE_REMOVED:
			mmguicore_devices_remove(mmguicore, GPOINTER_TO_UINT(data));
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_DEVICE_ENABLED_STATUS:
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_DEVICE_BLOCKED_STATUS:
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_DEVICE_PREPARED_STATUS:
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_DEVICE_CONNECTION_STATUS:
			if (mmguicorelc->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MONITORING) {
				mmguicore_update_connection_status(mmguicorelc, FALSE, FALSE);
			} else {
				if (mmguicorelc->extcb != NULL) {
					(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
				}
			}
			break;
		case MMGUI_EVENT_SMS_LIST_READY:
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_SMS_COMPLETED:
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_SMS_SENT:
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_USSD_RESULT:
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_MODEM_CONNECTION_RESULT:
			if (mmguicorelc->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MONITORING) {
				mmguicore_update_connection_status(mmguicore, TRUE, GPOINTER_TO_UINT(data));
			} else {
				if (mmguicorelc->extcb != NULL) {
					(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
				}
			}
			break;
		case MMGUI_EVENT_SCAN_RESULT:
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_SIGNAL_LEVEL_CHANGE:
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_NETWORK_MODE_CHANGE:
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_NETWORK_REGISTRATION_CHANGE:
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_LOCATION_CHANGE:
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_MODEM_ENABLE_RESULT:
			/*Update device information*/
			if (mmguicorelc->devices_information_func != NULL) {
				(mmguicorelc->devices_information_func)(mmguicorelc);
			}
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_MODEM_UNLOCK_WITH_PIN_RESULT:
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		case MMGUI_EVENT_EXTEND_CAPABILITIES:
			if (GPOINTER_TO_INT(data) == MMGUI_CAPS_CONTACTS) {
				/*Export contacts*/
				if (mmguicorelc->device->contactscaps & MMGUI_CONTACTS_CAPS_EXPORT) {
					mmguicore_contacts_enum(mmguicore);
				}
			}
			/*Forward event*/
			if (mmguicorelc->extcb != NULL) {
				(mmguicorelc->extcb)(event, mmguicorelc, data, mmguicorelc->userdata);
			}
			break;
		
		default: 
			
			break;
	}
}

static void mmguicore_svcmanager_callback(gpointer svcmanager, gint event, gpointer subject, gpointer userdata)
{
	mmguicore_t mmguicore;
	GSList *iterator;
	mmguimodule_t modparams;
	gchar *modname;
			
	mmguicore = (mmguicore_t)userdata;
	
	if (mmguicore == NULL) return;
	
	switch (event) {
		case MMGUI_SVCMANGER_EVENT_STARTED:
			/*Forward event*/
			if (mmguicore->extcb != NULL) {
				(mmguicore->extcb)(MMGUI_EVENT_SERVICE_ACTIVATION_STARTED, mmguicore, NULL, mmguicore->userdata);
			}
			break;
		case MMGUI_SVCMANGER_EVENT_ENTITY_ACTIVATED:
			modname = mmgui_svcmanager_get_transition_module_name(subject);
			if ((modname != NULL) && (mmguicore->modules != NULL)) {
				/*Mark module available*/
				for (iterator=mmguicore->modules; iterator; iterator=iterator->next) {
					modparams = iterator->data;
					if (g_str_equal(modparams->shortname, modname)) {
						modparams->applicable = TRUE;
						/*Forward event*/
						if (mmguicore->extcb != NULL) {
							(mmguicore->extcb)(MMGUI_EVENT_SERVICE_ACTIVATION_SERVICE_ACTIVATED, mmguicore, modparams->description, mmguicore->userdata);
						}
						break;
					}
				}
			}
			break;
		case MMGUI_SVCMANGER_EVENT_ENTITY_CHANGED:
			modname = mmgui_svcmanager_get_transition_module_name(subject);
			if ((modname != NULL) && (mmguicore->modules != NULL)) {
				/*Mark module available*/
				for (iterator=mmguicore->modules; iterator; iterator=iterator->next) {
					modparams = iterator->data;
					if (g_str_equal(modparams->shortname, modname)) {
						/*Forward event*/
						if (mmguicore->extcb != NULL) {
							(mmguicore->extcb)(MMGUI_EVENT_SERVICE_ACTIVATION_SERVICE_CHANGED, mmguicore, modparams->description, mmguicore->userdata);
						}
						break;
					}
				}
			}
			break;
		case MMGUI_SVCMANGER_EVENT_ENTITY_ERROR:
			/*Forward service error event*/
			if (mmguicore->extcb != NULL) {
				(mmguicore->extcb)(MMGUI_EVENT_SERVICE_ACTIVATION_SERVICE_ERROR, mmguicore, mmgui_svcmanager_get_last_error(svcmanager), mmguicore->userdata);
			}
			break;
		case MMGUI_SVCMANGER_EVENT_FINISHED:
			if (mmguicore_main(mmguicore)) {
				/*Forward event*/
				if (mmguicore->extcb != NULL) {
					(mmguicore->extcb)(MMGUI_EVENT_SERVICE_ACTIVATION_FINISHED, mmguicore, NULL, mmguicore->userdata);
				}
			} else {
				/*Forward startup error event*/
				if (mmguicore->extcb != NULL) {
					(mmguicore->extcb)(MMGUI_EVENT_SERVICE_ACTIVATION_STARTUP_ERROR, mmguicore, NULL, mmguicore->userdata);
				}
			}
			break;
		case MMGUI_SVCMANGER_EVENT_AUTH_ERROR:
			/*Forward authentication error event*/
			if (mmguicore->extcb != NULL) {
				(mmguicore->extcb)(MMGUI_EVENT_SERVICE_ACTIVATION_AUTH_ERROR, mmguicore, NULL, mmguicore->userdata);
			}
			break;
		case MMGUI_SVCMANGER_EVENT_OTHER_ERROR:
			/*Forward unknown error event*/
			if (mmguicore->extcb != NULL) {
				(mmguicore->extcb)(MMGUI_EVENT_SERVICE_ACTIVATION_OTHER_ERROR, mmguicore, mmgui_svcmanager_get_last_error(svcmanager), mmguicore->userdata);
			}
			break;
		default:
			
			break;
	}
}

static gint mmguicore_modules_sort(gconstpointer a, gconstpointer b)
{
	mmguimodule_t mod1, mod2;
	
	mod1 = (mmguimodule_t)a;
	mod2 = (mmguimodule_t)b;
	
	if (mod1->priority == mod2->priority) {
		if (mod1->identifier == mod2->identifier) {
			return 0;
		} else if (mod1->identifier > mod2->identifier) {
			return -1;
		} else if (mod1->identifier < mod2->identifier) {
			return 1;
		}
	} else if (mod1->priority > mod2->priority) {
		return -1;
	} else if (mod1->priority < mod2->priority) {
		return 1;
	}
	
	return 0;
}

static gint mmguicore_module_pairs_sort(gconstpointer a, gconstpointer b)
{
	mmguimodule_t *modpair1, *modpair2;
	
	modpair1 = (mmguimodule_t *)a;
	modpair2 = (mmguimodule_t *)b;
	
	if (modpair1[0]->priority + modpair1[1]->priority == modpair2[0]->priority + modpair2[1]->priority) {
		if (modpair1[0]->identifier + modpair1[1]->identifier == modpair2[0]->identifier + modpair2[1]->identifier) {
			return 0;
		} else if (modpair1[0]->identifier + modpair1[1]->identifier > modpair2[0]->identifier + modpair2[1]->identifier) {
			return -1;
		} else if (modpair1[0]->identifier + modpair1[1]->identifier < modpair2[0]->identifier + modpair2[1]->identifier) {
			return 1;
		}
	} else if (modpair1[0]->priority + modpair1[1]->priority > modpair2[0]->priority + modpair2[1]->priority) {
		return -1;
	} else if (modpair1[0]->priority + modpair1[1]->priority < modpair2[0]->priority + modpair2[1]->priority) {
		return 1;
	}
	
	return 0;
}

static gchar *mmguicore_modules_extract_name(const gchar *filename, gchar *buffer, gsize *bufsize)
{
	gchar *segend, *segstart;
	gsize namelen;
	
	if ((filename == NULL) || (buffer == NULL) || (bufsize == NULL)) return NULL;
		
	segstart = strchr(filename, MMGUI_MODULE_FILE_PREFIX);
	if (segstart != NULL) {
		segend = strstr(segstart + 1, MMGUI_MODULE_FILE_EXTENSION);
		if (segend != NULL) {
			namelen = segend - segstart - 1;
			if (namelen != 0) {
				if (namelen >= *bufsize) {
					namelen = *bufsize - 1;
				}
				
				strncpy(buffer, segstart + 1, namelen);
				buffer[namelen] = '\0';
				
				*bufsize = namelen;
				
				return buffer;
			}
		}
	} 
	
	*bufsize = 0;
	
	return NULL;
}

static gboolean mmguicore_modules_are_compatible(mmguimodule_t module1, mmguimodule_t module2)
{
	gchar **clist1, **clist2;
	gint cseq1, cseq2;
	gboolean found;
	
	if ((module1 == NULL) || (module2 == NULL)) return FALSE;
	if ((module1->servicename[0] == '\0') || (module2->servicename[0] == '\0')) return FALSE;
	if ((module1->compatibility[0] == '\0') || (module2->compatibility[0] == '\0')) return FALSE;
	
	found = FALSE;
	
	/*First find out if the first module compatible with second one*/
	clist1 = g_strsplit(module1->compatibility, ";", -1);
	for (cseq1 = 0; (clist1[cseq1] != NULL) && (clist1[cseq1][0] != '\0') && (!found); cseq1++) {
		if (g_ascii_strcasecmp(clist1[cseq1], module2->servicename) == 0) {
			/*One-way compatibility found, test reverse compatibility*/
			clist2 = g_strsplit(module2->compatibility, ";", -1);
			for (cseq2 = 0; (clist2[cseq2] != NULL) && (clist2[cseq2][0] != '\0'); cseq2++) {
				if (g_ascii_strcasecmp(clist2[cseq2], module1->servicename) == 0) {
					found = TRUE;
					break;
				}
			}
			g_strfreev(clist2);
		}
	}
	g_strfreev(clist1);
	
	return found;
}

static gboolean mmguicore_modules_cache_open(mmguicore_t mmguicore)
{
	gchar *confpath;
	time_t cachetime;
	struct stat statbuf;
	gchar **groups;
	gsize numgroups;
	gint i, filever;
	GError *error;
	
	if (mmguicore == NULL) return FALSE;
	
	/*Do not update cache by default*/
	mmguicore->updatecache = FALSE;
	
	/*Cache file directory*/
	confpath = g_build_path(G_DIR_SEPARATOR_S, g_get_user_cache_dir(), MMGUICORE_CACHE_DIR, NULL);
	/*Recursive directory creation*/
	if (g_mkdir_with_parents(confpath, MMGUICORE_CACHE_PERM) != 0) {
		g_debug("No write access to program cache directory");
		g_free(confpath);
		return FALSE;
	}
	g_free(confpath);
	
	/*Parameters of modules directory*/
	if (stat(RESOURCE_MODULES_DIR, &statbuf) != 0) {
		g_debug("No access to modules directory");
		return FALSE;
	}
	/*Cache file name*/
	mmguicore->cachefilename = g_build_filename(g_get_user_cache_dir(), MMGUICORE_CACHE_DIR, MMGUICORE_CACHE_FILE, NULL);
	/*Cache file object*/
	mmguicore->cachekeyfile = g_key_file_new();
	/*Modules directory modification time */
	mmguicore->updcachetime = statbuf.st_mtime;
	
	error = NULL;
	
	if (!g_key_file_load_from_file(mmguicore->cachekeyfile, mmguicore->cachefilename, G_KEY_FILE_NONE, &error)) {
		mmguicore->updatecache = TRUE;
		g_debug("Local cache file loading error: %s", error->message);
		g_error_free(error);
	} else {
		error = NULL;
		if (g_key_file_has_key(mmguicore->cachekeyfile, MMGUICORE_FILE_ROOT_SECTION, MMGUICORE_FILE_TIMESTAMP, &error)) {
			error = NULL;
			cachetime = (time_t)g_key_file_get_uint64(mmguicore->cachekeyfile, MMGUICORE_FILE_ROOT_SECTION, MMGUICORE_FILE_TIMESTAMP, &error);
			if (error == NULL) {
				if (difftime(cachetime, mmguicore->updcachetime) != 0.0) {
					mmguicore->updatecache = TRUE;
					g_debug("Local cache is outdated");
				} else {
					/*Timestamp is up to date - check version*/
					filever = g_key_file_get_integer(mmguicore->cachekeyfile, MMGUICORE_FILE_ROOT_SECTION, MMGUICORE_FILE_VERSION, &error);
					if (error == NULL) {
						if (filever >= MMGUICORE_CACHE_VER) {
							/*Acceptable version of file*/
							mmguicore->updatecache = FALSE;
							g_debug("Local cache file is up to date");
						} else {
							/*Old version of file*/
							mmguicore->updatecache = TRUE;
							g_debug("Old version of file: %u", filever);
						}
					} else {
						/*Unknown version of file*/
						mmguicore->updatecache = TRUE;
						g_debug("Local cache contains unreadable version identifier: %s", error->message);
						g_error_free(error);
					}
				}
			} else {
				mmguicore->updatecache = TRUE;
				g_debug("Local cache contains unreadable timestamp: %s", error->message);
				g_error_free(error);
			}
		} else {
			mmguicore->updatecache = TRUE;
			g_debug("Local cache does not contain timestamp: %s", error->message);
			g_error_free(error);
		}
	}
	
	/*Cleanup file*/
	if (mmguicore->updatecache) {
		groups = g_key_file_get_groups(mmguicore->cachekeyfile, &numgroups);
		if ((groups != NULL) && (numgroups > 0)) {
			for (i=0; i<numgroups; i++) {
				error = NULL;
				g_key_file_remove_group(mmguicore->cachekeyfile, groups[i], &error);
				if (error != NULL) {
					g_debug("Unable to remove cached module information: %s", error->message);
					g_error_free(error);
				}
			}
			g_strfreev(groups);
		}
	}
	
	return TRUE;
}

static gboolean mmguicore_modules_cache_close(mmguicore_t mmguicore)
{
	gchar *filedata;
	gsize datasize;
	GError *error;
	
	if (mmguicore == NULL) return FALSE;
	if ((mmguicore->cachefilename == NULL) || (mmguicore->cachekeyfile == NULL)) return FALSE;
	
	if (mmguicore->updatecache) {
		/*Save timestamp*/
		g_key_file_set_int64(mmguicore->cachekeyfile, MMGUICORE_FILE_ROOT_SECTION, MMGUICORE_FILE_TIMESTAMP, (gint64)mmguicore->updcachetime);
		/*Save version of file*/
		g_key_file_set_integer(mmguicore->cachekeyfile, MMGUICORE_FILE_ROOT_SECTION, MMGUICORE_FILE_VERSION, MMGUICORE_CACHE_VER);
		/*Write to file*/
		error = NULL;
		filedata = g_key_file_to_data(mmguicore->cachekeyfile, &datasize, &error);
		if (filedata != NULL) {
			if (!g_file_set_contents(mmguicore->cachefilename, filedata, datasize, &error)) {
				g_debug("No data saved to local cache file: %s", error->message);
				g_error_free(error);
			}
			g_free(filedata);
		}
	}
	/*Free resources*/
	g_free(mmguicore->cachefilename);
	g_key_file_free(mmguicore->cachekeyfile);
	
	return TRUE;
}

static gboolean mmguicore_modules_cache_add_module(mmguicore_t mmguicore, mmguimodule_t module)
{
	
	if ((mmguicore == NULL) || (module == NULL)) return FALSE;
	
	if ((!mmguicore->updatecache) || (strlen(module->filename) == 0)) return FALSE;
	
	/*Save main parameters*/
	g_key_file_set_integer(mmguicore->cachekeyfile, module->filename, MMGUICORE_FILE_IDENTIFIER, module->identifier);
	g_key_file_set_integer(mmguicore->cachekeyfile, module->filename, MMGUICORE_FILE_TYPE, module->type);
	g_key_file_set_integer(mmguicore->cachekeyfile, module->filename, MMGUICORE_FILE_REQUIREMENT, module->requirement);
	g_key_file_set_integer(mmguicore->cachekeyfile, module->filename, MMGUICORE_FILE_PRIORITY, module->priority);
	g_key_file_set_integer(mmguicore->cachekeyfile, module->filename, MMGUICORE_FILE_FUNCTIONS, module->functions);
	g_key_file_set_string(mmguicore->cachekeyfile, module->filename, MMGUICORE_FILE_DESCRIPTION, module->description);
	g_key_file_set_string(mmguicore->cachekeyfile, module->filename, MMGUICORE_FILE_SERVICENAME, module->servicename);
	g_key_file_set_string(mmguicore->cachekeyfile, module->filename, MMGUICORE_FILE_SYSTEMDNAME, module->systemdname);
	g_key_file_set_string(mmguicore->cachekeyfile, module->filename, MMGUICORE_FILE_COMPATIBILITY, module->compatibility);
	
	return TRUE;
}

static gint mmguicore_modules_cache_get_enumeration_value(GKeyFile *keyfile, gchar *group, gchar *parameter, gint firstvalue, ...)
{
	va_list args;
	gint cachedvalue, enumvalue, res;
	
	if ((keyfile == NULL) || (group == NULL) || (parameter == NULL)) return -1;
	
	if (!g_key_file_has_key(keyfile, group, parameter, NULL)) {
		g_debug("No parameter '%s' found in cache", parameter);
		return -1;
	}
	
	cachedvalue = g_key_file_get_integer(keyfile, group, parameter, NULL);
	
	if ((firstvalue == -1) || (cachedvalue == firstvalue)) {
		/*No range for output value or first value is used*/
		return cachedvalue;
	}
	
	res = -1;
	
	va_start(args, firstvalue);
	
	enumvalue = va_arg(args, gint);
	
	while (enumvalue != -1) {
		if (enumvalue == cachedvalue) {
			res = enumvalue;
			break;
		} else {
			enumvalue = va_arg(args, gint);
		}
	}
	
	
	if (res == -1) {
		g_debug("Parameter '%s' value is out of range", parameter);
	}
		
	return res;
}

static gchar *mmguicore_modules_cache_get_string_value(GKeyFile *keyfile, gchar *group, gchar *parameter, gchar *target, gsize size)
{
	gchar *cachedvalue, *res;
	
	if ((keyfile == NULL) || (group == NULL) || (parameter == NULL) || (target == NULL) || (size == 0)) return NULL;
	
	if (!g_key_file_has_key(keyfile, group, parameter, NULL)) {
		g_debug("No parameter '%s' found in cache", parameter);
		return NULL;
	}
	
	cachedvalue = g_key_file_get_string(keyfile, group, parameter, NULL);
	
	if (cachedvalue != NULL) {
		res = strncpy(target, cachedvalue, size);
	} else {
		res = NULL;
		g_debug("Parameter '%s' value can't be read", parameter);
	}
	
	return res;
}

static GSList *mmguicore_modules_cache_form_list(mmguicore_t mmguicore)
{
	GSList *modules;
	gchar **groups;
	gsize numgroups;
	gint i;
	gsize length;
	mmguimodule_t modparams;
	
	modules = NULL;
	
	if (mmguicore == NULL) {
		return modules;
	}
	
	if ((mmguicore->cachekeyfile == NULL) || (mmguicore->updatecache)) {
		g_debug("Cache file doesn't contain modules parameters");
		return modules;
	}
	
	/*Cached modules list*/
	groups = g_key_file_get_groups(mmguicore->cachekeyfile, &numgroups);
	if ((groups == NULL) || (numgroups == 0)) {
		g_debug("Cache file doesn't contain groups");
		return modules;
	}
	
	for (i=0; i<numgroups; i++) {
		/*Test file extension and filename length*/
		length = strlen(groups[i]);
		if ((length > 3) && (length < 256) && (groups[i][length-3] == '.') && (groups[i][length-2] == 's') && (groups[i][length-1] == 'o')) {
			modparams = g_new0(struct _mmguimodule, 1);
			/*Module identifier*/
			modparams->identifier = (guint)mmguicore_modules_cache_get_enumeration_value(mmguicore->cachekeyfile, groups[i], MMGUICORE_FILE_IDENTIFIER, -1);
			if ((gint)modparams->identifier == -1) {
				g_free(modparams);
				continue;
			}
			/*Module type*/
			modparams->type = mmguicore_modules_cache_get_enumeration_value(mmguicore->cachekeyfile, groups[i], MMGUICORE_FILE_TYPE, MMGUI_MODULE_TYPE_MODEM_MANAGER, MMGUI_MODULE_TYPE_CONNECTION_MANGER, -1);
			if (modparams->type == -1) {
				g_free(modparams);
				continue;
			}
			/*Module requirement*/
			modparams->requirement = mmguicore_modules_cache_get_enumeration_value(mmguicore->cachekeyfile, groups[i], MMGUICORE_FILE_REQUIREMENT, MMGUI_MODULE_REQUIREMENT_SERVICE, MMGUI_MODULE_REQUIREMENT_FILE, MMGUI_MODULE_REQUIREMENT_NONE, -1);
			if (modparams->requirement == -1) {
				g_free(modparams);
				continue;
			}
			/*Module priority*/
			modparams->priority = mmguicore_modules_cache_get_enumeration_value(mmguicore->cachekeyfile, groups[i], MMGUICORE_FILE_PRIORITY, MMGUI_MODULE_PRIORITY_LOW, MMGUI_MODULE_PRIORITY_NORMAL, MMGUI_MODULE_PRIORITY_RECOMMENDED, -1);
			if (modparams->priority == -1) {
				g_free(modparams);
				continue;
			}
			/*Module functions*/
			modparams->functions = mmguicore_modules_cache_get_enumeration_value(mmguicore->cachekeyfile, groups[i], MMGUICORE_FILE_FUNCTIONS, MMGUI_MODULE_FUNCTION_BASIC, MMGUI_MODULE_FUNCTION_POLKIT_PROTECTION, -1);
			if (modparams->functions == -1) {
				g_free(modparams);
				continue;
			}
			/*Module description*/
			if (mmguicore_modules_cache_get_string_value(mmguicore->cachekeyfile, groups[i], MMGUICORE_FILE_DESCRIPTION, modparams->description, sizeof(modparams->description)) == NULL) {
				g_free(modparams);
				continue;
			}
			/*Module service name*/
			if (mmguicore_modules_cache_get_string_value(mmguicore->cachekeyfile, groups[i], MMGUICORE_FILE_SERVICENAME, modparams->servicename, sizeof(modparams->servicename)) == NULL) {
				g_free(modparams);
				continue;
			}
			/*Module systemd name*/
			if (mmguicore_modules_cache_get_string_value(mmguicore->cachekeyfile, groups[i], MMGUICORE_FILE_SYSTEMDNAME, modparams->systemdname, sizeof(modparams->systemdname)) == NULL) {
				g_free(modparams);
				continue;
			}
			/*Module compatibility string*/
			if (mmguicore_modules_cache_get_string_value(mmguicore->cachekeyfile, groups[i], MMGUICORE_FILE_COMPATIBILITY, modparams->compatibility, sizeof(modparams->compatibility)) == NULL) {
				g_free(modparams);
				continue;
			}
			/*Module filename*/
			strncpy(modparams->filename, groups[i], sizeof(modparams->filename)-1);
			modparams->filename[sizeof(modparams->filename)-1] = '\0';
			/*Add to list*/
			modules = g_slist_prepend(modules, modparams);
		}
	}
	
	if (modules != NULL) {
		modules = g_slist_reverse(modules);
	}
	
	g_strfreev(groups);
	
	return modules;
}

static GSList *mmguicore_modules_dir_form_list(mmguicore_t mmguicore, gboolean updatecache)
{
	GSList *modules;
	GDir *moddir;
	const gchar *modname;
	gsize length;
	gchar *modpath;
	GModule *module;
	mmgui_module_init_func module_init;
	mmguimodule_t modparams;
	
	modules = NULL;
	
	moddir = g_dir_open(RESOURCE_MODULES_DIR, 0, NULL);
	
	if (moddir == NULL) {
		g_debug("Unable to open modules directory");
		return modules;
	}
		
	modname = g_dir_read_name(moddir);
	while (modname != NULL) {
		/*Test file extension and filename length*/
		length = strlen(modname);
		if ((length > 3) && (length < 256) && (modname[length-3] == '.') && (modname[length-2] == 's') && (modname[length-1] == 'o')) {
			/*Full path to module*/
			modpath = g_strconcat(RESOURCE_MODULES_DIR, G_DIR_SEPARATOR_S, modname, NULL);
			/*Test if module exists*/
			if (g_file_test(modpath, G_FILE_TEST_EXISTS)) {
				/*Open module*/
				module = g_module_open(modpath, G_MODULE_BIND_LAZY);
				if (module != NULL) {
					/*Call init function to get module parameters*/
					if (g_module_symbol(module, "mmgui_module_init", (gpointer*)&module_init)) {
						modparams = g_new0(struct _mmguimodule, 1);
						if (module_init(modparams)) {
							/*Add module filename*/
							strncpy(modparams->filename, modname, sizeof(modparams->filename)-1);
							modparams->filename[sizeof(modparams->filename)-1] = '\0';
							/*Module successfully initialized and can be used*/
							modules = g_slist_prepend(modules, modparams);
							/*Add module to cache if needed*/
							if ((mmguicore != NULL) && (updatecache)) {
								mmguicore_modules_cache_add_module(mmguicore, modparams);
							}
						} else {
							/*Unable to initialize module - skip*/
							g_free(modparams);
						}
					}
					/*Close module*/
					g_module_close(module);
				}
			}
			/*Free full path*/
			g_free(modpath);
		}
		/*Next file*/
		modname = g_dir_read_name(moddir);
	}
	
	if (modules != NULL) {
		modules = g_slist_reverse(modules);
	}
	
	/*Close directory*/
	g_dir_close(moddir);
	
	/*Save cache file*/
	if ((mmguicore != NULL) && (updatecache)) {
		mmguicore_modules_cache_close(mmguicore);
	}
	
	return modules;
}

static gboolean mmguicore_modules_enumerate(mmguicore_t mmguicore)
{
	mmguimodule_t mod1, mod2;
	GSList *iter1;
	gsize shortnamelen;
	GHashTable *mmmods, *cmmods;
	GHashTableIter iter2;
	gchar *sn1;
	gchar **comp;
	gint cid;
	mmguimodule_t *modpair;
	gboolean mmavailable, cmavailable, res;
		
	if (mmguicore == NULL) return FALSE;
	
	res = FALSE;
	
	mmguicore->modulepairs = NULL;
	
	/*Form full modules list*/
	if (mmguicore->updatecache) {
		/*Got to get all available modules*/
		mmguicore->modules = mmguicore_modules_dir_form_list(mmguicore, TRUE);
	} else {
		/*Can use cache*/
		mmguicore->modules = mmguicore_modules_cache_form_list(mmguicore);
		/*Fallback if cache is broken*/
		if (mmguicore->modules == NULL) {
			if ((mmguicore->cachefilename != NULL) && (mmguicore->cachekeyfile != NULL)) {
				/*Cache can be updated*/
				mmguicore->updatecache = TRUE;
				mmguicore->modules = mmguicore_modules_dir_form_list(mmguicore, TRUE);
			} else {
				/*Unable to update cache*/
				mmguicore->modules = mmguicore_modules_dir_form_list(mmguicore, FALSE);
			}
		}
	}
	
	/*We are going to check if at least one modem and connection management module is available*/
	mmavailable = FALSE;
	cmavailable = FALSE;
	
	mmmods = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)g_free, NULL);
	cmmods = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)g_free, NULL);
	
	/*Mark available modules*/
	if (mmguicore->modules != NULL) {
		for (iter1 = mmguicore->modules; iter1 != NULL; iter1 = iter1->next) {
			mod1 = iter1->data;
			shortnamelen = sizeof(mod1->shortname);
			if (mmguicore_modules_extract_name(mod1->filename, mod1->shortname, &shortnamelen) != NULL) {
				/*Test service/file existence*/
				mod1->applicable = FALSE;
				mod1->activationtech = MMGUI_SVCMANGER_ACTIVATION_TECH_NA;
				switch (mod1->requirement) {
					case MMGUI_MODULE_REQUIREMENT_SERVICE:
						mod1->applicable = mmgui_svcmanager_get_service_state(mmguicore->svcmanager, mod1->systemdname, mod1->servicename);
						if (!mod1->applicable) {
							mod1->activationtech = mmgui_svcmanager_get_service_activation_tech(mmguicore->svcmanager, mod1->systemdname, mod1->servicename);
						}
						break;
					case MMGUI_MODULE_REQUIREMENT_FILE:
						if (g_file_test(mod1->servicename, G_FILE_TEST_EXISTS)) {
							mod1->applicable = TRUE;
						}
						break;
					case MMGUI_MODULE_REQUIREMENT_NONE:
						mod1->applicable = TRUE;
						break;
					default:
						break;
				}
				/*Set module type available flag*/
				if ((mod1->applicable) || (mod1->activationtech != MMGUI_SVCMANGER_ACTIVATION_TECH_NA)) {
					switch (mod1->type) {
						case MMGUI_MODULE_TYPE_MODEM_MANAGER:
							g_hash_table_insert(mmmods, g_strdup(mod1->servicename), mod1);
							mmavailable = TRUE;
							break;
						case MMGUI_MODULE_TYPE_CONNECTION_MANGER:
							g_hash_table_insert(cmmods, g_strdup(mod1->servicename), mod1);
							cmavailable = TRUE;
							break;
						default:
							break;
					}
				}	
			}
		}
		/*Sort modules*/
		mmguicore->modules = g_slist_sort(mmguicore->modules, mmguicore_modules_sort);
	}
	
	/*Find at least one pair of compatible modules*/
	if (mmavailable && cmavailable) {
		g_hash_table_iter_init(&iter2, mmmods);
		while (g_hash_table_iter_next(&iter2, (gpointer *)&sn1, (gpointer *)&mod1)) {
			comp = g_strsplit(mod1->compatibility, ";", -1);
			if (comp != NULL) {
				for (cid = 0; (comp[cid] != NULL) && (comp[cid][0] != '\0'); cid++) {
					mod2 = g_hash_table_lookup(cmmods, comp[cid]);
					if (mod2 != NULL) {
						if (mmguicore_modules_are_compatible(mod1, mod2)) {
							modpair = g_malloc0(sizeof(mmguimodule_t) * 2);
							modpair[0] = mod1;
							modpair[1] = mod2;
							mmguicore->modulepairs = g_slist_prepend(mmguicore->modulepairs, modpair);
							g_debug("Found pair of modules: %s <---> %s\n", mod1->servicename, mod2->servicename);
							res = TRUE;
						}
					}
				}
				g_strfreev(comp);
			}
		}
	}
	/*Sort module pairs*/
	mmguicore->modulepairs = g_slist_sort(mmguicore->modulepairs, mmguicore_module_pairs_sort);
			
	/*Free resources*/
	g_hash_table_destroy(mmmods);
	g_hash_table_destroy(cmmods);
	
	return res;
} 

static gboolean mmguicore_modules_prepare(mmguicore_t mmguicore)
{
	mmguimodule_t curmod, mmmod, cmmod;
	mmguimodule_t *modpair;
	GSList *iter;
	gboolean pairfound;
	
	if (mmguicore == NULL) return FALSE;
	if (mmguicore->modules == NULL) return FALSE;
	
	mmmod = NULL;
	cmmod = NULL;
	pairfound = TRUE;
	
	/*Search for modules selected by user*/
	for (iter = mmguicore->modules; iter != NULL; iter = iter->next) {
		curmod = iter->data;
		switch (curmod->type) {
			case MMGUI_MODULE_TYPE_MODEM_MANAGER:
				if (mmguicore->options != NULL) {
					if (mmguicore->options->mmmodule != NULL) {
						if (g_str_equal(curmod->shortname, mmguicore->options->mmmodule)) {
							mmmod = curmod;
						}
					}
				}
			case MMGUI_MODULE_TYPE_CONNECTION_MANGER:
				if (mmguicore->options != NULL) {
					if (mmguicore->options->cmmodule != NULL) {
						if (g_str_equal(curmod->shortname, mmguicore->options->cmmodule)) {
							cmmod = curmod;
						}
					}
				}
				break;
			default:
				break;
		}
	}
	
	/*If modules are not compatible, use failsafe ones*/
	if (!mmguicore_modules_are_compatible(mmmod, cmmod)) {
		pairfound = FALSE;
		for (iter = mmguicore->modulepairs; iter != NULL; iter = iter->next) {
			modpair = (mmguimodule_t *)iter->data;
			if ((mmmod != NULL) && (cmmod != NULL)) {
				/*Selected modules arent compatible - find connection manager*/
				curmod = modpair[0];
				if (g_ascii_strcasecmp(mmmod->servicename, curmod->servicename) == 0) {
					cmmod = modpair[1];
					pairfound = TRUE;
					g_debug("Modules are not compatible, using safe combination: %s <---> %s\n", mmmod->servicename, cmmod->servicename);
					break;
				}
			} else if ((mmmod == NULL) && (cmmod != NULL)) {
				/*Connection manager not selected - select one*/
				curmod = modpair[1];
				if (g_ascii_strcasecmp(cmmod->servicename, curmod->servicename) == 0) {
					mmmod = modpair[0];
					pairfound = TRUE;
					g_debug("Modem manager not available, using safe combination: %s <---> %s\n", mmmod->servicename, cmmod->servicename);
					break;
				}
			} else if ((mmmod != NULL) && (cmmod == NULL)) {
				/*Modem manager not selected - select one*/
				curmod = modpair[0];
				if (g_ascii_strcasecmp(mmmod->servicename, curmod->servicename) == 0) {
					cmmod = modpair[1];
					pairfound = TRUE;
					g_debug("Connection manager not available, using safe combination: %s <---> %s\n", mmmod->servicename, cmmod->servicename);
					break;
				}
			} else {
				/*No module selected - take first pair*/
				mmmod = modpair[0];
				cmmod = modpair[1];
				pairfound = TRUE;
				g_debug("Selected pair of modules is not available, using safe combination: %s <---> %s\n", mmmod->servicename, cmmod->servicename);
				break;
			}
		}
	}
	
	/*If pair is still not found, select first one*/
	if (!pairfound) {
		modpair = (mmguimodule_t *)g_slist_nth_data(mmguicore->modulepairs, 0);
		mmmod = modpair[0];
		cmmod = modpair[1];
		g_debug("Selected pair of modules is not available, using safe combination: %s <---> %s\n", mmmod->servicename, cmmod->servicename);
	}
	
	/*Mark modules as recommended and schedule activation*/
	mmmod->recommended = TRUE;
	if ((!mmmod->applicable) && (mmmod->activationtech != MMGUI_SVCMANGER_ACTIVATION_TECH_NA) && (mmmod->requirement == MMGUI_MODULE_REQUIREMENT_SERVICE)) {
		mmgui_svcmanager_schedule_start_service(mmguicore->svcmanager, mmmod->systemdname, mmmod->servicename, mmmod->shortname, mmguicore->options->enableservices);
	}
	cmmod->recommended = TRUE;
	if ((!cmmod->applicable) && (cmmod->activationtech != MMGUI_SVCMANGER_ACTIVATION_TECH_NA) && (cmmod->requirement == MMGUI_MODULE_REQUIREMENT_SERVICE)) {
		mmgui_svcmanager_schedule_start_service(mmguicore->svcmanager, cmmod->systemdname, cmmod->servicename, cmmod->shortname, mmguicore->options->enableservices);
	}
	
	/*Initiate services activation*/
	mmgui_svcmanager_start_services_activation(mmguicore->svcmanager);
	
	return TRUE;	
}

GSList *mmguicore_modules_get_list(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return NULL;
	
	return mmguicore->modules;
}

static gboolean mmguicore_modules_free(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return FALSE;
	
	/*Free pairs list*/
	if (mmguicore->modulepairs != NULL) {
		g_slist_foreach(mmguicore->modulepairs,(GFunc)g_free, NULL);
		g_slist_free(mmguicore->modulepairs);
	}
	/*Free modules list*/
	if (mmguicore->modules != NULL) {
		g_slist_free(mmguicore->modules);
	}
	
	return TRUE;
}

static gboolean mmguicore_modules_mm_open(mmguicore_t mmguicore, mmguimodule_t mmguimodule)
{
	gboolean openstatus; 
	gchar *modulepath;
	gchar *polkitaction;
		
	if ((mmguicore == NULL) || (mmguimodule == NULL)) return FALSE;
	
	/*Wrong module type*/
	if (mmguimodule->type == MMGUI_MODULE_TYPE_CONNECTION_MANGER) return FALSE;
	
	/*Authentication process*/
	if (mmguimodule->functions & MMGUI_MODULE_FUNCTION_POLKIT_PROTECTION) {
		openstatus = TRUE;
		/*Autheticate user if needed*/
		polkitaction = g_strdup_printf("ru.linuxonly.modem-manager-gui.manage-modem-%s", mmguimodule->shortname);
		if (mmgui_polkit_action_needed(mmguicore->polkit, polkitaction, FALSE)) {
			openstatus = mmgui_polkit_request_password(mmguicore->polkit, polkitaction);
		}
		g_free(polkitaction);
		/*Show warning if not authenticated*/
		if (!openstatus) {
			g_debug("User not authenticated for modem manager usage\n");
		}
	}
	/*Dynamic loader*/
	modulepath = g_strconcat(RESOURCE_MODULES_DIR, G_DIR_SEPARATOR_S, mmguimodule->filename, NULL);
	mmguicore->module = g_module_open(modulepath, G_MODULE_BIND_LAZY);
	if (mmguicore->module != NULL) {
		openstatus = TRUE;
		/*Module function pointers*/
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_open", (gpointer *)&(mmguicore->open_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_close", (gpointer *)&(mmguicore->close_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_last_error", (gpointer *)&(mmguicore->last_error_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_interrupt_operation", (gpointer *)&(mmguicore->interrupt_operation_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_set_timeout", (gpointer *)&(mmguicore->set_timeout_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_devices_enum", (gpointer *)&(mmguicore->devices_enum_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_devices_open", (gpointer *)&(mmguicore->devices_open_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_devices_close", (gpointer *)&(mmguicore->devices_close_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_devices_state", (gpointer *)&(mmguicore->devices_state_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_devices_update_state", (gpointer *)&(mmguicore->devices_update_state_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_devices_information", (gpointer *)&(mmguicore->devices_information_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_devices_enable", (gpointer *)&(mmguicore->devices_enable_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_devices_unlock_with_pin", (gpointer *)&(mmguicore->devices_unlock_with_pin_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_sms_enum", (gpointer *)&(mmguicore->sms_enum_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_sms_get", (gpointer *)&(mmguicore->sms_get_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_sms_delete", (gpointer *)&(mmguicore->sms_delete_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_sms_send", (gpointer *)&(mmguicore->sms_send_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_ussd_cancel_session", (gpointer *)&(mmguicore->ussd_cancel_session_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_ussd_get_state", (gpointer *)&(mmguicore->ussd_get_state_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_ussd_send", (gpointer *)&(mmguicore->ussd_send_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_networks_scan", (gpointer *)&(mmguicore->networks_scan_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_contacts_enum", (gpointer *)&(mmguicore->contacts_enum_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_contacts_delete", (gpointer *)&(mmguicore->contacts_delete_func));
		openstatus = openstatus && g_module_symbol(mmguicore->module, "mmgui_module_contacts_add", (gpointer *)&(mmguicore->contacts_add_func));
		if (!openstatus) {
			/*Module function pointers*/
			mmguicore->open_func = NULL;
			mmguicore->close_func = NULL;
			mmguicore->last_error_func = NULL;
			mmguicore->interrupt_operation_func = NULL;
			mmguicore->set_timeout_func = NULL;
			mmguicore->devices_enum_func = NULL;
			mmguicore->devices_open_func = NULL;
			mmguicore->devices_close_func = NULL;
			mmguicore->devices_state_func = NULL;
			mmguicore->devices_update_state_func = NULL;
			mmguicore->devices_information_func = NULL;
			mmguicore->devices_enable_func = NULL;
			mmguicore->devices_unlock_with_pin_func = NULL;
			mmguicore->sms_enum_func = NULL;
			mmguicore->sms_get_func = NULL;
			mmguicore->sms_delete_func = NULL;
			mmguicore->sms_send_func = NULL;
			mmguicore->ussd_cancel_session_func = NULL;
			mmguicore->ussd_get_state_func = NULL;
			mmguicore->ussd_send_func = NULL;
			mmguicore->networks_scan_func = NULL;
			mmguicore->contacts_enum_func = NULL;
			mmguicore->contacts_delete_func = NULL;
			mmguicore->contacts_add_func = NULL;
			g_module_close(mmguicore->module);
			g_printf("Failed to load modem manager: %s\n", mmguimodule->description);
		}
	} else {
		openstatus = FALSE;
	}
	g_free(modulepath);
	
	/*Module preparation*/
	if (openstatus) {
		mmguicore->moduleptr = mmguimodule;
		/*Open module*/
		(mmguicore->open_func)(mmguicore);
		/*Set module timeouts*/
		if (mmguicore->options != NULL) {
			mmguicore_modules_mm_set_timeouts(mmguicore, MMGUI_DEVICE_OPERATION_ENABLE, mmguicore->options->enabletimeout,
														MMGUI_DEVICE_OPERATION_SEND_SMS, mmguicore->options->sendsmstimeout,
														MMGUI_DEVICE_OPERATION_SEND_USSD, mmguicore->options->sendussdtimeout,
														MMGUI_DEVICE_OPERATION_SCAN, mmguicore->options->scannetworkstimeout,
														-1);
		}
		g_printf("Modem manager: %s\n", mmguimodule->description);
	} else {
		mmguicore->moduleptr = NULL;
		mmguicore->module = NULL;
	}
	
	return openstatus;
}

static gboolean mmguicore_modules_cm_open(mmguicore_t mmguicore, mmguimodule_t mmguimodule)
{
	gboolean openstatus; 
	gchar *modulepath;
	gchar *polkitaction;
		
	if ((mmguicore == NULL) || (mmguimodule == NULL)) return FALSE;
	
	/*Wrong module type*/
	if (mmguimodule->type == MMGUI_MODULE_TYPE_MODEM_MANAGER) return FALSE;
	/*Authentication process*/
	if (mmguimodule->functions & MMGUI_MODULE_FUNCTION_POLKIT_PROTECTION) {
		openstatus = TRUE;
		/*Autheticate user if needed*/
		polkitaction = g_strdup_printf("ru.linuxonly.modem-manager-gui.manage-network-%s", mmguimodule->shortname);
		if (mmgui_polkit_action_needed(mmguicore->polkit, polkitaction, FALSE)) {
			openstatus = mmgui_polkit_request_password(mmguicore->polkit, polkitaction);
		}
		g_free(polkitaction);
		/*Show warning if not authenticated*/
		if (!openstatus) {
			g_debug("User not authenticated for connection manager usage\n");
		}
	}
	/*Dynamic loader*/
	modulepath = g_strconcat(RESOURCE_MODULES_DIR, G_DIR_SEPARATOR_S, mmguimodule->filename, NULL);
	mmguicore->cmodule = g_module_open(modulepath, G_MODULE_BIND_LAZY);
	if (mmguicore->cmodule != NULL) {
		openstatus = TRUE;
		/*Module function pointers*/
		openstatus = openstatus && g_module_symbol(mmguicore->cmodule, "mmgui_module_connection_open", (gpointer *)&(mmguicore->connection_open_func));
		openstatus = openstatus && g_module_symbol(mmguicore->cmodule, "mmgui_module_connection_close", (gpointer *)&(mmguicore->connection_close_func));
		openstatus = openstatus && g_module_symbol(mmguicore->cmodule, "mmgui_module_connection_enum", (gpointer *)&(mmguicore->connection_enum_func));
		openstatus = openstatus && g_module_symbol(mmguicore->cmodule, "mmgui_module_connection_add", (gpointer *)&(mmguicore->connection_add_func));
		openstatus = openstatus && g_module_symbol(mmguicore->cmodule, "mmgui_module_connection_update", (gpointer *)&(mmguicore->connection_update_func));
		openstatus = openstatus && g_module_symbol(mmguicore->cmodule, "mmgui_module_connection_remove", (gpointer *)&(mmguicore->connection_remove_func));
		openstatus = openstatus && g_module_symbol(mmguicore->cmodule, "mmgui_module_connection_last_error", (gpointer *)&(mmguicore->connection_last_error_func));
		openstatus = openstatus && g_module_symbol(mmguicore->cmodule, "mmgui_module_device_connection_open", (gpointer *)&(mmguicore->device_connection_open_func));
		openstatus = openstatus && g_module_symbol(mmguicore->cmodule, "mmgui_module_device_connection_close", (gpointer *)&(mmguicore->device_connection_close_func));
		openstatus = openstatus && g_module_symbol(mmguicore->cmodule, "mmgui_module_device_connection_status", (gpointer *)&(mmguicore->device_connection_status_func));
		openstatus = openstatus && g_module_symbol(mmguicore->cmodule, "mmgui_module_device_connection_timestamp", (gpointer *)&(mmguicore->device_connection_timestamp_func));
		openstatus = openstatus && g_module_symbol(mmguicore->cmodule, "mmgui_module_device_connection_get_active_uuid", (gpointer *)&(mmguicore->device_connection_get_active_uuid_func));
		openstatus = openstatus && g_module_symbol(mmguicore->cmodule, "mmgui_module_device_connection_connect", (gpointer *)&(mmguicore->device_connection_connect_func));
		openstatus = openstatus && g_module_symbol(mmguicore->cmodule, "mmgui_module_device_connection_disconnect", (gpointer *)&(mmguicore->device_connection_disconnect_func));
		
		if (!openstatus) {
			/*Module function pointers*/
			mmguicore->connection_open_func = NULL;
			mmguicore->connection_close_func = NULL;
			mmguicore->connection_enum_func = NULL;
			mmguicore->connection_add_func = NULL;
			mmguicore->connection_update_func = NULL;
			mmguicore->connection_remove_func = NULL;
			mmguicore->connection_last_error_func = NULL;
			mmguicore->device_connection_open_func = NULL;
			mmguicore->device_connection_close_func = NULL;
			mmguicore->device_connection_status_func = NULL;
			mmguicore->device_connection_timestamp_func = NULL;
			mmguicore->device_connection_get_active_uuid_func = NULL;
			mmguicore->device_connection_connect_func = NULL;
			mmguicore->device_connection_disconnect_func = NULL;
			
			g_module_close(mmguicore->cmodule);
		}
	} else {
		openstatus = FALSE;
	}
	g_free(modulepath);
	
	if (openstatus) {
		mmguicore->cmoduleptr = mmguimodule;
		(mmguicore->connection_open_func)(mmguicore);
		g_printf("Connection manager: %s\n", mmguimodule->description);
	} else {
		mmguicore->cmoduleptr = NULL;
		mmguicore->cmodule = NULL;
	}
	
	return openstatus;
}

static gboolean mmguicore_modules_close(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return FALSE;
	
	/*Modem manager module functions*/
	if (mmguicore->module != NULL) {
		if (mmguicore->close_func != NULL) {
			mmguicore->close_func(mmguicore);
		}
		g_module_close(mmguicore->module);
		mmguicore->module = NULL;
		/*Module function pointers*/
		mmguicore->open_func = NULL;
		mmguicore->close_func = NULL;
		mmguicore->last_error_func = NULL;
		mmguicore->devices_enum_func = NULL;
		mmguicore->devices_open_func = NULL;
		mmguicore->devices_close_func = NULL;
		mmguicore->devices_state_func = NULL;
		mmguicore->devices_update_state_func = NULL;
		mmguicore->devices_information_func = NULL;
		mmguicore->devices_enable_func = NULL;
		mmguicore->devices_unlock_with_pin_func = NULL;
		mmguicore->sms_enum_func = NULL;
		mmguicore->sms_get_func = NULL;
		mmguicore->sms_delete_func = NULL;
		mmguicore->sms_send_func = NULL;
		mmguicore->ussd_cancel_session_func = NULL;
		mmguicore->ussd_get_state_func = NULL;
		mmguicore->ussd_send_func = NULL;
		mmguicore->networks_scan_func = NULL;
		mmguicore->contacts_enum_func = NULL;
		mmguicore->contacts_delete_func = NULL;
		mmguicore->contacts_add_func = NULL;
		
		mmguicore->moduleptr = NULL;
	}
	
	/*Connection manager module functions*/
	if (mmguicore->cmodule != NULL) {
		if (mmguicore->connection_close_func != NULL) {
			mmguicore->connection_close_func(mmguicore);
		}
		g_module_close(mmguicore->cmodule);
		mmguicore->cmodule = NULL;
		/*Module function pointers*/
		mmguicore->connection_open_func = NULL;
		mmguicore->connection_close_func = NULL;
		mmguicore->connection_enum_func = NULL;
		mmguicore->connection_add_func = NULL;
		mmguicore->connection_update_func = NULL;
		mmguicore->connection_remove_func = NULL;
		mmguicore->connection_last_error_func = NULL;
		mmguicore->device_connection_open_func = NULL;
		mmguicore->device_connection_close_func = NULL;
		mmguicore->device_connection_status_func = NULL;
		mmguicore->device_connection_timestamp_func = NULL;
		mmguicore->device_connection_get_active_uuid_func = NULL;
		mmguicore->device_connection_connect_func = NULL;
		mmguicore->device_connection_disconnect_func = NULL;
		
		mmguicore->cmoduleptr = NULL;
	}
	
	return TRUE;
}

static gboolean mmguicore_modules_select(mmguicore_t mmguicore)
{
	GSList *iterator;
	mmguimodule_t module, mmmod, cmmod;
	mmguimodule_t *modpair;
	gboolean mmfound, cmfound;
	
	if (mmguicore == NULL) return FALSE;
	if (mmguicore->modules == NULL) return FALSE;
	
	if ((mmguicore->module != NULL) || (mmguicore->cmodule != NULL)) {
		mmguicore_modules_close(mmguicore);
	}
	
	mmfound = FALSE;
	cmfound = FALSE;
		
	/*Try to open recommended modules first*/
	for (iterator=mmguicore->modules; iterator; iterator=iterator->next) {
		module = iterator->data;
		if ((module->applicable) && (module->recommended)) {
			if ((module->type == MMGUI_MODULE_TYPE_MODEM_MANAGER) && (!mmfound)) {
				mmfound = mmguicore_modules_mm_open(mmguicore, module);
			} else if ((module->type == MMGUI_MODULE_TYPE_CONNECTION_MANGER) && (!cmfound)) {
				cmfound = mmguicore_modules_cm_open(mmguicore, module);
			}
		}
		if ((mmfound) && (cmfound)) {
			break;
		}
	}
	
	/*If modules not opened use full list*/
	if (!((mmfound) && (cmfound))) {
		if ((!mmfound) && (cmfound)) {
			/*Find compatible modem manager module and open it*/
			for (iterator=mmguicore->modulepairs; iterator; iterator=iterator->next) {
				modpair = (mmguimodule_t *)iterator->data;
				mmmod = modpair[0];
				cmmod = modpair[1];
				module = mmguicore->cmoduleptr;
				if ((g_ascii_strcasecmp(module->servicename, cmmod->servicename) == 0) && (mmmod->applicable)) {
					mmfound = mmguicore_modules_mm_open(mmguicore, mmmod);
					if (mmfound) {
						break;
					}
				}
			}
		} else if ((mmfound) && (!cmfound)) {
			/*Find compatible connection manager module and open it*/
			for (iterator=mmguicore->modulepairs; iterator; iterator=iterator->next) {
				modpair = (mmguimodule_t *)iterator->data;
				mmmod = modpair[0];
				cmmod = modpair[1];
				module = mmguicore->moduleptr;
				if ((g_ascii_strcasecmp(module->servicename, mmmod->servicename) == 0) && (cmmod->applicable)) {
					cmfound = mmguicore_modules_cm_open(mmguicore, cmmod);
					if (cmfound) {
						break;
					}
				}
			}
		} else if ((!mmfound) && (!cmfound)) {
			/*Find compatible pair of modules and open it*/
			for (iterator=mmguicore->modulepairs; iterator; iterator=iterator->next) {
				modpair = (mmguimodule_t *)iterator->data;
				mmfound = mmguicore_modules_mm_open(mmguicore, modpair[0]);
				cmfound = mmguicore_modules_cm_open(mmguicore, modpair[1]);
				if ((mmfound) && (cmfound)) {
					break;
				}
			}
		}
	}
		
	return ((mmfound) && (cmfound));
}

void mmguicore_modules_mm_set_timeouts(mmguicore_t mmguicore, gint operation1, gint timeout1, ...)
{
	va_list args;
	gint operation, timeout;
	
	if (mmguicore == NULL) return;
	if ((operation1 == -1) || (timeout1 == -1)) return;
	if ((mmguicore->moduleptr == NULL) || (mmguicore->set_timeout_func == NULL)) return;
		
	va_start(args, timeout1);
	
	operation = operation1;
	timeout = timeout1;
	
	while (TRUE) {
		/*Set timeout value*/
		switch (operation) {
			case MMGUI_DEVICE_OPERATION_ENABLE:
			case MMGUI_DEVICE_OPERATION_SEND_SMS:
			case MMGUI_DEVICE_OPERATION_SEND_USSD:
			case MMGUI_DEVICE_OPERATION_SCAN:
				(mmguicore->set_timeout_func)(mmguicore, operation, timeout);
				break;
			default:
				break;
		}
		/*Get new values*/		
		operation = va_arg(args, gint);
		if (operation == -1) {
			break;
		}
		timeout = va_arg(args, gint);
		if (timeout == -1) {
			break;
		}
	}
	
	va_end(args);
}


static gint mmguicore_connections_compare(gconstpointer a, gconstpointer b)
{
	mmguiconn_t connection;
	const gchar *uuid;
	
	connection = (mmguiconn_t)a;
	uuid = (const gchar *)b;
	
	return g_strcmp0(connection->uuid, uuid);
}

static gint mmguicore_connections_name_compare(gconstpointer a, gconstpointer b)
{
	mmguiconn_t connection1, connection2;
	
	connection1 = (mmguiconn_t)a;
	connection2 = (mmguiconn_t)b;
	
	return g_strcmp0(connection1->name, connection2->name);
}

static void mmguicore_connections_free_single(mmguiconn_t connection)
{
	if (connection != NULL) return;
	
	if (connection->uuid != NULL) {
		g_free(connection->uuid);
	}
	if (connection->name != NULL) {
		g_free(connection->name);
	}
	if (connection->number != NULL) {
		g_free(connection->number);
	}
	if (connection->username != NULL) {
		g_free(connection->username);
	}
	if (connection->password != NULL) {
		g_free(connection->password);
	}
	if (connection->apn != NULL) {
		g_free(connection->apn);
	}
	if (connection->dns1 != NULL) {
		g_free(connection->dns1);
	}
	if (connection->dns2 != NULL) {
		g_free(connection->dns2);
	}
}

static void mmguicore_connections_free_foreach(gpointer data, gpointer user_data)
{
	mmguiconn_t connection;
	
	if (data != NULL) return;
	
	connection = (mmguiconn_t)data;
	
	mmguicore_connections_free_single(connection);
}

static void mmguicore_connections_free(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return;
	
	g_slist_foreach(mmguicore->connections, mmguicore_connections_free_foreach, NULL);
		
	g_slist_free(mmguicore->connections);
	
	mmguicore->connections = NULL;
}

gboolean mmguicore_connections_enum(mmguicore_t mmguicore)
{
	if ((mmguicore == NULL) || (mmguicore->connection_enum_func == NULL)) return FALSE;
		
	if (mmguicore->connections != NULL) {
		mmguicore_connections_free(mmguicore);
	}
	
	(mmguicore->connection_enum_func)(mmguicore, &(mmguicore->connections));
	
	if (mmguicore->connections != NULL) {
		mmguicore->connections = g_slist_sort(mmguicore->connections, mmguicore_connections_name_compare);
	}
	
	return TRUE;
}

GSList *mmguicore_connections_get_list(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return NULL;
	
	return mmguicore->connections;
}

mmguiconn_t mmguicore_connections_add(mmguicore_t mmguicore, const gchar *name, const gchar *number, const gchar *username, const gchar *password, const gchar *apn, guint networkid, guint type, gboolean homeonly, const gchar *dns1, const gchar *dns2)
{
	mmguiconn_t connection;
	
	if ((mmguicore == NULL) || (name == NULL) || (mmguicore->connection_add_func == NULL)) return NULL;
	
	connection = (mmguicore->connection_add_func)(mmguicore, name, number, username, password, apn, networkid, type, homeonly, dns1, dns2);
	
	if (connection != NULL) {
		mmguicore->connections = g_slist_append(mmguicore->connections, connection);
	}
	
	return connection;
}

gboolean mmguicore_connections_update(mmguicore_t mmguicore, const gchar *uuid, const gchar *name, const gchar *number, const gchar *username, const gchar *password, const gchar *apn, guint networkid, gboolean homeonly, const gchar *dns1, const gchar *dns2)
{
	GSList *connptr;
	mmguiconn_t connection;
	
	if ((mmguicore == NULL) || (uuid == NULL) || (name == NULL) || (mmguicore->connection_update_func == NULL)) return FALSE;
	
	connptr = g_slist_find_custom(mmguicore->connections, uuid, mmguicore_connections_compare);
	if (connptr != NULL) {
		connection = connptr->data;
		if ((mmguicore->connection_update_func)(mmguicore, connection, name, number, username, password, apn, networkid, homeonly, dns1, dns2)) {
			return TRUE;
		}
	}
	
	return FALSE;
}

gboolean mmguicore_connections_remove(mmguicore_t mmguicore, const gchar *uuid)
{
	GSList *connptr;
	mmguiconn_t connection;
	
	if ((mmguicore == NULL) || (uuid == NULL) || (mmguicore->connection_remove_func == NULL)) return FALSE;
	
	connptr = g_slist_find_custom(mmguicore->connections, uuid, mmguicore_connections_compare);
	if (connptr != NULL) {
		connection = connptr->data;
		if ((mmguicore->connection_remove_func)(mmguicore, connection)) {
			mmguicore_connections_free_single(connection);
			mmguicore->connections = g_slist_remove(mmguicore->connections, connection);
			return TRUE;
		}
	}
	
	return TRUE;
}

gchar *mmguicore_connections_get_active_uuid(mmguicore_t mmguicore)
{
	if ((mmguicore == NULL) || (mmguicore->device_connection_get_active_uuid_func == NULL)) return NULL;
	
	
	return (mmguicore->device_connection_get_active_uuid_func)(mmguicore);
}

gboolean mmguicore_connections_connect(mmguicore_t mmguicore, const gchar *uuid)
{
	GSList *connptr;
	mmguiconn_t connection;
	
	if ((mmguicore == NULL) || (uuid == NULL) || (mmguicore->device_connection_connect_func == NULL)) return FALSE;
	
	connptr = g_slist_find_custom(mmguicore->connections, uuid, mmguicore_connections_compare);
	if (connptr != NULL) {
		connection = connptr->data;
		if ((mmguicore->device_connection_connect_func)(mmguicore, connection)) {
			return TRUE;
		}
	}
	
	return FALSE;
}

gboolean mmguicore_connections_disconnect(mmguicore_t mmguicore)
{
	if ((mmguicore == NULL) || (mmguicore->device_connection_disconnect_func == NULL)) return FALSE;
	
	if ((mmguicore->device != NULL) && (mmguicore->device->connected)) {
		if ((mmguicore->device_connection_disconnect_func)(mmguicore)) {
			return TRUE;
		}
	}
	
	return FALSE;
}

guint mmguicore_connections_get_capabilities(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return MMGUI_CONNECTION_MANAGER_CAPS_MANAGEMENT;
			
	return mmguicore->cmcaps;
}

gboolean mmguicore_connections_get_transition_flag(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return FALSE;
	
	if (mmguicore->device != NULL) {
		return mmguicore->device->conntransition;
	}
			
	return FALSE;
}

static void mmguicore_devices_free_single(mmguidevice_t device)
{
	if (device != NULL) return;
	
	if (device->manufacturer != NULL) {
		g_free(device->manufacturer);
	}
	if (device->model != NULL) {
		g_free(device->model);
	}
	if (device->version != NULL) {
		g_free(device->version);
	}
	if (device->port != NULL) {
		g_free(device->port);
	}
	if (device->objectpath != NULL) {
		g_free(device->objectpath);
	}
	if (device->sysfspath != NULL) {
		g_free(device->sysfspath);
	}
	if (device->internalid != NULL) {
		g_free(device->internalid);
	}
	if (device->persistentid != NULL) {
		g_free(device->persistentid);
	}
	if (device->imei != NULL) {
		g_free(device->imei);
	}
	if (device->imsi != NULL) {
		g_free(device->imsi);
	}
}

static void mmguicore_devices_free_foreach(gpointer data, gpointer user_data)
{
	mmguidevice_t device;
	
	if (data != NULL) return;
	
	device = (mmguidevice_t)data;
	
	mmguicore_devices_free_single(device);
}

static void mmguicore_devices_free(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return;
	
	g_slist_foreach(mmguicore->devices, mmguicore_devices_free_foreach, NULL);
		
	g_slist_free(mmguicore->devices);
	
	mmguicore->devices = NULL;
}

gboolean mmguicore_devices_enum(mmguicore_t mmguicore)
{
	GSList *iterator;
	mmguidevice_t device;
	
	if ((mmguicore == NULL) || (mmguicore->devices_enum_func == NULL)) return FALSE;
	
	if (mmguicore->devices != NULL) {
		mmguicore_devices_free(mmguicore);
	}
	
	(mmguicore->devices_enum_func)(mmguicore, &(mmguicore->devices));
	
	for (iterator=mmguicore->devices; iterator; iterator=iterator->next) {
		device = iterator->data;
		g_debug("Device: %s, %s, %s [%u] [%s]\n", device->manufacturer, device->model, device->version, device->id, device->persistentid);
	}
	
	return TRUE;
}

static gboolean mmguicore_devices_add(mmguicore_t mmguicore, mmguidevice_t device)
{
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	
	mmguicore->devices = g_slist_append(mmguicore->devices, device);
	
	g_debug("Device successfully added\n");
	
	return TRUE;
}

static gint mmguicore_devices_remove_compare(gconstpointer a, gconstpointer b)
{
	mmguidevice_t device;
	guint id;
	
	device = (mmguidevice_t)a;
	id = GPOINTER_TO_UINT(b);
	
	if (device->id < id) {
		return 1;
	} else if (device->id > id) {
		return -1;
	} else {
		return 0;
	}
}

static gboolean mmguicore_devices_remove(mmguicore_t mmguicore, guint deviceid)
{
	GSList *deviceptr;
	
	if (mmguicore == NULL) return FALSE;
	if (mmguicore->devices == NULL) return FALSE;
	
	deviceptr = g_slist_find_custom(mmguicore->devices, GUINT_TO_POINTER(deviceid), mmguicore_devices_remove_compare);
	
	if (deviceptr == NULL) return FALSE;
	
	if (mmguicore->device != NULL) {
		if (mmguicore->device->id == deviceid) {
			/*Close currently opened device*/
			mmguicore_devices_close(mmguicore);
		}
	}
	/*Remove device structure from list*/
	mmguicore->devices = g_slist_remove(mmguicore->devices, deviceptr->data);
	/*Free device structure*/
	mmguicore_devices_free_single(deviceptr->data);
	
	g_debug("Device successfully removed\n");
	
	return TRUE;
}

static gint mmguicore_devices_open_compare(gconstpointer a, gconstpointer b)
{
	mmguidevice_t device;
	guint id;
	
	device = (mmguidevice_t)a;
	id = GPOINTER_TO_UINT(b);
	
	if (device->id < id) {
		return 1;
	} else if (device->id > id) {
		return -1;
	} else {
		return 0;
	}
}

gboolean mmguicore_devices_open(mmguicore_t mmguicore, guint deviceid, gboolean openfirst)
{
	GSList *deviceptr;
	guint workthreadcmd;
	
	if (mmguicore == NULL) return FALSE;
	if ((mmguicore->devices == NULL) || (mmguicore->devices_open_func == NULL)) return FALSE;
	
	deviceptr = g_slist_find_custom(mmguicore->devices, GUINT_TO_POINTER(deviceid), mmguicore_devices_open_compare);
	
	if (deviceptr != NULL) {
		/*Test currently opened device*/
		if (mmguicore->device != NULL) {
			if (mmguicore->device->id == deviceid) {
				/*Already opened*/
				return TRUE;
			} else {
				/*Close currently opened device*/
				mmguicore_devices_close(mmguicore);
			}
		}
		if ((mmguicore->devices_open_func)(mmguicore, deviceptr->data)) {
			mmguicore->device = deviceptr->data;
			/*Update device information*/
			if (mmguicore->devices_information_func != NULL) {
				(mmguicore->devices_information_func)(mmguicore);
				/*Open SMS database*/
				mmguicore->device->smsdb = mmgui_smsdb_open(mmguicore->device->persistentid, mmguicore->device->internalid);
				/*Open traffic database*/
				mmguicore->device->trafficdb = mmgui_trafficdb_open(mmguicore->device->persistentid, mmguicore->device->internalid);
				/*Open contacts*/
				mmguicore_contacts_enum(mmguicore);
				/*For Huawei modem USSD answers must be converted*/
				if (g_ascii_strcasecmp(mmguicore->device->manufacturer, "huawei") == 0) {
					mmguicore->device->ussdencoding = MMGUI_USSD_ENCODING_UCS2;
				} else {
					mmguicore->device->ussdencoding = MMGUI_USSD_ENCODING_GSM7;
				}
			}
			/*Open connection statistics source*/
			if (mmguicore->device_connection_open_func != NULL) {
				(mmguicore->device_connection_open_func)(mmguicore, deviceptr->data);
			}
			/*Update connection parameters*/
			if (mmguicore->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MONITORING) {
				/*Update connection parameters in main thread*/
				mmguicore_update_connection_status(mmguicore, FALSE, FALSE);
			} else {
				/*Send command to refresh connection state in work thread*/
				workthreadcmd = MMGUI_THREAD_REFRESH_CMD;
				if (write(mmguicore->workthreadctl[1], &workthreadcmd, sizeof(workthreadcmd)) != sizeof(workthreadcmd)) {
					g_debug("Unable to send refresh command\n");
				}
			}
			/*Generate event*/
			if (mmguicore->extcb != NULL) {
				(mmguicore->extcb)(MMGUI_EVENT_DEVICE_OPENED, mmguicore, mmguicore->device, mmguicore->userdata);
			}
			
			g_debug("Device successfully opened\n");
			
			return TRUE;
		}
	} else if ((openfirst) && (deviceptr == NULL) && (mmguicore->devices != NULL)) {
		/*Open first available device if specified is not found*/
		if (mmguicore->devices->data != NULL) {
			if (mmguicore->device != NULL) {
				/*Close currently opened device*/
				mmguicore_devices_close(mmguicore);
			}
			
			if ((mmguicore->devices_open_func)(mmguicore, mmguicore->devices->data)) {
				mmguicore->device = mmguicore->devices->data;
				/*Update device information*/
				if (mmguicore->devices_information_func != NULL) {
					(mmguicore->devices_information_func)(mmguicore);
					/*Open SMS database*/
					mmguicore->device->smsdb = mmgui_smsdb_open(mmguicore->device->persistentid, mmguicore->device->internalid);
					/*Open traffic database*/
					mmguicore->device->trafficdb = mmgui_trafficdb_open(mmguicore->device->persistentid, mmguicore->device->internalid);
					/*Open contacts*/
					mmguicore_contacts_enum(mmguicore);
					/*For Huawei modem USSD answers must be converted*/
					if (g_ascii_strcasecmp(mmguicore->device->manufacturer, "huawei") == 0) {
						mmguicore->device->ussdencoding = MMGUI_USSD_ENCODING_UCS2;
					} else {
						mmguicore->device->ussdencoding = MMGUI_USSD_ENCODING_GSM7;
					}
				}
				/*Open connection statistics source*/
				if (mmguicore->device_connection_open_func != NULL) {
					(mmguicore->device_connection_open_func)(mmguicore, mmguicore->devices->data);
				}
				/*Update connection parameters*/
				if (mmguicore->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MONITORING) {
					/*Update connection parameters in main thread*/
					mmguicore_update_connection_status(mmguicore, FALSE, FALSE);
				} else {
					/*Send command to refresh connection state in work thread*/
					workthreadcmd = MMGUI_THREAD_REFRESH_CMD;
					if (write(mmguicore->workthreadctl[1], &workthreadcmd, sizeof(workthreadcmd)) != sizeof(workthreadcmd)) {
						g_debug("Unable to send refresh command\n");
					}
				}
				/*Generate event*/
				if (mmguicore->extcb != NULL) {
					(mmguicore->extcb)(MMGUI_EVENT_DEVICE_OPENED, mmguicore, mmguicore->device, mmguicore->userdata);
				}
				
				g_debug("First available device successfully opened\n");
				
				return TRUE;
			}
		}
	}
	
	return FALSE;
}

static gboolean mmguicore_devices_close(mmguicore_t mmguicore)
{
	gboolean result;
	
	result = FALSE; 
	
	if (mmguicore->device != NULL) {
		/*Callback*/
		if (mmguicore->extcb != NULL) {
			(mmguicore->extcb)(MMGUI_EVENT_DEVICE_CLOSING, mmguicore, mmguicore->device, mmguicore->userdata);
		}
		#if GLIB_CHECK_VERSION(2,32,0)
			g_mutex_lock(&mmguicore->workthreadmutex);
		#else
			g_mutex_lock(mmguicore->workthreadmutex);
		#endif
		if ((mmguicore->devices_close_func)(mmguicore)) {
			/*Close SMS database*/
			mmguicore->device->smscaps = MMGUI_SMS_CAPS_NONE;
			mmgui_smsdb_close(mmguicore->device->smsdb);
			mmguicore->device->smsdb = NULL;
			/*Close contacts*/
			mmguicore->device->contactscaps = MMGUI_CONTACTS_CAPS_NONE;
			mmguicore_contacts_free(mmguicore->device->contactslist);
			mmguicore->device->contactslist = NULL;
			/*Info*/
			if (mmguicore->device->operatorname != NULL) {
				g_free(mmguicore->device->operatorname);
				mmguicore->device->operatorname = NULL;
			}
			mmguicore->device->operatorcode = 0;
			if (mmguicore->device->imei != NULL) {
				g_free(mmguicore->device->imei);
				mmguicore->device->imei = NULL;
			}
			if (mmguicore->device->imsi != NULL) {
				g_free(mmguicore->device->imsi);
				mmguicore->device->imsi = NULL;
			}
			/*USSD*/
			mmguicore->device->ussdcaps = MMGUI_USSD_CAPS_NONE;
			mmguicore->device->ussdencoding = MMGUI_USSD_ENCODING_GSM7;
			/*Location*/
			mmguicore->device->locationcaps = MMGUI_LOCATION_CAPS_NONE;
			memset(mmguicore->device->loc3gppdata, 0, sizeof(mmguicore->device->loc3gppdata));
			memset(mmguicore->device->locgpsdata, 0, sizeof(mmguicore->device->locgpsdata));
			/*Scan*/
			mmguicore->device->scancaps = MMGUI_SCAN_CAPS_NONE;
			/*Close traffic database session*/
			mmgui_trafficdb_session_close(mmguicore->device->trafficdb);
			/*Close traffic database*/
			mmgui_trafficdb_close(mmguicore->device->trafficdb);
			mmguicore->device->trafficdb = NULL;
			/*Traffic*/
			mmguicore->device->rxbytes = 0;
			mmguicore->device->txbytes = 0;
			mmguicore->device->sessiontime = 0;
			mmguicore->device->speedchecktime = 0;
			mmguicore->device->smschecktime = 0;
			mmguicore->device->speedindex = 0;
			mmguicore->device->connected = FALSE;
			memset(mmguicore->device->speedvalues, 0, sizeof(mmguicore->device->speedvalues));
			memset(mmguicore->device->interface, 0, sizeof(mmguicore->device->interface));
			/*Zero traffic values in UI*/
			mmguicore_traffic_zero(mmguicore);
			/*Close connection state source*/
			if (mmguicore->device_connection_close_func != NULL) {
				(mmguicore->device_connection_close_func)(mmguicore);
			}
			/*Device*/
			mmguicore->device = NULL;
			/*Successfully closed*/
			g_debug("Device successfully closed\n");
			result = TRUE;
		}
		#if GLIB_CHECK_VERSION(2,32,0)
			g_mutex_unlock(&mmguicore->workthreadmutex);
		#else
			g_mutex_unlock(mmguicore->workthreadmutex);
		#endif
	}
	
	return result;
}

gboolean mmguicore_devices_enable(mmguicore_t mmguicore, gboolean enabled)
{
	if (mmguicore == NULL) return FALSE;
	if ((mmguicore->device == NULL) || (mmguicore->devices_enable_func == NULL)) return FALSE;
	
	return (mmguicore->devices_enable_func)(mmguicore, enabled);
}

gboolean mmguicore_devices_unlock_with_pin(mmguicore_t mmguicore, gchar *pin)
{
	if ((mmguicore == NULL) || (pin == NULL)) return FALSE;
	if ((mmguicore->device == NULL) || (mmguicore->devices_unlock_with_pin_func == NULL)) return FALSE;
	
	return (mmguicore->devices_unlock_with_pin_func)(mmguicore, pin);
}

GSList *mmguicore_devices_get_list(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return NULL;
	
	return mmguicore->devices;
}

mmguidevice_t mmguicore_devices_get_current(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return NULL;
	
	return mmguicore->device;
}

gboolean mmguicore_devices_get_enabled(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return FALSE;
	if ((mmguicore->device == NULL) || (mmguicore->devices_state_func == NULL)) return FALSE;
	
	return (mmguicore->devices_state_func)(mmguicore, MMGUI_DEVICE_STATE_REQUEST_ENABLED);
}

gboolean mmguicore_devices_get_locked(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return FALSE;
	if ((mmguicore->device == NULL) || (mmguicore->devices_state_func == NULL)) return FALSE;
	
	return (mmguicore->devices_state_func)(mmguicore, MMGUI_DEVICE_STATE_REQUEST_LOCKED);
}

gboolean mmguicore_devices_get_registered(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return FALSE;
	if ((mmguicore->device == NULL) || (mmguicore->devices_state_func == NULL)) return FALSE;
	
	return (mmguicore->devices_state_func)(mmguicore, MMGUI_DEVICE_STATE_REQUEST_REGISTERED);
}

gboolean mmguicore_devices_get_prepared(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return FALSE;
	if ((mmguicore->device == NULL) || (mmguicore->devices_state_func == NULL)) return FALSE;
	
	return (mmguicore->devices_state_func)(mmguicore, MMGUI_DEVICE_STATE_REQUEST_PREPARED);
}

gboolean mmguicore_devices_get_connected(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return FALSE;
	if (mmguicore->device == NULL) return FALSE;
	
	return mmguicore->device->connected;
}

gint mmguicore_devices_get_lock_type(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return MMGUI_LOCK_TYPE_NONE;
	if (mmguicore->device == NULL) return MMGUI_LOCK_TYPE_NONE;
	
	return mmguicore->device->locktype;
}

gboolean mmguicore_devices_update_state(mmguicore_t mmguicore)
{
	gboolean updated;
	
	if (mmguicore == NULL) return FALSE;
	if (mmguicore->devices_update_state_func == NULL) return FALSE;
		
	updated = FALSE;
	
	updated = (mmguicore->devices_update_state_func)(mmguicore);
	
	return updated;
}

const gchar *mmguicore_devices_get_identifier(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return NULL;
	if (mmguicore->device == NULL) return NULL;
	
	return (const gchar *)mmguicore->device->persistentid;
}

const gchar *mmguicore_devices_get_internal_identifier(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return NULL;
	if (mmguicore->device == NULL) return NULL;
	
	return (const gchar *)mmguicore->device->internalid;
}

gpointer mmguicore_devices_get_sms_db(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return NULL;
	if (mmguicore->device == NULL) return NULL;
	
	return (gpointer)mmguicore->device->smsdb;
}

gpointer mmguicore_devices_get_traffic_db(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return NULL;
	if (mmguicore->device == NULL) return NULL;
	
	return (gpointer)mmguicore->device->trafficdb;
}

gboolean mmguicore_devices_get_connection_status(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return FALSE;
	if ((mmguicore->device == NULL) || (mmguicore->device_connection_status_func == NULL)) return FALSE;
	
	return (mmguicore->device_connection_status_func)(mmguicore);
}

guint64 mmguicore_devices_get_connection_timestamp(mmguicore_t mmguicore)
{
	guint64 timestamp;
	
	timestamp = (guint64)time(NULL);
	
	if (mmguicore == NULL) return timestamp;
	if ((mmguicore->device == NULL) || (mmguicore->device_connection_timestamp_func == NULL)) return timestamp;
	
	timestamp = (mmguicore->device_connection_timestamp_func)(mmguicore);
	
	return timestamp;
}

guint mmguicore_devices_get_current_operation(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return MMGUI_DEVICE_OPERATION_IDLE;
	if (mmguicore->device == NULL) return MMGUI_DEVICE_OPERATION_IDLE;
	
	return  mmguicore->device->operation;
}

guint mmguicore_location_get_capabilities(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return MMGUI_LOCATION_CAPS_NONE;
	if (mmguicore->device == NULL) return MMGUI_LOCATION_CAPS_NONE;
		
	return mmguicore->device->locationcaps;
}

guint mmguicore_sms_get_capabilities(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return MMGUI_SMS_CAPS_NONE;
	if (mmguicore->device == NULL) return MMGUI_SMS_CAPS_NONE;
		
	return mmguicore->device->smscaps;
}

static gint mmguicore_sms_sort_index_compare(gconstpointer a, gconstpointer b)
{
	mmgui_sms_message_t message1, message2;
	guint id1, id2;
	
	message1 = (mmgui_sms_message_t)a;
	message2 = (mmgui_sms_message_t)b;
	
	id1 = mmgui_smsdb_message_get_identifier(message1);
	id2 = mmgui_smsdb_message_get_identifier(message2);
	
	if (id1 < id2) {
		return -1;
	} else if (id1 > id2) {
		return 1;
	} else {
		return 0;
	}
}

static gint mmguicore_sms_sort_timestamp_compare(gconstpointer a, gconstpointer b)
{
	mmgui_sms_message_t message1, message2;
	time_t ts1, ts2;
	
	message1 = (mmgui_sms_message_t)a;
	message2 = (mmgui_sms_message_t)b;
	
	ts1 = mmgui_smsdb_message_get_timestamp(message1);
	ts2 = mmgui_smsdb_message_get_timestamp(message2);
	
	if (difftime(ts1, ts2) < 0.0) {
		return -1;
	} else if (difftime(ts1, ts2) > 0.0) {
		return 1;
	} else {
		return 0;
	}
}

static void mmguicore_sms_merge_foreach(gpointer data, gpointer user_data)
{
	mmgui_sms_message_t curmessage, srcmessage;
	const gchar *srcnumber, *curnumber;
	time_t srcts, curts;
	gboolean srcbinary, curbinary;
	guint ident;
	const gchar *text;
	
	curmessage = (mmgui_sms_message_t)data;
	srcmessage = (mmgui_sms_message_t)user_data;
	
	if ((srcmessage == NULL) || (srcmessage == curmessage) || (mmgui_smsdb_message_get_read(curmessage))) return;
	
	/*Number*/
	srcnumber = mmgui_smsdb_message_get_number(srcmessage);
	curnumber = mmgui_smsdb_message_get_number(curmessage);
	/*Timestamp*/
	srcts = mmgui_smsdb_message_get_timestamp(srcmessage);
	curts = mmgui_smsdb_message_get_timestamp(curmessage);
	/*Binary format*/
	srcbinary = mmgui_smsdb_message_get_binary(srcmessage);
	curbinary = mmgui_smsdb_message_get_binary(curmessage);
	
	if ((g_str_equal(srcnumber, curnumber)) && (srcbinary == curbinary)) {
		if (abs((gint)difftime(srcts, curts)) <= 5) {
			/*Copy identifier*/
			ident = mmgui_smsdb_message_get_identifier(curmessage);
			mmgui_smsdb_message_set_identifier(srcmessage, ident, TRUE);
			/*Copy decoded text*/
			text = mmgui_smsdb_message_get_text(curmessage);
			if (!srcbinary) {
				mmgui_smsdb_message_set_text(srcmessage, text, TRUE);
			} else {
				mmgui_smsdb_message_set_binary(srcmessage, FALSE);
				mmgui_smsdb_message_set_text(srcmessage, text, TRUE);
				mmgui_smsdb_message_set_binary(srcmessage, TRUE);
			}
			/*Mark message obsolete*/
			mmgui_smsdb_message_set_read(curmessage, TRUE);
		}
	}
}

GSList *mmguicore_sms_enum(mmguicore_t mmguicore, gboolean concatenation)
{
	GSList *messages;
	guint nummessages;
	GSList *iterator;
	mmgui_sms_message_t message;
	
	if ((mmguicore == NULL) || (mmguicore->sms_enum_func == NULL)) return NULL;
	
	messages = NULL;
	
	nummessages = (mmguicore->sms_enum_func)(mmguicore, &messages);
	
	if (nummessages > 1) {
		if (concatenation) {
			/*Sort messages by index*/
			messages = g_slist_sort(messages, mmguicore_sms_sort_index_compare);
			/*Try to concatenate every message and mark already concatenated with 'read' flag*/
			for (iterator=messages; iterator; iterator=iterator->next) {
				message = iterator->data;
				if (!mmgui_smsdb_message_get_read(message)) {
					g_slist_foreach(messages, mmguicore_sms_merge_foreach, message);
				}
			}
		}
		/*After all, sort messages by timestamp*/
		messages = g_slist_sort(messages, mmguicore_sms_sort_timestamp_compare);
	}
	
	return messages;
}

mmgui_sms_message_t mmguicore_sms_get(mmguicore_t mmguicore, guint index)
{
	mmgui_sms_message_t message;
	
	if ((mmguicore == NULL) || (mmguicore->sms_get_func == NULL)) return NULL;
	
	message = (mmguicore->sms_get_func)(mmguicore, index);
	
	return message;
}

gboolean mmguicore_sms_delete(mmguicore_t mmguicore, guint index)
{
	if ((mmguicore == NULL) || (mmguicore->sms_delete_func == NULL)) return FALSE;
	
	return (mmguicore->sms_delete_func)(mmguicore, index);
}

gboolean mmguicore_sms_validate_number(const gchar *number)
{
	gboolean validated;
	gsize length, digitlength, i;
	
	validated = TRUE;
	
	if ((number != NULL) && (number[0] != '\0')) {
		length = strlen(number);
		if (number[0] == '+') {
			digitlength = length-1;
		} else {
			digitlength = length;
		} 
		if ((digitlength < MMGUI_MIN_SMS_NUMBER_LENGTH) || (digitlength > MMGUI_MAX_SMS_NUMBER_LENGTH)) {
			validated = FALSE;
		} else {
			for (i=0; i<length; i++) {
				if (((!isdigit(number[i])) && (number[i] != '+')) || ((i != 0) && (number[i] == '+'))) {
					validated = FALSE;
					break;
				}
			}
		}
	} else {
		validated = FALSE;
	}
	
	return validated;
}

gboolean mmguicore_sms_send(mmguicore_t mmguicore, gchar *number, gchar *text, gint validity, gboolean report)
{
	if ((mmguicore == NULL) || (mmguicore->sms_send_func == NULL)) return FALSE;
	
	if ((number == NULL) || (text == NULL)) return FALSE;
	
	if ((!mmguicore_sms_validate_number(number)) || (text[0] == '\0') || (validity < -1) || (validity > 255)) return FALSE;
	
	return (mmguicore->sms_send_func)(mmguicore, number, text, validity, report);
}

guint mmguicore_ussd_get_capabilities(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return MMGUI_USSD_CAPS_NONE;
	if (mmguicore->device == NULL) return MMGUI_USSD_CAPS_NONE;
		
	return mmguicore->device->ussdcaps;
}

enum _mmgui_ussd_validation mmguicore_ussd_validate_request(gchar *request)
{
	enum _mmgui_ussd_validation statusid;
	gsize length, i;
	gboolean wrongsym;
	
	statusid = MMGUI_USSD_VALIDATION_INVALID;
	
	if ((request == NULL) && (request[0] == '\0')) return statusid;
	
	length = strlen(request);
	
	if (length > MMGUI_MAX_USSD_REQUEST_LENGTH) return statusid;
	
	if (((request[0] == '*') || (request[0] == '#')) && (request[length-1] == '#') && (length > 2)) {
		wrongsym = FALSE;
		for (i=0; i<length; i++) {
			if ((!isdigit(request[i])) && (request[i] != '*') && (request[i] != '#')) {
				wrongsym = TRUE;
				break;
			}
		}
		if (!wrongsym) {
			statusid = MMGUI_USSD_VALIDATION_REQUEST;
		} else {
			statusid = MMGUI_USSD_VALIDATION_RESPONSE;
		}
	} else {
		statusid = MMGUI_USSD_VALIDATION_RESPONSE;
	}
	
	return statusid;
}

gboolean mmguicore_ussd_cancel_session(mmguicore_t mmguicore)
{
	if ((mmguicore == NULL) || (mmguicore->ussd_cancel_session_func == NULL)) return FALSE;
	
	return (mmguicore->ussd_cancel_session_func)(mmguicore);
}

enum _mmgui_ussd_state mmguicore_ussd_get_state(mmguicore_t mmguicore)
{
	enum _mmgui_ussd_state stateid;
	
	stateid = MMGUI_USSD_STATE_UNKNOWN;
	
	if ((mmguicore == NULL) || (mmguicore->ussd_get_state_func == NULL)) return stateid;
	
	stateid = (mmguicore->ussd_get_state_func)(mmguicore);
	
	return stateid;
}

gboolean mmguicore_ussd_send(mmguicore_t mmguicore, gchar *request)
{
	enum _mmgui_ussd_validation validationid;
	gboolean reencode;
	
	if ((mmguicore == NULL) || (mmguicore->ussd_send_func == NULL)) return FALSE;
	
	if (request == NULL) return FALSE;
	
	validationid = mmguicore_ussd_validate_request(request);
	
	/*Try to reencode answer if nedded*/
	if (mmguicore->device->ussdencoding == MMGUI_USSD_ENCODING_UCS2) {
		reencode = TRUE;
	} else {
		reencode = FALSE;
	}
	
	return (mmguicore->ussd_send_func)(mmguicore, request, validationid, reencode);
}

gboolean mmguicore_ussd_set_encoding(mmguicore_t mmguicore, enum _mmgui_ussd_encoding encoding)
{
	if ((mmguicore == NULL) || (mmguicore->device == NULL)) return FALSE;
	
	mmguicore->device->ussdencoding = encoding;
	
	return TRUE;
}

enum _mmgui_ussd_encoding mmguicore_ussd_get_encoding(mmguicore_t mmguicore)
{
	if ((mmguicore == NULL) || (mmguicore->device == NULL)) return MMGUI_USSD_ENCODING_GSM7;
	
	return mmguicore->device->ussdencoding;
}

guint mmguicore_newtworks_scan_get_capabilities(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return MMGUI_SCAN_CAPS_NONE;
	if (mmguicore->device == NULL) return MMGUI_SCAN_CAPS_NONE;
		
	return mmguicore->device->scancaps;
}

static void mmguicore_networks_scan_free_foreach(gpointer data, gpointer user_data)
{
	mmgui_scanned_network_t network;
	
	if (data == NULL) return;
	
	network = (mmgui_scanned_network_t)data;
	
	if (network->operator_long != NULL) {
		g_free(network->operator_long);
	}
	
	if (network->operator_short != NULL) {
		g_free(network->operator_short);
	}
}

void mmguicore_networks_scan_free(GSList *networks)
{
	if (networks == NULL) return;
	
	g_slist_foreach(networks, mmguicore_networks_scan_free_foreach, NULL);
	
	g_slist_free(networks);
}

gboolean mmguicore_networks_scan(mmguicore_t mmguicore)
{
	if ((mmguicore == NULL) || (mmguicore->networks_scan_func == NULL)) return FALSE;
	
	return (mmguicore->networks_scan_func)(mmguicore);
}

guint mmguicore_contacts_get_capabilities(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return MMGUI_CONTACTS_CAPS_NONE;
	if (mmguicore->device == NULL) return MMGUI_CONTACTS_CAPS_NONE;
		
	return mmguicore->device->contactscaps;
}

void mmguicore_contacts_free_single(mmgui_contact_t contact, gboolean freestruct)
{
	if (contact == NULL) return;
	
	if (contact->name != NULL) {
		g_free(contact->name);
	}
	if (contact->number != NULL) {
		g_free(contact->number);
	}
	if (contact->email != NULL) {
		g_free(contact->email);
	}
	if (contact->group != NULL) {
		g_free(contact->group);
	}
	if (contact->name2 != NULL) {
		g_free(contact->name2);
	}
	if (contact->number2 != NULL) {
		g_free(contact->number2);
	}
	/*Free contact structure*/
	if (freestruct) {
		g_free(contact);
	}
}

static void mmguicore_contacts_free_foreach(gpointer data, gpointer user_data)
{
	mmgui_contact_t contact;
	
	if (data == NULL) return;
	
	contact = (mmgui_contact_t)data;
	
	mmguicore_contacts_free_single(contact, FALSE);
}

static void mmguicore_contacts_free(GSList *contacts)
{
	if (contacts == NULL) return;
	
	g_slist_foreach(contacts, mmguicore_contacts_free_foreach, NULL);
	
	g_slist_free(contacts);
}

static guint mmguicore_contacts_enum(mmguicore_t mmguicore)
{
	guint numcontacts;
	
	if ((mmguicore == NULL) || (mmguicore->contacts_enum_func == NULL)) return 0;
	if (mmguicore->device == NULL) return 0;
	
	if (mmguicore->device->contactslist != NULL) {
		mmguicore_contacts_free(mmguicore->device->contactslist);
		mmguicore->device->contactslist = NULL;
	}
	
	numcontacts = (mmguicore->contacts_enum_func)(mmguicore, &(mmguicore->device->contactslist));
		
	return numcontacts;
}

GSList *mmguicore_contacts_list(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return NULL;
	if (mmguicore->device == NULL) return NULL;
	if (!(mmguicore->device->contactscaps & MMGUI_CONTACTS_CAPS_EXPORT)) return NULL;
	
	return mmguicore->device->contactslist;
}

static gint mmguicore_contacts_get_compare(gconstpointer a, gconstpointer b)
{
	mmgui_contact_t contact;
	guint id;
	
	contact = (mmgui_contact_t)a;
	id = GPOINTER_TO_UINT(b);
	
	if (contact->id < id) {
		return 1;
	} else if (contact->id > id) {
		return -1;
	} else {
		return 0;
	}
}

mmgui_contact_t mmguicore_contacts_get(mmguicore_t mmguicore, guint index)
{
	GSList *contactptr;
	mmgui_contact_t contact;
	
	if (mmguicore == NULL) return NULL;
	if (mmguicore->device == NULL) return NULL;
	if (!(mmguicore->device->contactscaps & MMGUI_CONTACTS_CAPS_EXPORT)) return NULL;
	
	contactptr = g_slist_find_custom(mmguicore->device->contactslist, GUINT_TO_POINTER(index), mmguicore_contacts_get_compare);
	
	if (contactptr != NULL) {
		contact = (mmgui_contact_t)contactptr->data;
		return contact;
	} else {
		return NULL;
	}
}

static gint mmguicore_contacts_delete_compare(gconstpointer a, gconstpointer b)
{
	mmgui_contact_t contact;
	guint id;
	
	contact = (mmgui_contact_t)a;
	id = GPOINTER_TO_UINT(b);
	
	if (contact->id < id) {
		return 1;
	} else if (contact->id > id) {
		return -1;
	} else {
		return 0;
	}
}

gboolean mmguicore_contacts_delete(mmguicore_t mmguicore, guint index)
{
	gboolean result;
	GSList *contactptr;
	
	if ((mmguicore == NULL) || (mmguicore->contacts_delete_func == NULL)) return FALSE;
	if (mmguicore->device == NULL) return FALSE;
	if (!(mmguicore->device->contactscaps & MMGUI_CONTACTS_CAPS_EDIT)) return FALSE;
	
	result = (mmguicore->contacts_delete_func)(mmguicore, index);
	
	if (result) {
		contactptr = g_slist_find_custom(mmguicore->device->contactslist, GUINT_TO_POINTER(index), mmguicore_contacts_delete_compare);
		if (contactptr != NULL) {
			/*Remove contact from list*/
			mmguicore_contacts_free_single(contactptr->data, FALSE);
			mmguicore->device->contactslist = g_slist_remove(mmguicore->device->contactslist, contactptr->data);
		}
		return TRUE;
	} else {
		return FALSE;
	} 
}

gboolean mmguicore_contacts_add(mmguicore_t mmguicore, mmgui_contact_t contact)
{
	gint index;
	
	if ((mmguicore == NULL) || (contact == NULL) || (mmguicore->contacts_add_func == NULL)) return FALSE;
	if ((contact->name == NULL) || (contact->number == NULL)) return FALSE;
	if ((!mmguicore_sms_validate_number(contact->number)) || (contact->name[0] == '\0')) return FALSE;
	if (!(mmguicore->device->contactscaps & MMGUI_CONTACTS_CAPS_EDIT)) return FALSE;
	
	index = (mmguicore->contacts_add_func)(mmguicore, contact);
	
	if (index > -1) {
		mmguicore->device->contactslist = g_slist_append(mmguicore->device->contactslist, contact);
		return TRUE;
	} else {
		return FALSE;
	}
}

GSList *mmguicore_open_connections_list(mmguicore_t mmguicore)
{
	GSList *connections;
	
	if (mmguicore == NULL) return NULL;
	
	#if GLIB_CHECK_VERSION(2,32,0)
		g_mutex_lock(&mmguicore->connsyncmutex);
	#else
		g_mutex_lock(mmguicore->connsyncmutex);
	#endif
	
	connections = mmgui_netlink_open_interactive_connections_list(mmguicore->netlink);
	
	#if GLIB_CHECK_VERSION(2,32,0)
		g_mutex_unlock(&mmguicore->connsyncmutex);
	#else
		g_mutex_unlock(mmguicore->connsyncmutex);
	#endif
	
	return connections;
}

void mmguicore_close_connections_list(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return;
	
	#if GLIB_CHECK_VERSION(2,32,0)
		g_mutex_lock(&mmguicore->connsyncmutex);
	#else
		g_mutex_lock(mmguicore->connsyncmutex);
	#endif
	
	mmgui_netlink_close_interactive_connections_list(mmguicore->netlink);
	
	#if GLIB_CHECK_VERSION(2,32,0)
		g_mutex_unlock(&mmguicore->connsyncmutex);
	#else
		g_mutex_unlock(mmguicore->connsyncmutex);
	#endif
}

GSList *mmguicore_get_connections_changes(mmguicore_t mmguicore)
{
	GSList *changes;
	
	if (mmguicore == NULL) return NULL;
	
	#if GLIB_CHECK_VERSION(2,32,0)
		g_mutex_lock(&mmguicore->connsyncmutex);
	#else
		g_mutex_lock(mmguicore->connsyncmutex);
	#endif
	
	changes = mmgui_netlink_get_connections_changes(mmguicore->netlink);
		
	#if GLIB_CHECK_VERSION(2,32,0)
		g_mutex_unlock(&mmguicore->connsyncmutex);
	#else
		g_mutex_unlock(mmguicore->connsyncmutex);
	#endif
	
	return changes;
}

gchar *mmguicore_get_last_error(mmguicore_t mmguicore)
{
	if ((mmguicore == NULL) || (mmguicore->last_error_func == NULL)) return NULL;
	
	return (mmguicore->last_error_func)(mmguicore);
}

gchar *mmguicore_get_last_connection_error(mmguicore_t mmguicore)
{
	if ((mmguicore == NULL) || (mmguicore->connection_last_error_func == NULL)) return NULL;
	
	return (mmguicore->connection_last_error_func)(mmguicore);
}

gboolean mmguicore_interrupt_operation(mmguicore_t mmguicore)
{
	if ((mmguicore == NULL) || (mmguicore->interrupt_operation_func == NULL)) return FALSE;
	
	return (mmguicore->interrupt_operation_func)(mmguicore);
}

mmguicore_t mmguicore_init(mmgui_event_ext_callback callback, mmgui_core_options_t options, gpointer userdata)
{
	mmguicore_t mmguicore;
	
	mmguicore = g_new0(struct _mmguicore, 1);
	/*Modules*/
	mmguicore->modules = NULL;
	mmguicore->modulepairs = NULL;
	/*Modem manager module*/
	mmguicore->module = NULL;
	mmguicore->moduleptr = NULL;
	mmguicore->module = NULL;
	/*Connection manager module*/
	mmguicore->cmodule = NULL;
	mmguicore->cmoduleptr = NULL;
	mmguicore->cmodule = NULL;
	/*Modem manager module functions*/
	mmguicore->open_func = NULL;
	mmguicore->close_func = NULL;
	mmguicore->last_error_func = NULL;
	mmguicore->interrupt_operation_func = NULL;
	mmguicore->set_timeout_func = NULL;
	mmguicore->devices_enum_func = NULL;
	mmguicore->devices_open_func = NULL;
	mmguicore->devices_close_func = NULL;
	mmguicore->devices_state_func = NULL;
	mmguicore->devices_update_state_func = NULL;
	mmguicore->devices_information_func = NULL;
	mmguicore->devices_enable_func = NULL;
	mmguicore->devices_unlock_with_pin_func = NULL;
	mmguicore->sms_enum_func = NULL;
	mmguicore->sms_get_func = NULL;
	mmguicore->sms_delete_func = NULL;
	mmguicore->sms_send_func = NULL;
	mmguicore->ussd_cancel_session_func = NULL;
	mmguicore->ussd_get_state_func = NULL;
	mmguicore->ussd_send_func = NULL;
	mmguicore->networks_scan_func = NULL;
	mmguicore->contacts_enum_func = NULL;
	mmguicore->contacts_delete_func = NULL;
	mmguicore->contacts_add_func = NULL;
	/*Connection manager module functions*/
	mmguicore->connection_open_func = NULL;
	mmguicore->connection_close_func = NULL;
	mmguicore->connection_enum_func = NULL;
	mmguicore->connection_add_func = NULL;
	mmguicore->connection_update_func = NULL;
	mmguicore->connection_remove_func = NULL;
	mmguicore->connection_last_error_func = NULL;
	mmguicore->device_connection_open_func = NULL;
	mmguicore->device_connection_close_func = NULL;
	mmguicore->device_connection_status_func = NULL;
	mmguicore->device_connection_timestamp_func = NULL;
	mmguicore->device_connection_get_active_uuid_func = NULL;
	mmguicore->device_connection_connect_func = NULL;
	mmguicore->device_connection_disconnect_func = NULL;
	/*Work thread*/
	mmguicore->workthread = NULL;
	/*Devices*/
	mmguicore->devices = NULL;
	mmguicore->device = NULL;
	/*External callback*/
	mmguicore->extcb = callback;
	/*Core options*/
	mmguicore->options = options;
	/*User data pointer*/
	mmguicore->userdata = userdata;
	/*Open polkit interface*/
	mmguicore->polkit = mmgui_polkit_open();
	/*Open service manager interface*/
	mmguicore->svcmanager = mmgui_svcmanager_open(mmguicore->polkit, mmguicore_svcmanager_callback, mmguicore);
	/*Open modules cache*/
	mmguicore_modules_cache_open(mmguicore);
	/*Prepare recommended modules*/
	if (!mmguicore_modules_enumerate(mmguicore)) {
		/*Close service manager interface*/
		mmgui_svcmanager_close(mmguicore->svcmanager);
		/*Close polkit interface*/
		mmgui_polkit_close(mmguicore->polkit);
		/*Free resources*/		
		g_free(mmguicore);
		return NULL;
	}
	
	return mmguicore;
}

gboolean mmguicore_start(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return FALSE;
	
	return mmguicore_modules_prepare(mmguicore);		
}

static gboolean mmguicore_main(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return FALSE;
	
	/*Internal callback*/
	mmguicore->eventcb = mmguicore_event_callback;
	
	/*Select module*/
	if (!mmguicore_modules_select(mmguicore)) {
		return FALSE;
	}
	
	/*Open netlink interface*/
	mmguicore->netlink = mmgui_netlink_open();
	
	/*New day time*/
	mmguicore->newdaytime = mmgui_trafficdb_get_new_day_timesatmp(time(NULL), NULL, NULL);
	
	/*Work thread*/
	if (pipe(mmguicore->workthreadctl) == 0) {
		#if GLIB_CHECK_VERSION(2,32,0)
			g_mutex_init(&mmguicore->workthreadmutex);
			g_mutex_init(&mmguicore->connsyncmutex);
		#else
			mmguicore->workthreadmutex = g_mutex_new();
			mmguicore->connsyncmutex = g_mutex_new();
		#endif
		
		#if GLIB_CHECK_VERSION(2,32,0)
			mmguicore->workthread = g_thread_new("Modem Manager GUI work thread", mmguicore_work_thread, mmguicore);
		#else 
			mmguicore->workthread = g_thread_create(mmguicore_work_thread, mmguicore, TRUE, NULL);
		#endif
	} else {
		mmguicore->workthread = NULL;
	}
	
	return TRUE;
}

void mmguicore_close(mmguicore_t mmguicore)
{
	guint workthreadcmd;
	
	if (mmguicore == NULL) return;
	
	/*Close opened device*/
	mmguicore_devices_close(mmguicore);
	
	if (mmguicore->workthread != NULL) {
		/*Stop work thread*/
		workthreadcmd = MMGUI_THREAD_STOP_CMD;
		/*Send command for thread termination*/
		if (write(mmguicore->workthreadctl[1], &workthreadcmd, sizeof(workthreadcmd)) > 0) {
			/*Wait for thread termination*/
			g_thread_join(mmguicore->workthread);
			/*Close thread control pipe*/
			close(mmguicore->workthreadctl[1]);
			/*Free mutexes*/
			#if GLIB_CHECK_VERSION(2,32,0)
				g_mutex_clear(&mmguicore->workthreadmutex);
				g_mutex_clear(&mmguicore->connsyncmutex);
			#else
				g_mutex_free(mmguicore->workthreadmutex);
				g_mutex_free(mmguicore->connsyncmutex);
			#endif
		}
	}
	
	/*Close netlink interface*/
	if (mmguicore->netlink != NULL) {
		mmgui_netlink_close(mmguicore->netlink);
	}
	/*Close service manager interface*/
	mmgui_svcmanager_close(mmguicore->svcmanager);
	/*Close polkit interface*/
	mmgui_polkit_close(mmguicore->polkit);
	/*Close opened modules*/
	mmguicore_modules_close(mmguicore);
	/*Free modules list*/
	mmguicore_modules_free(mmguicore);
	
	g_free(mmguicore);
}

static gpointer mmguicore_work_thread(gpointer data)
{
	mmguicore_t mmguicore;
	/*Shared buffer*/
	gchar *databuf, *radatabuf;
	gsize databufsize;
	/*Polling variables*/
	guint activesockets;
	gint pollstatus;
	gint recvbytes;
	struct pollfd pollfds[3];
	/*Work thread control*/
	gint ctlfd, ctlfdnum;
	gint workthreadcmd;
	/*Interface monitoring*/
	gint intfd, intfdnum;
	struct iovec intiov;
	struct msghdr intmsg;
	/*Connections monitoring*/
	gint connfd, connfdnum;
	struct iovec conniov;
	struct msghdr connmsg;
	/*Events*/
	struct _mmgui_netlink_interface_event event;
	time_t ifstatetime, currenttime;
	gboolean oldstate;
	gchar oldinterface[IFNAMSIZ];
		
	
	mmguicore = (mmguicore_t)data;
	
	if (mmguicore == NULL) return NULL;
	
	/*Initialize shared buffer*/
	databufsize = 4096;
	databuf = g_malloc0(databufsize);
	intiov.iov_len = databufsize;
	intiov.iov_base = databuf;
	conniov.iov_len = databufsize;
	conniov.iov_base = databuf;
	
	/*Initialize active sockets*/
	activesockets = 0;
	ctlfdnum = 0;
	intfdnum = 0;
	connfdnum = 0;
	
	ctlfd = mmguicore->workthreadctl[0];
	
	if (ctlfd != -1) {
		ctlfdnum = activesockets;
		pollfds[ctlfdnum].fd = ctlfd;
		pollfds[ctlfdnum].events = POLLIN;
		activesockets++;
	}
	
	intfd = mmgui_netlink_get_interfaces_monitoring_socket_fd(mmguicore->netlink);
	
	if (intfd != -1) {
		intfdnum = activesockets;
		pollfds[intfdnum].fd = intfd;
		pollfds[intfdnum].events = POLLIN;
		intmsg.msg_name = (void *)mmgui_netlink_get_interfaces_monitoring_socket_address(mmguicore->netlink);
		intmsg.msg_namelen = sizeof(struct sockaddr_nl);
		intmsg.msg_iov = &intiov;
		intmsg.msg_iovlen = 1;
		intmsg.msg_control = NULL;
		intmsg.msg_controllen = 0;
		intmsg.msg_flags = 0;
		activesockets++;
	}
	
	connfd = mmgui_netlink_get_connections_monitoring_socket_fd(mmguicore->netlink);
	
	if (connfd != -1) {
		connfdnum = activesockets;
		pollfds[connfdnum].fd = connfd;
		pollfds[connfdnum].events = POLLIN;
		connmsg.msg_name = (void *)mmgui_netlink_get_connections_monitoring_socket_address(mmguicore->netlink);
		connmsg.msg_namelen = sizeof(struct sockaddr_nl);
		connmsg.msg_iov = &conniov;
		connmsg.msg_iovlen = 1;
		connmsg.msg_control = NULL;
		connmsg.msg_controllen = 0;
		connmsg.msg_flags = 0;
		activesockets++;
	}
	
	/*First we have to get device state*/
	ifstatetime = time(NULL);
	
	while (TRUE) {
		pollstatus = poll(pollfds, activesockets, 1000);
		if (pollstatus > 0) {
			/*Work thread control*/
			if (pollfds[ctlfdnum].revents & POLLIN) {
				/*Extend buffer if needed*/
				recvbytes = recv(ctlfd, NULL, 0, MSG_PEEK | MSG_TRUNC);
				if (recvbytes > databufsize) {
					radatabuf = g_try_realloc(databuf, recvbytes);
					if (radatabuf != NULL) {
						databufsize = recvbytes;
						databuf = radatabuf;
						intiov.iov_len = databufsize;
						intiov.iov_base = databuf;
						conniov.iov_len = databufsize;
						conniov.iov_base = databuf;
					}
				}
				/*Receive data*/
				recvbytes = read(ctlfd, &workthreadcmd, sizeof(workthreadcmd));
				if (recvbytes > 0) {
					if (workthreadcmd == MMGUI_THREAD_STOP_CMD) {
						/*Terminate thread*/
						break;
					} else if (workthreadcmd == MMGUI_THREAD_REFRESH_CMD) {
						/*Refresh connection state*/
						ifstatetime = time(NULL);
					}
				} else {
					g_debug("Work thread: Control command not received\n");
				}
			}
		}
		
		/*Lock mutex and work with device in safe mode*/
		#if GLIB_CHECK_VERSION(2,32,0)
			g_mutex_lock(&mmguicore->workthreadmutex);
		#else
			g_mutex_lock(mmguicore->workthreadmutex);
		#endif
		
		currenttime = time(NULL);
		
		/*Connections monitoring*/
		if ((mmguicore->device != NULL) && (!(mmguicore->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MONITORING))) {
			if (abs((gint)difftime(ifstatetime, currenttime)) <= 15) {
				/*Save old values*/
				g_debug("Requesting network interface state information\n");
				oldstate = mmguicore->device->connected;
				strncpy(oldinterface, mmguicore->device->interface, sizeof(oldinterface));
				/*Request new information*/
				if (mmguicore_devices_get_connection_status(mmguicore)) {
					g_debug("Got new interface state\n");
					if ((oldstate != mmguicore->device->connected) || (!g_str_equal(oldinterface, mmguicore->device->interface))) {
						/*State changed, no need to request information more*/
						ifstatetime = 0;
						/*Zero values on disconnect*/
						if (!mmguicore->device->connected) {
							/*Close traffic database session*/
							mmgui_trafficdb_session_close(mmguicore->device->trafficdb);
							/*Zero traffic values in UI*/
							mmguicore_traffic_zero(mmguicore);
							/*Device disconnect signal*/
							if (mmguicore->extcb != NULL) {
								(mmguicore->extcb)(MMGUI_EVENT_DEVICE_CONNECTION_STATUS, mmguicore, GUINT_TO_POINTER(FALSE), mmguicore->userdata);
							}
						} else {
							/*Get session start timestamp*/
							mmguicore->device->sessionstarttime = (time_t)mmguicore_devices_get_connection_timestamp(mmguicore);
							mmguicore->device->sessiontime = llabs((gint64)difftime(currenttime, mmguicore->device->sessionstarttime));
							g_debug("Session start time: %" G_GUINT64_FORMAT ", duration: %" G_GUINT64_FORMAT "\n", (guint64)mmguicore->device->sessionstarttime, mmguicore->device->sessiontime);
							/*Open traffic database session*/
							mmgui_trafficdb_session_new(mmguicore->device->trafficdb, mmguicore->device->sessionstarttime);
							/*Device connect signal*/
							if (mmguicore->extcb != NULL) {
								(mmguicore->extcb)(MMGUI_EVENT_DEVICE_CONNECTION_STATUS, mmguicore, GUINT_TO_POINTER(TRUE), mmguicore->userdata);
							}
						}
						g_debug("Interface state changed\n");
					}
				}
			}
		}
		
		if (pollstatus == 0) {
			/*Timeout - request data*/
			if (mmguicore->device != NULL) {
				if (mmguicore->device->connected) {
					/*Interface statistics*/
					mmgui_netlink_request_interface_statistics(mmguicore->netlink, mmguicore->device->interface);
					/*TCP connections - IPv4*/
					mmgui_netlink_request_connections_list(mmguicore->netlink, AF_INET);
					/*TCP connections - IPv6*/
					mmgui_netlink_request_connections_list(mmguicore->netlink, AF_INET6);
				}
			}
		} else if (pollstatus > 0) {
			/*New data available*/
			/*Interface monitoring*/
			if (pollfds[intfdnum].revents & POLLIN) {
				/*Extend buffer if needed*/
				recvbytes = recv(intfd, NULL, 0, MSG_PEEK | MSG_TRUNC);
				if (recvbytes > databufsize) {
					radatabuf = g_try_realloc(databuf, recvbytes);
					if (radatabuf != NULL) {
						databufsize = recvbytes;
						databuf = radatabuf;
						intiov.iov_len = databufsize;
						intiov.iov_base = databuf;
						conniov.iov_len = databufsize;
						conniov.iov_base = databuf;
					}
				}
				/*Receive data*/
				recvbytes = recvmsg(intfd, &intmsg, 0);
				if (recvbytes) {
					if (mmgui_netlink_read_interface_event(mmguicore->netlink, databuf, recvbytes, &event)) {
						/*Traffic statisctics available*/
						if (event.type & MMGUI_NETLINK_INTERFACE_EVENT_TYPE_STATS) {
							if (mmguicore->device != NULL) {
								if ((mmguicore->device->connected) && (g_str_equal(mmguicore->device->interface, event.ifname))) {
									/*Count traffic*/
									mmguicore_traffic_count(mmguicore, event.rxbytes, event.txbytes);
								}
							}
						}
						/*Interface created*/
						if (event.type & MMGUI_NETLINK_INTERFACE_EVENT_TYPE_ADD) {
							g_debug("Created network interface event\n");
							if ((mmguicore->device != NULL) && (!(mmguicore->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MONITORING))) {
								if ((!mmguicore->device->connected) && (event.up) && (event.running)) {
									/*PPP or Ethernet interface*/
									g_debug("Created network interface event signal\n");
									ifstatetime = time(NULL);
								} else if ((mmguicore->device->connected) && (!event.up) && (!event.running)) {
									/*Ethernet interface*/
									if (g_str_equal(mmguicore->device->interface, event.ifname)) {
										g_debug("Brought down network interface event signal\n");
										ifstatetime = time(NULL);
									}
								}
							}
						}
						/*Interface removed*/
						if (event.type & MMGUI_NETLINK_INTERFACE_EVENT_TYPE_REMOVE) {
							g_debug("Removed network interface event\n");
							if ((mmguicore->device != NULL) && (!(mmguicore->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MONITORING))) {
								if ((mmguicore->device->connected) && (g_str_equal(mmguicore->device->interface, event.ifname))) {
									g_debug("Removed network interface event signal\n");
									ifstatetime = time(NULL);
								}
							}
						}	
					}
				} else {
					g_debug("Work thread: interface event not received\n");
				}
			}
			/*Connections monitoring*/
			if (pollfds[connfdnum].revents & POLLIN) {
				/*Extend buffer if needed*/
				recvbytes = recv(connfd, NULL, 0, MSG_PEEK | MSG_TRUNC);
				if (recvbytes > databufsize) {
					radatabuf = g_try_realloc(databuf, recvbytes);
					if (radatabuf != NULL) {
						databufsize = recvbytes;
						databuf = radatabuf;
						intiov.iov_len = databufsize;
						intiov.iov_base = databuf;
						conniov.iov_len = databufsize;
						conniov.iov_base = databuf;
					}
				}
				/*Receive data*/
				recvbytes = recvmsg(connfd, &connmsg, 0);
				if (recvbytes) {
					#if GLIB_CHECK_VERSION(2,32,0)
						g_mutex_lock(&mmguicore->connsyncmutex);
					#else
						g_mutex_lock(mmguicore->connsyncmutex);
					#endif
					if (mmgui_netlink_read_connections_list(mmguicore->netlink, databuf, recvbytes)) {
						if (mmguicore->extcb != NULL) {
							(mmguicore->extcb)(MMGUI_EVENT_UPDATE_CONNECTIONS_LIST, mmguicore, mmguicore, mmguicore->userdata);
						}
					}
					#if GLIB_CHECK_VERSION(2,32,0)
						g_mutex_unlock(&mmguicore->connsyncmutex);
					#else
						g_mutex_unlock(mmguicore->connsyncmutex);
					#endif
				} else {
					g_debug("Work thread: connections data not received\n");
				}
			}
		}
		
		/*Update internal module state*/
		mmguicore_devices_update_state(mmguicore);
		
		if (mmguicore->device != NULL) {
			/*Handle traffic limits*/
			mmguicore_traffic_limits(mmguicore);
		}
		
		/*New day time*/
		if (difftime(mmguicore->newdaytime, currenttime) <= 0) {
			/*Callback*/
			if (mmguicore->extcb != NULL) {
				(mmguicore->extcb)(MMGUI_EVENT_SMS_NEW_DAY, mmguicore, NULL, mmguicore->userdata);
			}
			mmguicore->newdaytime = mmgui_trafficdb_get_new_day_timesatmp(currenttime, NULL, NULL);
		}
		
		/*Unlock mutex after work finished*/
		#if GLIB_CHECK_VERSION(2,32,0)
			g_mutex_unlock(&mmguicore->workthreadmutex);
		#else
			g_mutex_unlock(mmguicore->workthreadmutex);
		#endif
	}
	
	/*Close thread control pipe descriptor*/
	close(mmguicore->workthreadctl[0]);
	
	return NULL;
}

static void mmguicore_traffic_count(mmguicore_t mmguicore, guint64 rxbytes, guint64 txbytes)
{
	mmguidevice_t device;
	time_t currenttime;
	guint timeframe;
	struct _mmgui_traffic_update trafficupd;
	
	if (mmguicore == NULL) return;
	
	device = mmguicore->device;
	if (device == NULL) return;	
	
	currenttime = time(NULL);
	
	if ((device->connected) && (device->rxbytes <= rxbytes) && (device->txbytes <= txbytes)) {
		if (device->speedchecktime != 0) {
			/*Time period for speed calculation*/
			timeframe = (guint)difftime(currenttime, device->speedchecktime);
			device->sessiontime += timeframe;
			if (device->speedindex < MMGUI_SPEED_VALUES_NUMBER) {
				/*Add new speed value*/
				device->speedvalues[0][device->speedindex] = (gfloat)((rxbytes - device->rxbytes)*8)/(gfloat)(timeframe*1024);
				device->speedvalues[1][device->speedindex] = (gfloat)((txbytes - device->txbytes)*8)/(gfloat)(timeframe*1024);
				device->speedindex++;
			} else {
				/*Move speed values and add new one*/
				memmove(&device->speedvalues[0][0], &device->speedvalues[0][1], sizeof(device->speedvalues[0]) - sizeof(device->speedvalues[0][0]));
				memmove(&device->speedvalues[1][0], &device->speedvalues[1][1], sizeof(device->speedvalues[1]) - sizeof(device->speedvalues[1][0]));
				device->speedvalues[0][MMGUI_SPEED_VALUES_NUMBER-1] = (gfloat)((rxbytes - device->rxbytes)*8)/(gfloat)(timeframe*1024);
				device->speedvalues[1][MMGUI_SPEED_VALUES_NUMBER-1] = (gfloat)((txbytes - device->txbytes)*8)/(gfloat)(timeframe*1024);
			}
			
			/*Update database*/
			trafficupd.fullrxbytes = device->rxbytes;
			trafficupd.fulltxbytes = device->txbytes;
			trafficupd.fulltime = device->sessiontime;
			trafficupd.deltarxbytes = rxbytes - device->rxbytes;
			trafficupd.deltatxbytes = txbytes - device->txbytes;
			trafficupd.deltaduration = timeframe;
			
			mmgui_trafficdb_traffic_update(mmguicore->device->trafficdb, &trafficupd);
		}
		/*Update traffic count*/
		device->rxbytes = rxbytes;
		device->txbytes = txbytes;
	}
	
	/*Set last update time*/
	device->speedchecktime = currenttime;
	/*Callback*/
	if (mmguicore->extcb != NULL) {
		(mmguicore->extcb)(MMGUI_EVENT_NET_STATUS, mmguicore, device, mmguicore->userdata);
	}
}

static void mmguicore_traffic_zero(mmguicore_t mmguicore)
{
	mmguidevice_t device;
		
	if (mmguicore == NULL) return;
	
	device = mmguicore->device;
	
	if (device == NULL) return;	
	
	/*Zero speed values if device is not connected anymore*/
	mmguicore->device->speedindex = 0;
	mmguicore->device->speedvalues[0][0] = 0.0;
	mmguicore->device->speedvalues[1][0] = 0.0;
	mmguicore->device->rxbytes = 0;
	mmguicore->device->txbytes = 0;
	/*Set last update time*/
	mmguicore->device->speedchecktime = time(NULL);
	/*Callback*/
	if (mmguicore->extcb != NULL) {
		(mmguicore->extcb)(MMGUI_EVENT_NET_STATUS, mmguicore, NULL, mmguicore->userdata);
	}
}

static void mmguicore_traffic_limits(mmguicore_t mmguicore)
{
	if (mmguicore == NULL) return;
	
	if ((mmguicore->options != NULL) && (mmguicore->device != NULL)) {
		/*Traffic limit*/
		if (mmguicore->options->trafficenabled) {
			if ((!mmguicore->options->trafficexecuted) && (mmguicore->options->trafficfull < (mmguicore->device->rxbytes + mmguicore->device->txbytes))) {
				mmguicore->options->trafficexecuted = TRUE;
				if (mmguicore->options->trafficaction == MMGUI_EVENT_ACTION_DISCONNECT) {
					if (mmguicore->device_connection_disconnect_func != NULL) {
						(mmguicore->device_connection_disconnect_func)(mmguicore);
					}
				}
				/*Callback*/
				if (mmguicore->extcb != NULL) {
					(mmguicore->extcb)(MMGUI_EVENT_TRAFFIC_LIMIT, mmguicore, NULL, mmguicore->userdata);
				}
			}
		}
		
		/*Time limit*/
		if (mmguicore->options->timeenabled) {
			if ((!mmguicore->options->timeexecuted) && (mmguicore->options->timefull < mmguicore->device->sessiontime)) {
				mmguicore->options->timeexecuted = TRUE;
				if (mmguicore->options->timeaction == MMGUI_EVENT_ACTION_DISCONNECT) {
					if (mmguicore->device_connection_disconnect_func != NULL) {
						(mmguicore->device_connection_disconnect_func)(mmguicore);
					}
				}
				/*Callback*/
				if (mmguicore->extcb != NULL) {
					(mmguicore->extcb)(MMGUI_EVENT_TIME_LIMIT, mmguicore, NULL, mmguicore->userdata);
				}
			}
		}
	}
}

static void mmguicore_update_connection_status(mmguicore_t mmguicore, gboolean sendresult, gboolean result)
{
	if (mmguicore == NULL) return;
	
	if (!mmguicore->device->connected) {
		/*Close traffic database session*/
		mmgui_trafficdb_session_close(mmguicore->device->trafficdb);
		/*Zero traffic values in UI*/
		mmguicore_traffic_zero(mmguicore);
		if (sendresult) {
			if (mmguicore->extcb != NULL) {
				(mmguicore->extcb)(MMGUI_EVENT_MODEM_CONNECTION_RESULT, mmguicore, GUINT_TO_POINTER(result), mmguicore->userdata);
			}
		} else {
			if (mmguicore->extcb != NULL) {
				(mmguicore->extcb)(MMGUI_EVENT_DEVICE_CONNECTION_STATUS, mmguicore, GUINT_TO_POINTER(FALSE), mmguicore->userdata);
			}
		}
	} else {
		/*Get session start timestamp*/
		mmguicore->device->sessionstarttime = (time_t)mmguicore_devices_get_connection_timestamp(mmguicore);
		mmguicore->device->sessiontime = llabs((gint64)difftime(time(NULL), mmguicore->device->sessionstarttime));
		/*Open traffic database session*/
		mmgui_trafficdb_session_new(mmguicore->device->trafficdb, mmguicore->device->sessionstarttime);
		if (sendresult) {
			if (mmguicore->extcb != NULL) {
				(mmguicore->extcb)(MMGUI_EVENT_MODEM_CONNECTION_RESULT, mmguicore, GUINT_TO_POINTER(result), mmguicore->userdata);
			}
		} else {
			if (mmguicore->extcb != NULL) {
				(mmguicore->extcb)(MMGUI_EVENT_DEVICE_CONNECTION_STATUS, mmguicore, GUINT_TO_POINTER(TRUE), mmguicore->userdata);
			}
		}
	}
}
