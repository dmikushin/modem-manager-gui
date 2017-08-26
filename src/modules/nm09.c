/*
 *      nm09.c
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
#include <string.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../mmguicore.h"

#define MMGUI_MODULE_SERVICE_NAME  "org.freedesktop.NetworkManager"
#define MMGUI_MODULE_SYSTEMD_NAME  "NetworkManager.service"
#define MMGUI_MODULE_IDENTIFIER    90
#define MMGUI_MODULE_DESCRIPTION   "Network Manager >= 0.9.0"

//Internal definitions
#define MODULE_INT_NM_TIMESTAMPS_FILE_PATH     "/var/lib/NetworkManager/timestamps"
#define MODULE_INT_NM_TIMESTAMPS_FILE_SECTION  "timestamps"

//Internal enumerations
//Modem state internal flags
typedef enum {
	MODULE_INT_DEVICE_STATE_UNKNOWN = 0,        //The device is in an unknown state.
	MODULE_INT_DEVICE_STATE_UNMANAGED = 10,     //The device is recognized but not managed by NetworkManager.
	MODULE_INT_DEVICE_STATE_UNAVAILABLE = 20,   //The device cannot be used (carrier off, rfkill, etc).
	MODULE_INT_DEVICE_STATE_DISCONNECTED = 30,  //The device is not connected.
	MODULE_INT_DEVICE_STATE_PREPARE = 40,       //The device is preparing to connect.
	MODULE_INT_DEVICE_STATE_CONFIG = 50,        //The device is being configured.
	MODULE_INT_DEVICE_STATE_NEED_AUTH = 60,     //The device is awaiting secrets necessary to continue connection.
	MODULE_INT_DEVICE_STATE_IP_CONFIG = 70,     //The IP settings of the device are being requested and configured.
	MODULE_INT_DEVICE_STATE_IP_CHECK = 80,      //The device's IP connectivity ability is being determined.
	MODULE_INT_DEVICE_STATE_SECONDARIES = 90,   //The device is waiting for secondary connections to be activated.
	MODULE_INT_DEVICE_STATE_ACTIVATED = 100,    //The device is active.
	MODULE_INT_DEVICE_STATE_DEACTIVATING = 110, //The device's network connection is being torn down.
	MODULE_INT_DEVICE_STATE_FAILED = 120        //The device is in a failure state following an attempt to activate it.
} ModuleIntDeviceState;

//Device type internal flags
typedef enum {
	MODULE_INT_DEVICE_TYPE_UNKNOWN = 0,    //The device type is unknown.
	MODULE_INT_DEVICE_TYPE_ETHERNET = 1,   //The device is wired Ethernet device.
	MODULE_INT_DEVICE_TYPE_WIFI = 2,       //The device is an 802.11 WiFi device.
	MODULE_INT_DEVICE_TYPE_UNUSED1 = 3,    //Unused
	MODULE_INT_DEVICE_TYPE_UNUSED2 = 4,    //Unused
	MODULE_INT_DEVICE_TYPE_BT = 5,         //The device is Bluetooth device that provides PAN or DUN capabilities.
	MODULE_INT_DEVICE_TYPE_OLPC_MESH = 6,  //The device is an OLPC mesh networking device.
	MODULE_INT_DEVICE_TYPE_WIMAX = 7,      //The device is an 802.16e Mobile WiMAX device.
	MODULE_INT_DEVICE_TYPE_MODEM = 8,      //The device is a modem supporting one or more of analog telephone, CDMA/EVDO, GSM/UMTS/HSPA, or LTE standards to access a cellular or wireline data network.
	MODULE_INT_DEVICE_TYPE_INFINIBAND = 9, //The device is an IP-capable InfiniBand interface.
	MODULE_INT_DEVICE_TYPE_BOND = 10,      //The device is a bond master interface.
	MODULE_INT_DEVICE_TYPE_VLAN = 11,      //The device is a VLAN interface.
	MODULE_INT_DEVICE_TYPE_ADSL = 12,      //The device is an ADSL device supporting PPPoE and PPPoATM protocols.
	MODULE_INT_DEVICE_TYPE_BRIDGE = 13     //The device is a bridge interface.
} ModuleIntDeviceType;

//Active connection state internal flags
typedef enum {
	MODULE_INT_ACTIVE_CONNECTION_STATE_UNKNOWN = 0,      /*The active connection is in an unknown state.*/
	MODULE_INT_ACTIVE_CONNECTION_STATE_ACTIVATING = 1,   /*The connection is activating.*/
	MODULE_INT_ACTIVE_CONNECTION_STATE_ACTIVATED = 2,    /*The connection is activated.*/
	MODULE_INT_ACTIVE_CONNECTION_STATE_DEACTIVATING = 3, /*The connection is being torn down and cleaned up.*/
	MODULE_INT_ACTIVE_CONNECTION_STATE_DEACTIVATED = 4   /*The connection is no longer active.*/
} ModuleIntActiveConnectionState;

