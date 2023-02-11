/*
 *      nm09.c
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
#include <string.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include <stdio.h>

#include "uuid.h"
#include "../mmguicore.h"

#define MMGUI_MODULE_SERVICE_NAME  "org.freedesktop.NetworkManager"
#define MMGUI_MODULE_SYSTEMD_NAME  "NetworkManager.service"
#define MMGUI_MODULE_IDENTIFIER    90
#define MMGUI_MODULE_DESCRIPTION   "Network Manager >= 0.9.0"
#define MMGUI_MODULE_COMPATIBILITY "org.freedesktop.ModemManager;org.freedesktop.ModemManager1;"

//Internal definitions
#define MODULE_INT_NM_TIMESTAMPS_FILE_PATH     "/var/run/modem-manager-gui/timestamps"
#define MODULE_INT_NM_TIMESTAMPS_FILE_SECTION  "timestamps"
#define MODULE_INT_NM_ERROR_CODE_NO_SECRETS    36

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

/*Private module variables*/
struct _mmguimoduledata {
	/*DBus connection*/
	GDBusConnection *connection;
	/*DBus proxy objects*/
	GDBusProxy *nmproxy;
	GDBusProxy *setproxy;
	GDBusProxy *devproxy;
	/*Dbus object paths*/
	//gchar *nmdevpath;
	/*Signal handlers*/
	gulong statesignal;
	/*Internal state flags*/
	gboolean opinitiated;
	gboolean opstate;
	/*Last error message*/
	gchar *errormessage;
	/*UUID RNG*/
	GRand *uuidrng;
	/*Version information*/
	gint vermajor;
	gint verminor;
	gint verrevision;
};

typedef struct _mmguimoduledata *moduledata_t;


static void mmgui_module_get_updated_interface_state(mmguicore_t mmguicore, gboolean checkstate);
static void mmgui_module_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data);
static gchar *mmgui_module_get_variant_string(GVariant *variant, const gchar *name, const gchar *defvalue);
static gboolean mmgui_module_get_variant_boolean(GVariant *variant, const gchar *name, gboolean defvalue);
static void mmgui_module_handle_error_message(mmguicore_t mmguicore, GError *error);
static gboolean mmgui_module_check_service_version(mmguicore_t mmguicore, gint major, gint minor, gint revision);
static mmguiconn_t mmgui_module_connection_get_params(mmguicore_t mmguicore, const gchar *connpath);
static GVariant *mmgui_module_connection_serialize(const gchar *uuid, const gchar *name, const gchar *number, const gchar *username, const gchar *password, const gchar *apn, guint networkid, guint type, gboolean homeonly, const gchar *dns1, const gchar *dns2);


static void mmgui_module_get_updated_interface_state(mmguicore_t mmguicore, gboolean checkstate)
{
	moduledata_t moduledata;
	GDBusProxy *devproxy;
	GError *error;
	GVariant *devproperty;
	guint devstate;
	const gchar *devinterface;
	gsize strlength;
	
	if (mmguicore == NULL) return;
	
	moduledata = (moduledata_t)(mmguicore->cmoduledata);
	
	error = NULL;
	
	if (!mmgui_module_check_service_version(mmguicore, 1, 0, 0)) {
		/*Old NetworkManager does not update properties on opened proxy objects*/
		devproxy = g_dbus_proxy_new_sync(moduledata->connection,
										G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
										NULL,
										"org.freedesktop.NetworkManager",
										g_dbus_proxy_get_object_path(moduledata->devproxy),
										"org.freedesktop.NetworkManager.Device",
										NULL,
										&error);
	} else {
		devproxy = moduledata->devproxy;
	}
	
	if ((devproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
		return;
	} else {
		if (checkstate) {
			devproperty = g_dbus_proxy_get_cached_property(devproxy, "State");
			devstate = g_variant_get_uint32(devproperty);
			g_variant_unref(devproperty);
		}
		
		if ((!checkstate) || (checkstate && (devstate == MODULE_INT_DEVICE_STATE_ACTIVATED))) {
			devproperty = g_dbus_proxy_get_cached_property(devproxy, "IpInterface");
			if (devproperty != NULL) {
				strlength = IFNAMSIZ;
				devinterface = g_variant_get_string(devproperty, &strlength);
				if ((devinterface != NULL) && (devinterface[0] != '\0')) {
					memset(mmguicore->device->interface, 0, IFNAMSIZ);
					strncpy(mmguicore->device->interface, devinterface, IFNAMSIZ-1);
					mmguicore->device->connected = TRUE;
				}
				g_variant_unref(devproperty);
			}
		} else if (checkstate && (devstate != MODULE_INT_DEVICE_STATE_ACTIVATED)) {
			memset(mmguicore->device->interface, 0, IFNAMSIZ);
			mmguicore->device->connected = FALSE;
		}
		
		if (!mmgui_module_check_service_version(mmguicore, 1, 0, 0)) {
			g_object_unref(devproxy);
		}
	}
}

static void mmgui_module_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	guint oldstate, newstate, reason;
		
	if (data == NULL) return;
	
	mmguicorelc = (mmguicore_t)data;
	moduledata = (moduledata_t)(mmguicorelc->cmoduledata);
	
	if (g_str_equal(signal_name, "StateChanged")) {
		g_variant_get(parameters, "(uuu)", &newstate, &oldstate, &reason);
		switch (newstate) {
			case MODULE_INT_DEVICE_STATE_DISCONNECTED:
				/*Update internal info*/
				memset(mmguicorelc->device->interface, 0, IFNAMSIZ);
				mmguicorelc->device->connected = FALSE;
				/*Update connection transition flag*/
				mmguicorelc->device->conntransition = FALSE;
				/*Generate signals*/
				if (moduledata->opinitiated) {
					/*Connection deactivated by MMGUI*/
					if (mmguicorelc->eventcb != NULL) {
						(mmguicorelc->eventcb)(MMGUI_EVENT_MODEM_CONNECTION_RESULT, mmguicorelc, GUINT_TO_POINTER(moduledata->opstate));
					}
					moduledata->opinitiated = FALSE;
					moduledata->opstate = FALSE;
				} else {
					/*Connection deactivated by another mean*/
					if (mmguicorelc->eventcb != NULL) {
						(mmguicorelc->eventcb)(MMGUI_EVENT_DEVICE_CONNECTION_STATUS, mmguicorelc, GUINT_TO_POINTER(FALSE));
					}
				}
				break;
			case MODULE_INT_DEVICE_STATE_ACTIVATED:
				/*Update internal info*/
				mmgui_module_get_updated_interface_state(mmguicorelc, FALSE);
				/*Update connection transition flag*/
				mmguicorelc->device->conntransition = FALSE;
				/*Generate signals*/
				if (moduledata->opinitiated) {
					/*Connection activated by MMGUI*/
					if (mmguicorelc->eventcb != NULL) {
						(mmguicorelc->eventcb)(MMGUI_EVENT_MODEM_CONNECTION_RESULT, mmguicorelc, GUINT_TO_POINTER(moduledata->opstate));
					}
					moduledata->opinitiated = FALSE;
					moduledata->opstate = FALSE;
				} else {
					/*Connection activated by another mean*/
					if (mmguicorelc->eventcb != NULL) {
						(mmguicorelc->eventcb)(MMGUI_EVENT_DEVICE_CONNECTION_STATUS, mmguicorelc, GUINT_TO_POINTER(TRUE));
					}
				}
				break;
			case MODULE_INT_DEVICE_STATE_FAILED:
				/*Operation state flag*/
				moduledata->opstate = FALSE;
				/*Update connection transition flag*/
				mmguicorelc->device->conntransition = TRUE;
				break;
			default:
				/*Update connection transition flag*/
				mmguicorelc->device->conntransition = TRUE;
				break;
		}
		g_debug("State change: %u - %u - %u\n", oldstate, newstate, reason);
	}
}

