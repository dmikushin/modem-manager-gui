/*
 *      pppd245.c
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
#include <gio/gio.h>
#include <gmodule.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <string.h>
#include <net/if.h>

#include "../mmguicore.h"

#define MMGUI_MODULE_SERVICE_NAME  "/usr/sbin/pppd"
#define MMGUI_MODULE_SYSTEMD_NAME  "NULL"
#define MMGUI_MODULE_IDENTIFIER    245
#define MMGUI_MODULE_DESCRIPTION   "pppd >= 2.4.5"
#define MMGUI_MODULE_COMPATIBILITY "org.freedesktop.ModemManager;org.freedesktop.ModemManager1;org.ofono;"

//Internal definitions
#define MODULE_INT_PPPD_DB_FILE_PATH_1               "/var/run/pppd2.tdb"
#define MODULE_INT_PPPD_DB_FILE_PATH_2               "/var/run/ppp/pppd2.tdb"
#define MODULE_INT_PPPD_LOCK_FILE_PATH               "/var/run/%s.pid"
#define MODULE_INT_PPPD_DB_FILE_START_PARAMETER      "ORIG_UID="
#define MODULE_INT_PPPD_DB_FILE_END_PARAMETER        "USEPEERDNS="
#define MODULE_INT_PPPD_DB_FILE_DEVICE_PARAMETER     "DEVICE="
#define MODULE_INT_PPPD_DB_FILE_INTERFACE_PARAMETER  "IFNAME="
#define MODULE_INT_PPPD_DB_FILE_PID_PARAMETER        "PPPD_PID="
#define MODULE_INT_PPPD_SYSFS_DEVICE_PATH            "/sys/dev/char/%u:%u"
#define MODULE_INT_PPPD_SYSFS_START_PARAMETER        "devices/pci"
#define MODULE_INT_PPPD_SYSFS_END_PARAMETER          '/'

//Private module variables
struct _mmguimoduledata {
	//Device serial
	gchar devserial[32];
	//Connected device information
	gchar conndevice[256];
	gchar connserial[32];
	gchar connifname[IFNAMSIZ];
	//pppd service pid
	pid_t pid;
	//Last error message
	gchar *errormessage;
};

typedef struct _mmguimoduledata *moduledata_t;


static void mmgui_module_handle_error_message(mmguicore_t mmguicore, gchar *message)
{
	moduledata_t moduledata;
	
	if (mmguicore == NULL) return;
	
	moduledata = (moduledata_t)mmguicore->cmoduledata;
	
	if (moduledata == NULL) return;
	
	if (moduledata->errormessage != NULL) {
		g_free(moduledata->errormessage);
	}
	
	if (message != NULL) {
		moduledata->errormessage = g_strdup(message);
	} else {
		moduledata->errormessage = g_strdup("Unknown error");
	}
	
	g_warning("%s: %s", MMGUI_MODULE_DESCRIPTION, moduledata->errormessage);
}

static gchar *mmgui_module_pppd_strrstr(gchar *source, gsize sourcelen, gchar *entry, gsize entrylen)
{
	gboolean found;
	gchar *sourceptr, *curptr;
	gsize ind;
	
	if ((source == NULL) || (entry == NULL)) return NULL;
	if ((sourcelen == 0) || (entrylen == 0)) return source;
	
	sourceptr = source + sourcelen - entrylen;
	
	for (curptr=sourceptr; curptr>=source; curptr--) {
		found = TRUE;
		for (ind=0; ind<entrylen; ind++) {
			if (curptr[ind] != entry[ind]) {
				found = FALSE;
				break;
			}
		}
		
		if (found) {
			return curptr;
		}
	}
	
	return NULL;
}

static gchar *mmgui_module_pppd_get_config_fragment(gchar *string, gchar *parameter, gchar *buffer, gsize bufsize)
{
	gchar *segend, *segstart;
	gsize datalen, paramlen;
	
	if ((string == NULL) || (parameter == NULL) || (buffer == NULL) || (bufsize == 0)) return NULL;
	
	segstart = strstr(string, parameter);
	
	if (segstart == NULL) return NULL;
	
	segend = strchr(segstart, ';');
	
	if (segend == NULL) return NULL;
	
	paramlen = strlen(parameter);
	
	datalen = segend - segstart - paramlen;
	
	if (datalen > (bufsize - 1)) datalen = bufsize - 1;
	
	memcpy(buffer, segstart+paramlen, datalen);
	
	buffer[datalen] = '\0';
	
	return buffer;
}

static gchar *mmgui_module_pppd_get_config_string(gpointer dbmapping, gsize dbsize)
{
	gchar *segend, *segstart;
	gchar *result;
	
	if ((dbmapping == NULL) || (dbsize == 0)) return NULL;
	
	segend = mmgui_module_pppd_strrstr(dbmapping, dbsize, MODULE_INT_PPPD_DB_FILE_END_PARAMETER, strlen(MODULE_INT_PPPD_DB_FILE_END_PARAMETER));
	
	if ((segend == NULL) || ((segend != NULL) && ((*segend) >= dbsize))) return NULL;
	
	segstart = mmgui_module_pppd_strrstr(dbmapping, dbsize-(*segend), MODULE_INT_PPPD_DB_FILE_START_PARAMETER, strlen(MODULE_INT_PPPD_DB_FILE_START_PARAMETER));
	
	if ((segstart == NULL) || ((segstart != NULL) && (segend <= segstart))) return NULL;
	
	result = g_malloc0(segend-segstart+1);
	
	if (result == NULL) return NULL;
	
	memcpy(result, segstart, segend-segstart);
	
	return result;
}

static gchar *mmgui_module_pppd_get_device_serial(gchar *string, gchar *buffer, gsize bufsize)
{
	gchar *segend, *segstart;
	gsize datalen, paramlen;
	
	if ((string == NULL) || (buffer == NULL) || (bufsize == 0)) return NULL;
	
	paramlen = strlen(MODULE_INT_PPPD_SYSFS_START_PARAMETER);
	
	segstart = strstr(string, MODULE_INT_PPPD_SYSFS_START_PARAMETER);
	
	if (segstart == NULL) return NULL;
	
	segstart = strchr(segstart + paramlen, MODULE_INT_PPPD_SYSFS_END_PARAMETER);
	
	if (segstart == NULL) return NULL;
	
	segend = strchr(segstart+paramlen, MODULE_INT_PPPD_SYSFS_END_PARAMETER);
	
	if (segend == NULL) return NULL;
	
	datalen = segend - segstart - 1;
	
	if (datalen > (bufsize - 1)) datalen = bufsize - 1;
	
	memcpy(buffer, segstart + 1, datalen);
	
	buffer[datalen] = '\0';
	
	return buffer;
}

static gboolean mmgui_module_pppd_get_daemon_running(moduledata_t moduledata)
{
	gchar pidfilepath[32], pidfiledata[32];
	gint pidfilefd, status;
	/*struct flock pidfilelock;*/
	
	if (moduledata == NULL) return FALSE;
	if (moduledata->connifname[0] == '\0') return FALSE;
	
	memset(pidfilepath, 0, sizeof(pidfilepath));
	memset(pidfiledata, 0, sizeof(pidfiledata));
	
	snprintf(pidfilepath, sizeof(pidfilepath), MODULE_INT_PPPD_LOCK_FILE_PATH, moduledata->connifname);
	
	pidfilefd = open(pidfilepath, O_RDONLY);
	
	if (pidfilefd == -1) return FALSE;
	
	status = read(pidfilefd, pidfiledata, sizeof(pidfiledata));
	
	close(pidfilefd);
	
	if ((status != -1) && ((pid_t)atoi(pidfiledata) == moduledata->pid)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

static void mmgui_module_pppd_set_connection_status(gpointer mmguicore, gboolean connected)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	
	if (mmguicore == NULL) return;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->cmoduledata == NULL) return;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return;
	
	if (connected) {
		memset(mmguicorelc->device->interface, 0, IFNAMSIZ);
		strncpy(mmguicorelc->device->interface, moduledata->connifname, IFNAMSIZ);
		mmguicorelc->device->connected = TRUE;
	} else {
		memset(mmguicorelc->device->interface, 0, IFNAMSIZ);
		mmguicorelc->device->connected = FALSE;
	}
}