//Private module variables
struct _mmguimoduledata {
	//DBus connection
	GDBusConnection *connection;
	//DBus proxy objects
	GDBusProxy *nmproxy;
	//Dbus object paths
	gchar *nmdevpath;
	//Last error message
	gchar *errormessage;
};

typedef struct _mmguimoduledata *moduledata_t;


static void mmgui_module_handle_error_message(mmguicore_t mmguicore, GError *error)
{
	moduledata_t moduledata;
	
	if ((mmguicore == NULL) || (error == NULL)) return;
	
	moduledata = (moduledata_t)mmguicore->cmoduledata;
	
	if (moduledata == NULL) return;
	
	if (moduledata->errormessage != NULL) {
		g_free(moduledata->errormessage);
	}
	
	if (error->message != NULL) {
		moduledata->errormessage = g_strdup(error->message);
	} else {
		moduledata->errormessage = g_strdup("Unknown error");
	}
	
	g_warning("%s: %s", MMGUI_MODULE_DESCRIPTION, moduledata->errormessage);
}

G_MODULE_EXPORT gboolean mmgui_module_init(mmguimodule_t module)
{
	if (module == NULL) return FALSE;
	
	module->type = MMGUI_MODULE_TYPE_CONNECTION_MANGER;
	module->requirement = MMGUI_MODULE_REQUIREMENT_SERVICE;
	module->priority = MMGUI_MODULE_PRIORITY_NORMAL;
	module->identifier = MMGUI_MODULE_IDENTIFIER;
	module->functions = MMGUI_MODULE_FUNCTION_BASIC;
	g_snprintf(module->servicename, sizeof(module->servicename), MMGUI_MODULE_SERVICE_NAME);
	g_snprintf(module->systemdname, sizeof(module->systemdname), MMGUI_MODULE_SYSTEMD_NAME);
	g_snprintf(module->description, sizeof(module->description), MMGUI_MODULE_DESCRIPTION);
	
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_connection_open(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t *moduledata;
	GError *error;
	
	if (mmguicore == NULL) return FALSE;
	
	mmguicorelc = (mmguicore_t)mmguicore;
	
	moduledata = (moduledata_t *)&mmguicorelc->cmoduledata;
	
	(*moduledata) = g_new0(struct _mmguimoduledata, 1);
	
	error = NULL;
	
	(*moduledata)->connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	
	(*moduledata)->errormessage = NULL;
	
	if (((*moduledata)->connection == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_free(mmguicorelc->moduledata);
		return FALSE;
	}
	
	error = NULL;
	
	(*moduledata)->nmproxy = g_dbus_proxy_new_sync((*moduledata)->connection,
													G_DBUS_PROXY_FLAGS_NONE,
													NULL,
													"org.freedesktop.NetworkManager",
													"/org/freedesktop/NetworkManager",
													"org.freedesktop.NetworkManager",
													NULL,
													&error);
		
	if (((*moduledata)->nmproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_object_unref((*moduledata)->connection);
		g_free(mmguicorelc->cmoduledata);
		return FALSE;
	}
	
	(*moduledata)->nmdevpath = NULL;
		
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
		
		if (moduledata->nmproxy != NULL) {
			g_object_unref(moduledata->nmproxy);
			moduledata->nmproxy = NULL;
		}
		
		if (moduledata->connection != NULL) {
			g_object_unref(moduledata->connection);
			moduledata->connection = NULL;
		}
		
		g_free(moduledata);
	}
	
	return TRUE;
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
	GError *error;
	GVariant *nmdevices;
	GVariantIter nmdeviterl1, nmdeviterl2;
	GVariant *nmdevnodel1, *nmdevnodel2;
	gsize strlength;
	const gchar *valuestr;
	GDBusProxy *nmdevproxy;
	GVariant *devproperties;
	const gchar *nmdevpath/*, *nmactconnpath*/;
	guint nmdevtype;
	
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (device == NULL) return FALSE;	
	if (device->objectpath == NULL) return FALSE;
	
	error = NULL;
	
	//First time just set it to NULL
	moduledata->nmdevpath = NULL;
	
	/*Initialize local variables*/
	nmdevtype = MODULE_INT_DEVICE_TYPE_UNKNOWN;
	nmdevpath = NULL;
	
	//Network Manager interface
	if (moduledata->nmproxy != NULL) {
		nmdevices = g_dbus_proxy_call_sync(moduledata->nmproxy, "GetDevices", NULL, 0, -1, NULL, &error);
		if ((nmdevices != NULL) && (error == NULL)) {
			g_variant_iter_init(&nmdeviterl1, nmdevices);
			while ((nmdevnodel1 = g_variant_iter_next_value(&nmdeviterl1)) != NULL) {
				g_variant_iter_init(&nmdeviterl2, nmdevnodel1);
				while ((nmdevnodel2 = g_variant_iter_next_value(&nmdeviterl2)) != NULL) {
					//Device path
					strlength = 256;
					valuestr = g_variant_get_string(nmdevnodel2, &strlength);
					//Device proxy
					error = NULL;
					nmdevproxy = g_dbus_proxy_new_sync(moduledata->connection,
														G_DBUS_PROXY_FLAGS_NONE,
														NULL,
														"org.freedesktop.NetworkManager",
														valuestr,
														"org.freedesktop.NetworkManager.Device",
														NULL,
														&error);
					if ((nmdevproxy != NULL) && (error == NULL)) {
						/*Device path*/
						devproperties = g_dbus_proxy_get_cached_property(nmdevproxy, "Udi");
						if (devproperties != NULL) {
							strlength = 256;
							nmdevpath = g_variant_get_string(devproperties, &strlength);
							g_variant_unref(devproperties);
						}
						/*Device type*/
						devproperties = g_dbus_proxy_get_cached_property(nmdevproxy, "DeviceType");
						if (devproperties != NULL) {
							nmdevtype = g_variant_get_uint32(devproperties);
							g_variant_unref(devproperties);
						}
						/*Active connection*/
						/*devproperties = g_dbus_proxy_get_cached_property(nmdevproxy, "ActiveConnection");
						if (devproperties != NULL) {
							strlength = 256;
							nmactconnpath = g_variant_get_string(devproperties, &strlength);
							g_variant_unref(devproperties);
						}*/
						/*Is it device we looking for*/
						if ((nmdevtype == MODULE_INT_DEVICE_TYPE_MODEM) && (g_str_equal(device->objectpath, nmdevpath))) {
							/*Cache device path*/
							moduledata->nmdevpath = g_strdup(valuestr);
							g_object_unref(nmdevproxy);
							break;
						} else {
							g_object_unref(nmdevproxy);
						}
					} else {
						//Failed to create Network Manager device proxy
						/*mmgui_module_device_connection_get_timestamp(mmguicorelc);*/
						g_error_free(error);
					}
					g_variant_unref(nmdevnodel2);
				}
				g_variant_unref(nmdevnodel1);
			}
			g_variant_unref(nmdevices);
		} else {
			//No devices found by Network Manager
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
		}
	}
		
	if (moduledata->nmdevpath != NULL) {
		return TRUE;
	} else {
		return FALSE;
	}
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_close(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
					
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (moduledata->nmdevpath != NULL) {
		g_free(moduledata->nmdevpath);
		moduledata->nmdevpath = NULL;
	}
		
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_status(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GDBusProxy *nmdevproxy;
	GVariant *devproperty;
	const gchar *devinterface;
	guint devstate;
	gsize strlength;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (moduledata->nmdevpath == NULL) return FALSE;
	
	error = NULL;
	
	//Device proxy
	nmdevproxy = g_dbus_proxy_new_sync(moduledata->connection,
										G_DBUS_PROXY_FLAGS_NONE,
										NULL,
										"org.freedesktop.NetworkManager",
										moduledata->nmdevpath,
										"org.freedesktop.NetworkManager.Device",
										NULL,
										&error);
	
	if ((nmdevproxy != NULL) && (error == NULL)) {
		devproperty = g_dbus_proxy_get_cached_property(nmdevproxy, "State");
		devstate = g_variant_get_uint32(devproperty);
		g_variant_unref(devproperty);
		//If device connected, get interface name
		if (devstate == MODULE_INT_DEVICE_STATE_ACTIVATED) {
			devproperty = g_dbus_proxy_get_cached_property(nmdevproxy, "IpInterface");
			strlength = 256;
			devinterface = g_variant_get_string(devproperty, &strlength);
			memset(mmguicorelc->device->interface, 0, IFNAMSIZ);
			strncpy(mmguicorelc->device->interface, devinterface, IFNAMSIZ);
			mmguicorelc->device->connected = TRUE;
			g_variant_unref(devproperty);
		} else {
			memset(mmguicorelc->device->interface, 0, IFNAMSIZ);
			mmguicorelc->device->connected = FALSE;
		} 
		g_object_unref(nmdevproxy);
	} else {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
	}
	
	return TRUE;
}

G_MODULE_EXPORT guint64 mmgui_module_device_connection_timestamp(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GDBusProxy *devproxy;
	GDBusProxy *connproxy;
	GVariant *property;
	guint64 curts, realts;
	const gchar *connpath;
	const gchar *connuuid;
	GKeyFile *tsfile;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (moduledata->nmdevpath == NULL) return FALSE;
	
	/*Get current timestamp*/
	curts = (guint64)time(NULL);
	
	error = NULL;
	
	/*Device proxy*/
	devproxy = g_dbus_proxy_new_sync(moduledata->connection,
										G_DBUS_PROXY_FLAGS_NONE,
										NULL,
										"org.freedesktop.NetworkManager",
										moduledata->nmdevpath,
										"org.freedesktop.NetworkManager.Device",
										NULL,
										&error);
	
	if ((devproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return curts;
	}
	
	/*Get path to Active Connection interface*/
	property = g_dbus_proxy_get_cached_property(devproxy, "ActiveConnection");
	connpath = g_variant_get_string(property, NULL);
	
	/*Active Connection proxy*/
	connproxy = g_dbus_proxy_new_sync(moduledata->connection,
										G_DBUS_PROXY_FLAGS_NONE,
										NULL,
										"org.freedesktop.NetworkManager",
										connpath,
										"org.freedesktop.NetworkManager.Connection.Active",
										NULL,
										&error);
	
	if ((connproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_variant_unref(property);
		g_object_unref(devproxy);
		return curts;
	}
	
	/*Free first proxy resources*/
	g_variant_unref(property);
	g_object_unref(devproxy);
	
	/*Get active connection UUID*/
	property = g_dbus_proxy_get_cached_property(connproxy, "Uuid");
	connuuid = g_variant_get_string(property, NULL);
	
	/*Retrieve timestamp from Network Manager timestamps ini file*/
	tsfile = g_key_file_new();
	
	if (!g_key_file_load_from_file(tsfile, MODULE_INT_NM_TIMESTAMPS_FILE_PATH, G_KEY_FILE_NONE, &error)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_key_file_free(tsfile);
		g_variant_unref(property);
		g_object_unref(connproxy);
		return curts;
	}
	
	/*File has only one section 'timestamps' with elements 'uuid=timestamp'*/
	realts = g_key_file_get_uint64(tsfile, MODULE_INT_NM_TIMESTAMPS_FILE_SECTION, connuuid, &error);
	
	if ((realts == 0) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_key_file_free(tsfile);
		g_variant_unref(property);
		g_object_unref(connproxy);
		return curts;
	}
	
	/*Free second proxy and ini file resources*/
	g_key_file_free(tsfile);
	g_variant_unref(property);
	g_object_unref(connproxy);
	
	return realts;
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_disconnect(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GDBusProxy *nmdevproxy;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (moduledata->nmdevpath == NULL) return FALSE;
	
	//If device already disconnected, return TRUE
	if (!mmguicorelc->device->connected) return TRUE;
	
	error = NULL;
	
	//Device proxy
	nmdevproxy = g_dbus_proxy_new_sync(moduledata->connection,
										G_DBUS_PROXY_FLAGS_NONE,
										NULL,
										"org.freedesktop.NetworkManager",
										moduledata->nmdevpath,
										"org.freedesktop.NetworkManager.Device",
										NULL,
										&error);
	
	if ((nmdevproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	}
	
	//Call disconnect method
	g_dbus_proxy_call_sync(nmdevproxy, "Disconnect", NULL, 0, -1, NULL, &error);
	
	if (error != NULL) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_object_unref(nmdevproxy);
		return FALSE;
	}
	
	g_object_unref(nmdevproxy);
	
	//Update device state
	mmguicorelc->device->connected = FALSE;
	
	return TRUE;	
}