static gchar *mmgui_module_get_variant_string(GVariant *variant, const gchar *name, const gchar *defvalue)
{
	GVariant *strvar;
	const gchar *str;
	gchar *res;
	
	if ((variant == NULL) || (name == NULL)) return NULL;
	
	res = NULL;
	
	strvar = g_variant_lookup_value(variant, name, G_VARIANT_TYPE_STRING);
	if (strvar != NULL) {
		str = g_variant_get_string(strvar, NULL);
		if ((str != NULL) && (str[0] != '\0')) {
			res = g_strdup(str);
		} else {
			if (defvalue != NULL) {
				res = g_strdup(defvalue);
			}
		}
		g_variant_unref(strvar);
	}
	
	return res;
}

static gboolean mmgui_module_get_variant_boolean(GVariant *variant, const gchar *name, gboolean defvalue)
{
	GVariant *booleanvar;
	gboolean res;
	
	if ((variant == NULL) || (name == NULL)) return defvalue;
	
	res = defvalue;
	
	booleanvar = g_variant_lookup_value(variant, name, G_VARIANT_TYPE_BOOLEAN);
	if (booleanvar != NULL) {
		res = g_variant_get_boolean(booleanvar);
		g_variant_unref(booleanvar);
	}
	
	return res;
}

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

static gboolean mmgui_module_check_service_version(mmguicore_t mmguicore, gint major, gint minor, gint revision)
{
	moduledata_t moduledata;
	gboolean compatiblever;
	
	if (mmguicore == NULL) return FALSE;
	
	moduledata = (moduledata_t)mmguicore->cmoduledata;
	
	compatiblever = (moduledata->vermajor >= major);
	
	if ((moduledata->vermajor == major) && (minor != -1)) {
		compatiblever = compatiblever && (moduledata->verminor >= minor);
		if ((moduledata->verminor == minor) && (revision != -1)) {
			compatiblever = compatiblever && (moduledata->verrevision >= revision); 
		}
	}
		
	return compatiblever;
}