G_MODULE_EXPORT gboolean mmgui_module_init(mmguimodule_t module)
{
	if (module == NULL) return FALSE;
	
	module->type = MMGUI_MODULE_TYPE_CONNECTION_MANGER;
	module->requirement = MMGUI_MODULE_REQUIREMENT_FILE;
	module->priority = MMGUI_MODULE_PRIORITY_LOW;
	module->identifier = MMGUI_MODULE_IDENTIFIER;
	module->functions = MMGUI_MODULE_FUNCTION_BASIC;
	g_snprintf(module->servicename,   sizeof(module->servicename),   MMGUI_MODULE_SERVICE_NAME);
	g_snprintf(module->systemdname,   sizeof(module->systemdname),   MMGUI_MODULE_SYSTEMD_NAME);
	g_snprintf(module->description,   sizeof(module->description),   MMGUI_MODULE_DESCRIPTION);
	g_snprintf(module->compatibility, sizeof(module->compatibility), MMGUI_MODULE_COMPATIBILITY);
	
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_connection_open(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t *moduledata;
	
	if (mmguicore == NULL) return FALSE;
	
	mmguicorelc = (mmguicore_t)mmguicore;
	
	mmguicorelc->cmcaps = MMGUI_CONNECTION_MANAGER_CAPS_BASIC;
	
	moduledata = (moduledata_t *)&mmguicorelc->cmoduledata;
	
	(*moduledata) = g_new0(struct _mmguimoduledata, 1);
	
	(*moduledata)->errormessage = NULL;
	
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_connection_close(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
		
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	moduledata = (moduledata_t)(mmguicorelc->cmoduledata);
	
	if (moduledata != NULL) {
		if (moduledata->errormessage != NULL) {
			g_free(moduledata->errormessage);
		}
		
		g_free(moduledata);
	}
	
	return TRUE;
}

G_MODULE_EXPORT guint mmgui_module_connection_enum(gpointer mmguicore, GSList **connlist)
{
	return 0;
}

G_MODULE_EXPORT mmguiconn_t mmgui_module_connection_add(gpointer mmguicore, const gchar *name, const gchar *number, const gchar *username, const gchar *password, const gchar *apn, guint networkid, guint type, gboolean homeonly, const gchar *dns1, const gchar *dns2)
{
	return FALSE;
}

G_MODULE_EXPORT gboolean mmgui_module_connection_update(gpointer mmguicore, mmguiconn_t connection, const gchar *name, const gchar *number, const gchar *username, const gchar *password, const gchar *apn, guint networkid, gboolean homeonly, const gchar *dns1, const gchar *dns2)
{
	return FALSE;
}

G_MODULE_EXPORT gboolean mmgui_module_connection_remove(gpointer mmguicore, mmguiconn_t connection)
{
	return FALSE;
}

G_MODULE_EXPORT gchar *mmgui_module_connection_last_error(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	
	if (mmguicore == NULL) return NULL;
	
	mmguicorelc = (mmguicore_t)mmguicore;
	moduledata = (moduledata_t)(mmguicorelc->cmoduledata);
	
	return moduledata->errormessage;
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_open(gpointer mmguicore, mmguidevice_t device)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
		
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (device->sysfspath == NULL) {
		mmgui_module_handle_error_message(mmguicorelc, "Device data not available");
		return FALSE;
	}
	
	if (mmgui_module_pppd_get_device_serial(device->sysfspath, moduledata->devserial, sizeof(moduledata->devserial)) == NULL) {
		mmgui_module_handle_error_message(mmguicorelc, "Device serial not available");
		return FALSE;
	}
	
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_close(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
					
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	memset(moduledata->devserial, 0, sizeof(moduledata->devserial));
		
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_status(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	gint dbfd, state;
	struct stat statbuf;
	gpointer dbmapping;
	gchar *pppdconfig;
	gchar intbuf[16], sysfspath[128], sysfslink[128];
	gchar *parameter;
	gint status, devmajor, devminor;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (moduledata->devserial[0] == '\0') return FALSE;
	
	/*Usual PPPD database path*/
	dbfd = open(MODULE_INT_PPPD_DB_FILE_PATH_1, O_RDONLY);
	/*Try special path first*/
	if (dbfd == -1) {
		/*Special PPPD database path (Fedora uses it)*/
		dbfd = open(MODULE_INT_PPPD_DB_FILE_PATH_2, O_RDONLY);
		/*No database found*/
		if (dbfd == -1) {
			mmgui_module_pppd_set_connection_status(mmguicore, FALSE);
			g_debug("Failed to open PPPD database file\n");
			return TRUE;
		}
	}
	
	state = fstat(dbfd, &statbuf);
	
	if (state == -1) {
		mmgui_module_pppd_set_connection_status(mmguicore, FALSE);
		g_debug("Failed to get PPPD database file size\n");
		close(dbfd);
		return TRUE;
	}
	
	dbmapping = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, dbfd, 0);
	
	if(dbmapping == (void *)-1) {
		mmgui_module_pppd_set_connection_status(mmguicore, FALSE);
		mmgui_module_handle_error_message(mmguicorelc, "Failed to map PPPD database file into memory");
		close(dbfd);
		return TRUE;
	}
	
	pppdconfig = mmgui_module_pppd_get_config_string(dbmapping, statbuf.st_size);
	
	if (pppdconfig == NULL) {
		mmgui_module_pppd_set_connection_status(mmguicore, FALSE);
		mmgui_module_handle_error_message(mmguicorelc, "Failed to get config");
		munmap(dbmapping, statbuf.st_size);
		close(dbfd);
		return TRUE;
	}
	
	munmap(dbmapping, statbuf.st_size);
	close(dbfd);
	
	parameter = mmgui_module_pppd_get_config_fragment(pppdconfig, MODULE_INT_PPPD_DB_FILE_DEVICE_PARAMETER, moduledata->conndevice, sizeof(moduledata->conndevice));
	if (parameter == NULL) {
		mmgui_module_pppd_set_connection_status(mmguicore, FALSE);
		g_debug("Device entry not found in PPPD database\n");
		g_free(pppdconfig);
		return TRUE;
	}
	
	parameter = mmgui_module_pppd_get_config_fragment(pppdconfig, MODULE_INT_PPPD_DB_FILE_INTERFACE_PARAMETER, moduledata->connifname, sizeof(moduledata->connifname));
	if (parameter == NULL) {
		mmgui_module_pppd_set_connection_status(mmguicore, FALSE);
		g_debug("Interface entry not found in PPPD database\n");
		g_free(pppdconfig);
		return TRUE;
	}
	
	parameter = mmgui_module_pppd_get_config_fragment(pppdconfig, MODULE_INT_PPPD_DB_FILE_PID_PARAMETER, intbuf, sizeof(intbuf));
	if (parameter == NULL) {
		mmgui_module_pppd_set_connection_status(mmguicore, FALSE);
		g_debug("PPPD pid not found in PPPD database\n");
		g_free(pppdconfig);
		return TRUE;
	} else {
		moduledata->pid = (pid_t)atoi(parameter);
	}
	
	g_free(pppdconfig);
	
	if (!mmgui_module_pppd_get_daemon_running(moduledata)) {
		mmgui_module_pppd_set_connection_status(mmguicore, FALSE);
		g_debug("PPPD daemon is not running\n");
		return TRUE;
	}
	
	status = stat(moduledata->conndevice, &statbuf);
	
	if ((status == -1) || ((status == 0) && (!S_ISCHR(statbuf.st_mode)))) {
		mmgui_module_pppd_set_connection_status(mmguicore, FALSE);
		g_debug("Device not suitable\n");
		return TRUE;
	}
	
	devmajor = major(statbuf.st_rdev);
	devminor = minor(statbuf.st_rdev);
	
	memset(sysfspath, 0, sizeof(sysfspath));
	memset(sysfslink, 0, sizeof(sysfslink));
	
	snprintf(sysfspath, sizeof(sysfspath), MODULE_INT_PPPD_SYSFS_DEVICE_PATH, devmajor, devminor);
	
	status = readlink((const char *)sysfspath, sysfslink, sizeof(sysfslink));
	
	if (status == -1) {
		mmgui_module_pppd_set_connection_status(mmguicore, FALSE);
		mmgui_module_handle_error_message(mmguicorelc, "Device sysfs path not available");
		return TRUE;
	}
	
	sysfslink[status] = '\0';
	
	if (mmgui_module_pppd_get_device_serial(sysfslink, moduledata->connserial, sizeof(moduledata->connserial)) == NULL) {
		mmgui_module_pppd_set_connection_status(mmguicore, FALSE);
		mmgui_module_handle_error_message(mmguicorelc, "Device serial not available");
		return TRUE;
	}
	
	if (g_str_equal(moduledata->connserial, moduledata->devserial)) {
		mmgui_module_pppd_set_connection_status(mmguicore, TRUE);
	} else {
		mmgui_module_pppd_set_connection_status(mmguicore, FALSE);
	}
	
	return TRUE;
}

G_MODULE_EXPORT guint64 mmgui_module_device_connection_timestamp(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	/*moduledata_t moduledata;*/
	gchar lockfilepath[128];
	guint64 timestamp;
	struct stat statbuf;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	/*if (mmguicorelc->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;*/
	
	if (mmguicorelc->device == NULL) return FALSE;
	
	/*Get current timestamp*/
	timestamp = (guint64)time(NULL);
	
	/*Form lock file path*/
	memset(lockfilepath, 0, sizeof(lockfilepath));
	g_snprintf(lockfilepath, sizeof(lockfilepath), MODULE_INT_PPPD_LOCK_FILE_PATH, mmguicorelc->device->interface);
	/*Get lock file modification timestamp*/
	if (stat(lockfilepath, &statbuf) == 0) {
		timestamp = (guint64)statbuf.st_mtime;
	}
	
	return timestamp;
}

G_MODULE_EXPORT gchar *mmgui_module_device_connection_get_active_uuid(gpointer mmguicore)
{
	return NULL;
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_connect(gpointer mmguicore, mmguiconn_t connection)
{
	return FALSE;
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_disconnect(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	gchar *stderrdata = NULL;
	gint exitstatus = 0;
	gchar *argv[3] = {"/sbin/ifdown", NULL, NULL};
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (moduledata->devserial[0] == '\0') return FALSE;
	
	//If device already disconnected, return TRUE
	if (!mmguicorelc->device->connected) return TRUE;
	
	error = NULL;
	argv[1] = mmguicorelc->device->interface;
	
	if(g_spawn_sync(NULL, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL, NULL, NULL, NULL, &stderrdata, &exitstatus, &error)) {
		//Disconnected - update device state
		memset(mmguicorelc->device->interface, 0, sizeof(mmguicorelc->device->interface));
		mmguicorelc->device->connected = FALSE;
		return TRUE;
	} else {
		//Failed to disconnect
		if (error != NULL) {
			mmgui_module_handle_error_message(mmguicorelc, error->message);
			g_error_free(error);
		} else if (stderrdata != NULL) {
			mmgui_module_handle_error_message(mmguicorelc, stderrdata);
			g_free(stderrdata);
		}
		return FALSE;
	}
}