G_MODULE_EXPORT gboolean mmgui_module_init(mmguimodule_t module)
{
	if (module == NULL) return FALSE;
	
	module->type = MMGUI_MODULE_TYPE_CONNECTION_MANGER;
	module->requirement = MMGUI_MODULE_REQUIREMENT_SERVICE;
	module->priority = MMGUI_MODULE_PRIORITY_NORMAL;
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
	GError *error;
	GVariant *version;
	const gchar *verstr;
	gchar **verstrarr;
	gint i;
	
	if (mmguicore == NULL) return FALSE;
	
	mmguicorelc = (mmguicore_t)mmguicore;
	
	mmguicorelc->cmcaps = MMGUI_CONNECTION_MANAGER_CAPS_BASIC | MMGUI_CONNECTION_MANAGER_CAPS_MANAGEMENT | MMGUI_CONNECTION_MANAGER_CAPS_MONITORING;
	
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
	
	/*Get version of the system service*/
	(*moduledata)->vermajor = 0;
	(*moduledata)->verminor = 0;
	(*moduledata)->verrevision = 0;
	version = g_dbus_proxy_get_cached_property((*moduledata)->nmproxy, "Version");
	if (version != NULL) {
		verstr = g_variant_get_string(version, NULL);
		if ((verstr != NULL) && (verstr[0] != '\0')) {
			verstrarr = g_strsplit(verstr, ".", -1);
			if (verstrarr != NULL) {
				i = 0;
				while (verstrarr[i] != NULL) {
					switch (i) {
						case 0:
							(*moduledata)->vermajor = atoi(verstrarr[i]);
							break;
						case 1:
							(*moduledata)->verminor = atoi(verstrarr[i]);
							break;
						case 2:
							(*moduledata)->verrevision = atoi(verstrarr[i]);
							break;
						default:
							break;
					}
					i++;
				}
				g_strfreev(verstrarr);
			}
		}
		g_variant_unref(version);
	}
	
	(*moduledata)->setproxy = g_dbus_proxy_new_sync((*moduledata)->connection,
													G_DBUS_PROXY_FLAGS_NONE,
													NULL,
													"org.freedesktop.NetworkManager",
													"/org/freedesktop/NetworkManager/Settings",
													"org.freedesktop.NetworkManager.Settings",
													NULL,
													&error);
		
	if (((*moduledata)->setproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_object_unref((*moduledata)->connection);
		g_free(mmguicorelc->cmoduledata);
		return FALSE;
	}
	
	//(*moduledata)->nmdevpath = NULL;
	
	(*moduledata)->devproxy = NULL;
	
	(*moduledata)->uuidrng = mmgui_uuid_init();
		
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
		if (moduledata->uuidrng != NULL) {
			g_rand_free(moduledata->uuidrng);
		}
		
		if (moduledata->errormessage != NULL) {
			g_free(moduledata->errormessage);
		}
		
		if (moduledata->setproxy != NULL) {
			g_object_unref(moduledata->setproxy);
			moduledata->setproxy = NULL;
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

static mmguiconn_t mmgui_module_connection_get_params(mmguicore_t mmguicore, const gchar *connpath)
{
	moduledata_t moduledata;
	GDBusProxy *connproxy;
	GError *error;
	GVariant *conninfo, *passinfo;
	GVariant *connparams, *passparams;
	GVariant *connconsec, *connipv4sec, *conntechsec;
	GVariant *conndnsvar;
	gchar *conntypestr, *connparamstr;
	gint i, addrint;
	GVariant *addrvar;
	gchar *techstr;
	mmguiconn_t connection;
		
	if ((mmguicore == NULL) || (connpath == NULL)) return NULL;
	
	if (mmguicore->cmoduledata == NULL) return NULL;
	
	moduledata = (moduledata_t)mmguicore->cmoduledata;
	
	connection = NULL;
	techstr = "gsm";
	
	error = NULL;
	
	connproxy = g_dbus_proxy_new_sync(moduledata->connection,
										G_DBUS_PROXY_FLAGS_NONE,
										NULL,
										"org.freedesktop.NetworkManager",
										connpath,
										"org.freedesktop.NetworkManager.Settings.Connection",
										NULL,
										&error);
	
	if (error != NULL) {
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
		return NULL;
	}
	
	conninfo = g_dbus_proxy_call_sync(connproxy,
										"GetSettings",
										NULL,
										0,
										-1,
										NULL,
										&error);
	
	if (error != NULL) {
		g_object_unref(connproxy);
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
		return NULL;
	}
	
	connparams = g_variant_get_child_value(conninfo, 0);
	if (connparams != NULL) {
		connconsec = g_variant_lookup_value(connparams, "connection", G_VARIANT_TYPE_ARRAY);
		if (connconsec != NULL) {
			conntypestr = mmgui_module_get_variant_string(connconsec, "type", NULL);
			if (conntypestr != NULL) {
				if (g_str_equal(conntypestr, "gsm") || (g_str_equal(conntypestr, "cdma"))) {
					connection = g_new0(struct _mmguiconn, 1);
					/*UUID*/
					connection->uuid = mmgui_module_get_variant_string(connconsec, "uuid", NULL);
					/*Name*/
					connection->name = mmgui_module_get_variant_string(connconsec, "id", NULL);
					/*GSM section*/
					if (g_str_equal(conntypestr, "gsm")) {
						conntechsec = g_variant_lookup_value(connparams, "gsm", G_VARIANT_TYPE_ARRAY);
						if (conntechsec != NULL) {
							/*Number*/
							connection->number = mmgui_module_get_variant_string(conntechsec, "number", NULL);
							/*Username*/
							connection->username = mmgui_module_get_variant_string(conntechsec, "username", NULL);
							/*APN*/
							connection->apn = mmgui_module_get_variant_string(conntechsec, "apn", NULL);
							/*Network ID*/
							connparamstr = mmgui_module_get_variant_string(conntechsec, "network-id", NULL);
							if (connparamstr != NULL) {
								connection->networkid = (guint)atoi(connparamstr);
								g_free(connparamstr);
							}
							/*Home only*/
							connection->homeonly = mmgui_module_get_variant_boolean(conntechsec, "home-only", FALSE);
							/*Type*/
							connection->type = MMGUI_DEVICE_TYPE_GSM;
							techstr = "gsm";
							/*Free resources*/
							g_variant_unref(conntechsec);
						}
					} else if (g_str_equal(conntypestr, "cdma")) {
						conntechsec = g_variant_lookup_value(connparams, "cdma", G_VARIANT_TYPE_ARRAY);
						if (conntechsec != NULL) {
							/*Number*/
							connection->number = mmgui_module_get_variant_string(conntechsec, "number", NULL);
							/*Username*/
							connection->username = mmgui_module_get_variant_string(conntechsec, "username", NULL);
							/*Type*/
							connection->type = MMGUI_DEVICE_TYPE_CDMA;
							techstr = "cdma";
							/*Free resources*/
							g_variant_unref(conntechsec);
						}
					}
					/*ipv4 section*/
					connipv4sec = g_variant_lookup_value(connparams, "ipv4", G_VARIANT_TYPE_ARRAY);
					if (connipv4sec != NULL) {
						/*DNS*/
						conndnsvar = g_variant_lookup_value(connipv4sec, "dns", G_VARIANT_TYPE_ARRAY);
						if (conndnsvar) {
                            for (i = 0; i < g_variant_n_children(conndnsvar); i++) {
							    addrvar = g_variant_get_child_value(conndnsvar, i);
							    addrint = ntohl(g_variant_get_uint32(addrvar));
							    if (connection->dns1 == NULL) {
								    connection->dns1 = g_strdup_printf("%u.%u.%u.%u", (addrint >> 24) & 0xFF, (addrint >> 16) & 0xFF, (addrint >> 8) & 0xFF, addrint & 0xFF);
							    } else if (connection->dns2 == NULL) {
								    connection->dns2 = g_strdup_printf("%u.%u.%u.%u", (addrint >> 24) & 0xFF, (addrint >> 16) & 0xFF, (addrint >> 8) & 0xFF, addrint & 0xFF);
							    }
							    g_variant_unref(addrvar);
						    }
                            g_variant_unref(conndnsvar);
                        }
						g_variant_unref(connipv4sec);
                    }
					/*Password*/
					passinfo = g_dbus_proxy_call_sync(connproxy,
													"GetSecrets",
													g_variant_new("(s)", techstr),
													0,
													-1,
													NULL,
													&error);
					
					if ((passinfo != NULL) && (error == NULL)) {
						passparams = g_variant_get_child_value(passinfo, 0);
						if (passparams != NULL) {
							conntechsec = g_variant_lookup_value(passparams, techstr, G_VARIANT_TYPE_ARRAY);
							if (conntechsec != NULL) {
								/*Password*/
								connection->password = mmgui_module_get_variant_string(conntechsec, "password", NULL);
								g_variant_unref(conntechsec);
							}
							g_variant_unref(passparams);
						}
					} else {
						if (error->code != MODULE_INT_NM_ERROR_CODE_NO_SECRETS) {
							/*We can safely ignore 'NoSecrets' error*/
							mmgui_module_handle_error_message(mmguicore, error);
						}
						g_error_free(error);
					}
				}
				g_free(conntypestr);
			}
			g_variant_unref(connconsec);
		}
		g_variant_unref(connparams);
	}
	
	g_variant_unref(conninfo);
	
	g_object_unref(connproxy);
	
	return connection;
}

G_MODULE_EXPORT guint mmgui_module_connection_enum(gpointer mmguicore, GSList **connlist)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	guint connnum;
	GError *error;
	GVariant *connvar;
	GVariantIter conniter, conniter2;
	GVariant *connnode, *connnode2;
	const gchar *connpath;
	mmguiconn_t connection;
	
	if ((mmguicore == NULL) || (connlist == NULL)) return 0;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (!(mmguicorelc->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MANAGEMENT)) return 0;
	
	if (mmguicorelc->cmoduledata == NULL) return 0;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	connnum = 0;
	
	error = NULL;
	
	connvar = g_dbus_proxy_call_sync(moduledata->setproxy,
									"ListConnections",
									NULL,
									0,
									-1,
									NULL,
									&error);
										
	if (error != NULL) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return 0;
	}
	
	g_variant_iter_init(&conniter, connvar);
	
	while ((connnode = g_variant_iter_next_value(&conniter)) != NULL) {
		g_variant_iter_init(&conniter2, connnode);
		while ((connnode2 = g_variant_iter_next_value(&conniter2)) != NULL) {
			connpath = g_variant_get_string(connnode2, NULL);
			if ((connpath != NULL) && (connpath[0] != '\0')) {
				connection = mmgui_module_connection_get_params(mmguicorelc, connpath);
				if (connection != NULL) {
					*connlist = g_slist_prepend(*connlist, connection);
					connnum++;
				}
			}
			g_variant_unref(connnode2);
		}
		g_variant_unref(connnode);
    }
	
	return connnum;
}

static GVariant *mmgui_module_connection_serialize(const gchar *uuid, const gchar *name, const gchar *number, const gchar *username, const gchar *password, const gchar *apn, guint networkid, guint type, gboolean homeonly, const gchar *dns1, const gchar *dns2)
{
	GVariantBuilder *paramsbuilder, *connbuilder, *serialbuilder, *pppbuilder, *techbuilder, *ipv4builder, *ipv6builder, *dnsbuilder;
	GVariant *connparams;
	GInetAddress *address;
	guint8 *addrbytes;
	gchar strbuf[32];
	
	if ((uuid == NULL) || (name == NULL)) return NULL;
	
	/*Connection parameters*/
	connbuilder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(connbuilder, "{sv}", "id", g_variant_new_string(name));
	g_variant_builder_add(connbuilder, "{sv}", "uuid", g_variant_new_string(uuid));
	g_variant_builder_add(connbuilder, "{sv}", "autoconnect", g_variant_new_boolean(FALSE));
	if (type == MMGUI_DEVICE_TYPE_GSM) {
		g_variant_builder_add(connbuilder, "{sv}", "type", g_variant_new_string("gsm"));
	} else if (type == MMGUI_DEVICE_TYPE_CDMA) {
		g_variant_builder_add(connbuilder, "{sv}", "type", g_variant_new_string("cdma"));
	}
	/*Serial port parameters*/
	serialbuilder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(serialbuilder, "{sv}", "baud", g_variant_new_uint32(115200));
	/*PPP protocol parameters*/
	pppbuilder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(pppbuilder, "{sv}", "lcp-echo-failure", g_variant_new_uint32(5));
	g_variant_builder_add(pppbuilder, "{sv}", "lcp-echo-interval", g_variant_new_uint32(30));
	/*Broadband connection parameters*/
	techbuilder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	if ((number != NULL) && (strlen(number) > 0)) {
		g_variant_builder_add(techbuilder, "{sv}", "number", g_variant_new_string(number));
	}
	if ((username != NULL) && (strlen(username) > 0)) {
		g_variant_builder_add(techbuilder, "{sv}", "username", g_variant_new_string(username));
	}
	if ((password != NULL) && (strlen(password) > 0)) {
		g_variant_builder_add(techbuilder, "{sv}", "password", g_variant_new_string(password));
	}
	if (type == MMGUI_DEVICE_TYPE_GSM) {
		if ((apn != NULL) && (strlen(apn) > 0)) {
			g_variant_builder_add(techbuilder, "{sv}", "apn", g_variant_new_string(apn));
		}
		if (networkid > 9999) {
			memset(strbuf, 0, sizeof(strbuf));
			snprintf(strbuf, sizeof(strbuf), "%u", networkid);
			g_variant_builder_add(techbuilder, "{sv}", "network-id", g_variant_new_string(strbuf));
			g_variant_builder_add(techbuilder, "{sv}", "home-only", g_variant_new_boolean(homeonly));
		}
	}
	/*IPv4 parameters*/
	ipv4builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(ipv4builder, "{sv}", "method", g_variant_new_string("auto"));
	/*DNS*/
	if (((dns1 != NULL) && (strlen(dns1) > 0)) || ((dns2 != NULL) && (strlen(dns2) > 0))) {
		dnsbuilder = g_variant_builder_new(G_VARIANT_TYPE("au"));
		if ((dns1 != NULL) && (strlen(dns1) > 0)) {
			address = g_inet_address_new_from_string(dns1);
			if (address != NULL) {
				addrbytes = (guint8 *)g_inet_address_to_bytes(address);
				g_variant_builder_add(dnsbuilder, "u", htonl((addrbytes[0] << 24) | (addrbytes[1] << 16) | (addrbytes[2] << 8) | (addrbytes[3])));
				g_object_unref(address);
			}
		}
 		if ((dns2 != NULL) && (strlen(dns2) > 0)) {
			address = g_inet_address_new_from_string(dns2);
			if (address != NULL) {
				addrbytes = (guint8 *)g_inet_address_to_bytes(address);
				g_variant_builder_add(dnsbuilder, "u", htonl((addrbytes[0] << 24) | (addrbytes[1] << 16) | (addrbytes[2] << 8) | (addrbytes[3])));
				g_object_unref(address);
			}
		}
		g_variant_builder_add(ipv4builder, "{sv}", "dns", g_variant_new("au", dnsbuilder));
	}
	/*IPv6 parameters*/
	ipv6builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(ipv6builder, "{sv}", "method", g_variant_new_string("ignore"));
	/*Parameters representation*/
	paramsbuilder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
	g_variant_builder_add(paramsbuilder, "{sa{sv}}", "connection", connbuilder);
	g_variant_builder_add(paramsbuilder, "{sa{sv}}", "serial", serialbuilder);
	g_variant_builder_add(paramsbuilder, "{sa{sv}}", "ppp", pppbuilder);
	if (type == MMGUI_DEVICE_TYPE_GSM) {
		g_variant_builder_add(paramsbuilder, "{sa{sv}}", "gsm", techbuilder);
	} else if (type == MMGUI_DEVICE_TYPE_CDMA) {
		g_variant_builder_add(paramsbuilder, "{sa{sv}}", "cdma", techbuilder);
	}
	g_variant_builder_add(paramsbuilder, "{sa{sv}}", "ipv4", ipv4builder);
	g_variant_builder_add(paramsbuilder, "{sa{sv}}", "ipv6", ipv6builder);
	connparams = g_variant_new("(a{sa{sv}})", paramsbuilder);
	
	return connparams;
}

G_MODULE_EXPORT mmguiconn_t mmgui_module_connection_add(gpointer mmguicore, const gchar *name, const gchar *number, const gchar *username, const gchar *password, const gchar *apn, guint networkid, guint type, gboolean homeonly, const gchar *dns1, const gchar *dns2)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	mmguiconn_t connection;
	gchar *uuid;
	GVariant *connparams, *connpath;
	GError *error;
	
	if ((mmguicore == NULL) || (name == NULL)) return NULL;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (!(mmguicorelc->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MANAGEMENT)) return NULL;
	
	if (mmguicorelc->cmoduledata == NULL) return NULL;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	/*UUID*/
	uuid = mmgui_uuid_generate(moduledata->uuidrng);
	
	/*Serialize*/
	connparams = mmgui_module_connection_serialize(uuid, name, number, username, password, apn, networkid, type, homeonly, dns1, dns2);
	
	/*Add new connection*/
	error = NULL;
	connpath = g_dbus_proxy_call_sync(moduledata->setproxy,
									"AddConnection",
									connparams,
									0,
									-1,
									NULL,
									&error);
										
	if (error != NULL) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_variant_unref(connpath);
		g_variant_unref(connparams);
		g_free(uuid);
		return NULL;
	}
	
	/*Create new connection struct*/
	connection = g_new0(struct _mmguiconn, 1);
	connection->uuid = uuid;
	connection->name = g_strdup(name);
	connection->number = g_strdup(number);
	connection->username = g_strdup(username);
	connection->password = g_strdup(password);
	connection->apn = g_strdup(apn);
	connection->networkid = networkid;
	connection->type = type;
	connection->homeonly = homeonly;
	connection->dns1 = g_strdup(dns1);
	connection->dns2 = g_strdup(dns2);
		
	return connection;
}

G_MODULE_EXPORT gboolean mmgui_module_connection_update(gpointer mmguicore, mmguiconn_t connection, const gchar *name, const gchar *number, const gchar *username, const gchar *password, const gchar *apn, guint networkid, gboolean homeonly, const gchar *dns1, const gchar *dns2)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GVariant *connpath, *connparams; 
	gchar *path;
	GDBusProxy *connproxy;
	GError *error;
	
	if ((mmguicore == NULL) || (connection == NULL) || (name == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (!(mmguicorelc->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MANAGEMENT)) return FALSE;
	
	if (mmguicorelc->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	/*Get connection object path*/
	error = NULL;
	connpath = g_dbus_proxy_call_sync(moduledata->setproxy,
									"GetConnectionByUuid",
									g_variant_new("(s)", connection->uuid),
									0,
									-1,
									NULL,
									&error);
										
	if ((connpath == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	}
	
	g_variant_get(connpath, "(o)", &path);
	g_variant_unref(connpath);
	
	/*Open connection proxy*/
	connproxy = g_dbus_proxy_new_sync(moduledata->connection,
										G_DBUS_PROXY_FLAGS_NONE,
										NULL,
										"org.freedesktop.NetworkManager",
										path,
										"org.freedesktop.NetworkManager.Settings.Connection",
										NULL,
										&error);
		
	if ((connproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_free(path);
		return FALSE;
	}
	
	g_free(path);
	
	/*Serialize connection data*/
	connparams = mmgui_module_connection_serialize(connection->uuid, name, number, username, password, apn, networkid, connection->type, homeonly, dns1, dns2);
	
	/*Call update method*/
	g_dbus_proxy_call_sync(connproxy,
							"Update",
							connparams,
							0,
							-1,
							NULL,
							&error);
										
	if (error != NULL) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_variant_unref(connparams);
		g_object_unref(connproxy);
		return FALSE;
	}
	
	/*Update data*/
	if (connection->name != NULL) {
		g_free(connection->name);
	}
	connection->name = g_strdup(name);
	if (connection->number != NULL) {
		g_free(connection->number);
	}
	connection->number = g_strdup(number);
	if (connection->username != NULL) {
		g_free(connection->username);
	}
	connection->username = g_strdup(username);
	if (connection->password != NULL) {
		g_free(connection->password);
	}
	connection->password = g_strdup(password);
	if (connection->apn != NULL) {
		g_free(connection->apn);
	}
	connection->apn = g_strdup(apn);
	connection->networkid = networkid;
	connection->homeonly = homeonly;
	if (connection->dns1 != NULL) {
		g_free(connection->dns1);
	}
	connection->dns1 = g_strdup(dns1);
	if (connection->dns2 != NULL) {
		g_free(connection->dns2);
	}
	connection->dns2 = g_strdup(dns2);
		
	/*Free resources*/
	g_object_unref(connproxy);
	
	return FALSE;
}

G_MODULE_EXPORT gboolean mmgui_module_connection_remove(gpointer mmguicore, mmguiconn_t connection)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GVariant *connpath;
	gchar *path;
	GDBusProxy *connproxy;
	GError *error;
	
	if ((mmguicore == NULL) || (connection == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (!(mmguicorelc->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MANAGEMENT)) return FALSE;
	
	if (mmguicorelc->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	/*Get connection object path*/
	error = NULL;
	connpath = g_dbus_proxy_call_sync(moduledata->setproxy,
									"GetConnectionByUuid",
									g_variant_new("(s)", connection->uuid),
									0,
									-1,
									NULL,
									&error);
										
	if ((connpath == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	}
	
	g_variant_get(connpath, "(o)", &path);
	g_variant_unref(connpath);
	
	/*Open connection proxy*/
	connproxy = g_dbus_proxy_new_sync(moduledata->connection,
										G_DBUS_PROXY_FLAGS_NONE,
										NULL,
										"org.freedesktop.NetworkManager",
										path,
										"org.freedesktop.NetworkManager.Settings.Connection",
										NULL,
										&error);
		
	if ((connproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_free(path);
		return FALSE;
	}
	
	g_free(path);
	
	/*Call update method*/
	g_dbus_proxy_call_sync(connproxy,
							"Delete",
							NULL,
							0,
							-1,
							NULL,
							&error);
										
	if (error != NULL) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_object_unref(connproxy);
		return FALSE;
	}
	
	/*Free resources*/
	g_object_unref(connproxy);
	
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
	GError *error;
	GVariant *nmdevices;
	GVariantIter nmdeviterl1, nmdeviterl2;
	GVariant *nmdevnodel1, *nmdevnodel2;
	gsize strlength;
	const gchar *valuestr;
	GDBusProxy *nmdevproxy;
	GVariant *devproperties;
	const gchar *nmdevpath, *nmdevinterface;
	guint nmdevtype, nmdevstate;
	
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (device == NULL) return FALSE;	
	if (device->objectpath == NULL) return FALSE;
	
	error = NULL;
	
	/*First time just set it to NULL*/
	moduledata->devproxy = NULL;
	
	/*Initialize local variables*/
	nmdevtype = MODULE_INT_DEVICE_TYPE_UNKNOWN;
	nmdevpath = NULL;
	
	/*Network Manager interface*/
	if (moduledata->nmproxy != NULL) {
		nmdevices = g_dbus_proxy_call_sync(moduledata->nmproxy, "GetDevices", NULL, 0, -1, NULL, &error);
		if ((nmdevices != NULL) && (error == NULL)) {
			g_variant_iter_init(&nmdeviterl1, nmdevices);
			while ((nmdevnodel1 = g_variant_iter_next_value(&nmdeviterl1)) != NULL) {
				g_variant_iter_init(&nmdeviterl2, nmdevnodel1);
				while ((nmdevnodel2 = g_variant_iter_next_value(&nmdeviterl2)) != NULL) {
					/*Device path*/
					strlength = 256;
					valuestr = g_variant_get_string(nmdevnodel2, &strlength);
					if ((valuestr != NULL) && (valuestr[0] != '\0')) {
						/*Device proxy*/
						error = NULL;
						nmdevproxy = g_dbus_proxy_new_sync(moduledata->connection,
															G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
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
							if ((nmdevpath != NULL) && (nmdevpath[0] != '\0')) {
								/*Is it device we looking for*/
								if ((nmdevtype == MODULE_INT_DEVICE_TYPE_MODEM) && (g_str_equal(device->objectpath, nmdevpath))) {
									/*Get device state*/
									devproperties = g_dbus_proxy_get_cached_property(nmdevproxy, "State");
									nmdevstate = g_variant_get_uint32(devproperties);
									g_variant_unref(devproperties);
									if ((nmdevstate != MODULE_INT_DEVICE_STATE_UNKNOWN) && (nmdevstate != MODULE_INT_DEVICE_STATE_UNMANAGED)) {
										/*If device connected, get interface name*/
										if (nmdevstate == MODULE_INT_DEVICE_STATE_ACTIVATED) {
											devproperties = g_dbus_proxy_get_cached_property(nmdevproxy, "IpInterface");
											strlength = IFNAMSIZ;
											nmdevinterface = g_variant_get_string(devproperties, &strlength);
											if ((nmdevinterface != NULL) && (nmdevinterface[0] != '\0')) {
												memset(mmguicorelc->device->interface, 0, IFNAMSIZ);
												strncpy(mmguicorelc->device->interface, nmdevinterface, IFNAMSIZ-1);
												mmguicorelc->device->connected = TRUE;
											} else {
												memset(mmguicorelc->device->interface, 0, IFNAMSIZ);
												mmguicorelc->device->connected = FALSE;
											}
											g_variant_unref(devproperties);
										} else {
											memset(mmguicorelc->device->interface, 0, IFNAMSIZ);
											mmguicorelc->device->connected = FALSE;
										}
										/*Update connection transition flag*/
										mmguicorelc->device->conntransition = !((nmdevstate == MODULE_INT_DEVICE_STATE_DISCONNECTED) || (nmdevstate == MODULE_INT_DEVICE_STATE_ACTIVATED));
										/*Save device proxy*/
										moduledata->devproxy = nmdevproxy;
										moduledata->statesignal = g_signal_connect(moduledata->devproxy, "g-signal", G_CALLBACK(mmgui_module_signal_handler), mmguicore);
										break;
									} else {
										g_object_unref(nmdevproxy);
									}
								} else {
									g_object_unref(nmdevproxy);
								}
							}
						} else {
							//Failed to create Network Manager device proxy
							/*mmgui_module_device_connection_get_timestamp(mmguicorelc);*/
							g_error_free(error);
						}
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
		
	if (moduledata->devproxy != NULL) {
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
	
	/*Handle device disconnection*/
	if (moduledata->opinitiated) {
		if (mmguicorelc->eventcb != NULL) {
			(mmguicorelc->eventcb)(MMGUI_EVENT_MODEM_CONNECTION_RESULT, mmguicorelc, GUINT_TO_POINTER(TRUE));
		}
		moduledata->opinitiated = FALSE;
		moduledata->opstate = FALSE;
	}
	
	if (moduledata->devproxy != NULL) {
		if (g_signal_handler_is_connected(moduledata->devproxy, moduledata->statesignal)) {
			g_signal_handler_disconnect(moduledata->devproxy, moduledata->statesignal);
		}
		g_object_unref(moduledata->devproxy);
		moduledata->devproxy = NULL;
	}
		
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_status(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (moduledata->devproxy == NULL) return FALSE;
	
	mmgui_module_get_updated_interface_state(mmguicorelc, TRUE);
	
	return TRUE;
}

G_MODULE_EXPORT guint64 mmgui_module_device_connection_timestamp(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	GError *error;
	guint64 curts, realts;
	GKeyFile *tsfile;
	
	/*Get current timestamp*/
	curts = (guint64)time(NULL);
	
	if (mmguicore == NULL) return curts;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->device == NULL) return curts;
	if (!mmguicorelc->device->connected) return curts;
	
	error = NULL;
	
	/*Retrieve timestamp from Network Manager timestamps ini file*/
	tsfile = g_key_file_new();
	
	if (!g_key_file_load_from_file(tsfile, MODULE_INT_NM_TIMESTAMPS_FILE_PATH, G_KEY_FILE_NONE, &error)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_key_file_free(tsfile);
		return curts;
	}
	
	/*File has only one section 'timestamps' with elements 'interface=timestamp'*/
	realts = g_key_file_get_uint64(tsfile, MODULE_INT_NM_TIMESTAMPS_FILE_SECTION, mmguicorelc->device->interface, &error);
	
	if (error != NULL) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_key_file_free(tsfile);
		return curts;
	}
	
	/*Free resources*/
	g_key_file_free(tsfile);
	
	return realts;
}

G_MODULE_EXPORT gchar *mmgui_module_device_connection_get_active_uuid(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GVariant *connpathv, *connuuidv;
	gchar *connpath, *connuuid;
	GDBusProxy *connproxy;
	GError *error;
	
	if (mmguicore == NULL) return NULL;
		
	mmguicorelc = (mmguicore_t)mmguicore;
	moduledata = (moduledata_t)(mmguicorelc->cmoduledata);
	
	if (moduledata->devproxy == NULL) return NULL;
	if (!mmguicorelc->device->connected) return NULL;
	
	connuuid = NULL;
	
	/*First get path to active connection object (if there's no active connection, path equals '/')*/
	connpathv = g_dbus_proxy_get_cached_property(moduledata->devproxy, "ActiveConnection");
	if (connpathv != NULL) {
		connpath = (gchar *)g_variant_get_string(connpathv, NULL);
		if ((connpath != NULL) && (g_strcmp0(connpath, "/") != 0)) {
			/*Create connection proxy to read it's property*/
			error = NULL;
			connproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.freedesktop.NetworkManager",
												connpath,
												"org.freedesktop.NetworkManager.Connection.Active",
												NULL,
												&error);
			
			if ((connproxy != NULL) && (error == NULL)) {
				/*Connection proxy is ready - get 'Uuid' property value*/
				connuuidv = g_dbus_proxy_get_cached_property(connproxy, "Uuid");
				if (connuuidv != NULL) {
					connuuid = (gchar *)g_variant_get_string(connuuidv, NULL);
					if (connuuid != NULL) {
						connuuid = g_strdup(connuuid);
					}
					g_variant_unref(connuuidv);
				}
				g_object_unref(connproxy);
			} else {
				/*No such connection - handle error*/
				mmgui_module_handle_error_message(mmguicorelc, error);
				g_error_free(error);
			}
		}
		g_variant_unref(connpathv);
	}
	
	return connuuid;
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_connect(gpointer mmguicore, mmguiconn_t connection)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GVariant *connpath, *conndesc;
	gchar *path;
	GError *error;
	
	if ((mmguicore == NULL) || (connection == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (moduledata->devproxy == NULL) return FALSE;
	
	/*If device already connected, return TRUE*/
	if (mmguicorelc->device->connected) return TRUE;
	
	/*Get connection object path*/
	error = NULL;
	connpath = g_dbus_proxy_call_sync(moduledata->setproxy,
									"GetConnectionByUuid",
									g_variant_new("(s)", connection->uuid),
									0,
									-1,
									NULL,
									&error);
										
	if ((connpath == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	}
	
	g_variant_get(connpath, "(o)", &path);
	g_variant_unref(connpath);
	
	/*Set flag*/
	moduledata->opinitiated = TRUE;
	moduledata->opstate = TRUE;
	
	/*Call activate connection method*/
	conndesc = g_variant_new("(ooo)", path, g_dbus_proxy_get_object_path(moduledata->devproxy), "/");
	
	g_dbus_proxy_call_sync(moduledata->nmproxy,
								"ActivateConnection",
								conndesc,
								0,
								-1,
								NULL,
								&error);
										
	if (error != NULL) {
		moduledata->opinitiated = FALSE;
		moduledata->opstate = FALSE;
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_variant_unref(conndesc);
		return FALSE;
	}
	
	
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_disconnect(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (moduledata->devproxy == NULL) return FALSE;
	
	/*If device already disconnected, return TRUE*/
	if (!mmguicorelc->device->connected) return TRUE;
	
	/*Set flag*/
	moduledata->opinitiated = TRUE;
	moduledata->opstate = TRUE;
	
	/*Call disconnect method*/
	error = NULL;
	
	g_dbus_proxy_call_sync(moduledata->devproxy,
							"Disconnect",
							NULL,
							0,
							-1,
							NULL,
							&error);
	
	if (error != NULL) {
		moduledata->opinitiated = FALSE;
		moduledata->opstate = FALSE;
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	}
	
	/*Update device state*/
	mmguicorelc->device->connected = FALSE;
	
	return TRUE;	
}
