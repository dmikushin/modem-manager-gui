/*
 *      mm06.c
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
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gmodule.h>
#include <string.h>
#include <stdlib.h>
#include <net/if.h>

#include "../mmguicore.h"
#include "../smsdb.h"
#include "../encoding.h"
#include "../dbus-utils.h"

#define MMGUI_MODULE_SERVICE_NAME  "org.freedesktop.ModemManager"
#define MMGUI_MODULE_SYSTEMD_NAME  "modem-manager.service"
#define MMGUI_MODULE_IDENTIFIER    60
#define MMGUI_MODULE_DESCRIPTION   "Modem Manager <= 0.6.0/Wader"
#define MMGUI_MODULE_COMPATIBILITY "org.freedesktop.NetworkManager;/usr/sbin/pppd;"

#define MMGUI_MODULE_ENABLE_OPERATION_TIMEOUT           20000
#define MMGUI_MODULE_SEND_SMS_OPERATION_TIMEOUT         35000
#define MMGUI_MODULE_SEND_USSD_OPERATION_TIMEOUT        25000
#define MMGUI_MODULE_NETWORKS_SCAN_OPERATION_TIMEOUT    60000
#define MMGUI_MODULE_NETWORKS_UNLOCK_OPERATION_TIMEOUT  20000
#define MMGUI_MODULE_SMS_POLL_INTERVAL                  3

//Location types internal flags
typedef enum {
	MODULE_INT_MODEM_LOCATION_CAPABILITY_UNKNOWN    = 0x0,
	MODULE_INT_MODEM_LOCATION_CAPABILITY_GPS_NMEA   = 0x1,
	MODULE_INT_MODEM_LOCATION_CAPABILITY_GSM_LAC_CI = 0x2
} ModuleIntModemLocationCapability;

/*Registration types internal flags*/
typedef enum {
	MODULE_INT_GSM_NETWORK_REG_STATUS_IDLE          = 0,
	MODULE_INT_GSM_NETWORK_REG_STATUS_HOME          = 1,
	MODULE_INT_GSM_NETWORK_REG_STATUS_SEARCHING     = 2,
	MODULE_INT_GSM_NETWORK_REG_STATUS_DENIED        = 3,
	MODULE_INT_GSM_NETWORK_REG_STATUS_UNKNOWN       = 4,
	MODULE_INT_GSM_NETWORK_REG_STATUS_ROAMING       = 5
} ModuleIntGsmNetworkRegStatus;

typedef enum {
	MODULE_INT_CDMA_REGISTRATION_STATE_UNKNOWN      = 0,
	MODULE_INT_CDMA_REGISTRATION_STATE_REGISTERED   = 1,
	MODULE_INT_CDMA_REGISTRATION_STATE_HOME         = 2,
	MODULE_INT_CDMA_REGISTRATION_STATE_ROAMING      = 3
} ModuleIntCdmaRegistrationState;

/*State internal flags*/
typedef enum {
	MODULE_INT_MODEM_STATE_UNKNOWN                  = 0,
	MODULE_INT_MODEM_STATE_DISABLED                 = 10,
	MODULE_INT_MODEM_STATE_DISABLING                = 20,
	MODULE_INT_MODEM_STATE_ENABLING                 = 30,
	MODULE_INT_MODEM_STATE_ENABLED                  = 40,
	MODULE_INT_MODEM_STATE_SEARCHING                = 50,
	MODULE_INT_MODEM_STATE_REGISTERED               = 60,
	MODULE_INT_MODEM_STATE_DISCONNECTING            = 70,
	MODULE_INT_MODEM_STATE_CONNECTING               = 80,
	MODULE_INT_MODEM_STATE_CONNECTED                = 90
}ModuleIntModemState;

/*Service type*/
typedef enum {
	MODULE_INT_SERVICE_UNDEFINED                    = 0x0,
	MODULE_INT_SERVICE_MODEM_MANAGER                = 0x1,
	MODULE_INT_SERVICE_WADER                        = 0x2
} ModuleIntService;

//Private module variables
struct _mmguimoduledata {
	//DBus connection
	GDBusConnection *connection;
	//DBus proxy objects
	GDBusProxy *managerproxy;
	GDBusProxy *cardproxy;
	GDBusProxy *netproxy;
	GDBusProxy *modemproxy;
	GDBusProxy *smsproxy;
	GDBusProxy *ussdproxy;
	GDBusProxy *locationproxy;
	GDBusProxy *timeproxy;
	GDBusProxy *contactsproxy;
	//Attached signal handlers
	gulong statesignal;
	gulong smssignal;
	//Property change signal
	gulong netsignal;
	gulong netpropsignal;
	gulong locationpropsignal;
	//Service type
	ModuleIntService service;
	//Legacy ModemManager versions
	gboolean needsmspolling;
	time_t polltimestamp;
	//USSD reencoding flag
	gboolean reencodeussd;
	//Last error message
	gchar *errormessage;
	//Cancellable
	GCancellable *cancellable;
	//Operations timeouts
	guint timeouts[MMGUI_DEVICE_OPERATIONS];
};

typedef struct _mmguimoduledata *moduledata_t;


static void mmgui_module_handle_error_message(mmguicore_t mmguicore, GError *error);
static guint mmgui_module_device_id(const gchar *devpath);
static gint mmgui_module_gsm_operator_code(const gchar *opcodestr);
static void mmgui_module_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data);
static void mmgui_module_property_change_handler(GDBusProxy *proxy, GVariant *changed_properties, GStrv invalidated_properties, gpointer data);
static gboolean mmgui_module_device_enabled_from_state(guint state);
static gboolean mmgui_module_device_locked_from_unlock_string(gchar *ustring);
static gint mmgui_module_device_lock_type_from_unlock_string(gchar *ustring);
static gboolean mmgui_module_device_connected_from_state(gint state);
static gboolean mmgui_module_device_registered_from_state(gint state);
static gboolean mmgui_module_device_registered_from_status(guint status);
static gboolean mmgui_module_cdma_device_registered_from_status(guint status);
static enum _mmgui_reg_status mmgui_module_registration_status_translate(guint status);
static enum _mmgui_reg_status mmgui_module_cdma_registration_status_translate(guint status);
static mmguidevice_t mmgui_module_device_new(mmguicore_t mmguicore, const gchar *devpath);
static gboolean mmgui_module_devices_update_device_mode(gpointer mmguicore, gint oldstate, gint newstate, guint changereason);
static gboolean mmgui_module_devices_update_location(gpointer mmguicore, mmguidevice_t device);
static gboolean mmgui_module_devices_enable_location(gpointer mmguicore, mmguidevice_t device, gboolean enable);
static gboolean mmgui_module_devices_restart_ussd(gpointer mmguicore);
static void mmgui_module_devices_enable_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data);
static time_t mmgui_module_str_to_time(const gchar *str);
static mmgui_sms_message_t mmgui_module_sms_retrieve(mmguicore_t mmguicore, GVariant *messagev);
static void mmgui_module_sms_send_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data);
static void mmgui_module_ussd_send_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data);
static mmgui_scanned_network_t mmgui_module_network_retrieve(GVariant *networkv);
static void mmgui_module_networks_scan_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data);
static mmgui_contact_t mmgui_module_contact_retrieve(GVariant *contactv);


static void mmgui_module_handle_error_message(mmguicore_t mmguicore, GError *error)
{
	moduledata_t moduledata;
	
	if ((mmguicore == NULL) || (error == NULL)) return;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	
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

static guint mmgui_module_device_id(const gchar *devpath)
{
	guint id;
	gchar *devidstr;
	
	devidstr = strrchr(devpath, '/') + 1;
	if ((devidstr != NULL) && (devidstr[0] != '\0')) {
		id = atoi(devidstr);
	} else {
		id = 0;
	}
	
	return id;
}

static gint mmgui_module_gsm_operator_code(const gchar *opcodestr)
{
	gsize length;
	gchar codepartbuf[4];
	gint operatorcode;
	gchar *decopcodestr;
	gsize decopcodelen;
	
	if (opcodestr == NULL) return 0;
	
	length = strlen(opcodestr);
	
	operatorcode = 0;
	
	decopcodestr = NULL;
	decopcodelen = 0;
	
	if ((length == 5) || (length == 6)) {
		/*UTF-8 operator code*/
		decopcodestr = g_strdup(opcodestr);
		decopcodelen = length;
	} else if ((length == 20) || (length == 24)) {
		/*UCS-2 operator code*/
		decopcodestr = (gchar *)ucs2_to_utf8((const guchar *)opcodestr, length, &decopcodelen);
		if ((decopcodelen != 5) && (decopcodelen != 6)) {
			if (decopcodestr != NULL) {
				g_free(decopcodestr);
			}
			return operatorcode;
		}
	} else {
		/*Unknown format*/
		return operatorcode;
	}
	
	/*MCC*/
	memset(codepartbuf, 0, sizeof(codepartbuf));
	memcpy(codepartbuf, decopcodestr, 3);
	operatorcode |= (atoi(codepartbuf) & 0x0000ffff) << 16;
	
	/*MNC*/
	memset(codepartbuf, 0, sizeof(codepartbuf));
	memcpy(codepartbuf, decopcodestr + 3, decopcodelen - 3);
	operatorcode |= atoi(codepartbuf) & 0x0000ffff;
	
	g_free(decopcodestr);
	
	return operatorcode;
}

static void mmgui_module_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data)
{
	mmguicore_t mmguicore;
	mmguidevice_t device;
	moduledata_t moduledata;
	gchar *devpath, *operatorcode, *operatorname;
	guint id, oldstate, newstate, changereason, regstatus, regstatus2;
	gboolean status;
		
	mmguicore = (mmguicore_t)data;
	if (mmguicore == NULL) return;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	if (moduledata == NULL) return;
	
	if (g_str_equal(signal_name, "DeviceAdded")) {
		g_variant_get(parameters, "(o)", &devpath);
		if (devpath != NULL) {
			device = mmgui_module_device_new(mmguicore, devpath);
			if (mmguicore->eventcb != NULL) {
				(mmguicore->eventcb)(MMGUI_EVENT_DEVICE_ADDED, mmguicore, device);
			}
		}
	} else if (g_str_equal(signal_name, "DeviceRemoved")) {
		g_variant_get(parameters, "(o)", &devpath);
		if (devpath != NULL) {
			id = mmgui_module_device_id(devpath);
			if (mmguicore->eventcb != NULL) {
				(mmguicore->eventcb)(MMGUI_EVENT_DEVICE_REMOVED, mmguicore, GUINT_TO_POINTER(id));
			}
		}
	} else if (g_str_equal(signal_name, "Completed")) {
		g_variant_get(parameters, "(ub)", &id, &status);
		if ((status) && (!moduledata->needsmspolling)) {
			if (mmguicore->eventcb != NULL) {
				(mmguicore->eventcb)(MMGUI_EVENT_SMS_COMPLETED, mmguicore, GUINT_TO_POINTER(id));
			}
		}
	} else if (g_str_equal(signal_name, "SignalQuality")) {
		g_variant_get(parameters, "(u)", &id);
		if (mmguicore->device != NULL) {
			mmguicore->device->siglevel = id;
			if (mmguicore->eventcb != NULL) {
				(mmguicore->eventcb)(MMGUI_EVENT_SIGNAL_LEVEL_CHANGE, mmguicore, mmguicore->device);
			}
		}
	} else if (g_str_equal(signal_name, "RegistrationInfo")) {
		if (mmguicore->device != NULL) {
			g_variant_get(parameters, "(uss)", &regstatus, &operatorcode, &operatorname);
			if (mmguicore->device->operatorname != NULL) {
				g_free(mmguicore->device->operatorname);
				mmguicore->device->operatorname = NULL;
			}
			mmguicore->device->registered = mmgui_module_device_registered_from_status(regstatus);
			mmguicore->device->regstatus = mmgui_module_registration_status_translate(regstatus);
			mmguicore->device->operatorcode = mmgui_module_gsm_operator_code(operatorcode);
			mmguicore->device->operatorname = g_strdup(operatorname);
			if (mmguicore->eventcb != NULL) {
				(mmguicore->eventcb)(MMGUI_EVENT_NETWORK_REGISTRATION_CHANGE, mmguicore, mmguicore->device);
			}
		}
	} else if (g_str_equal(signal_name, "RegistrationStateChanged")) {
		if (mmguicore->device != NULL) {
			g_variant_get(parameters, "(uu)", &regstatus, &regstatus2);
			mmguicore->device->registered = mmgui_module_cdma_device_registered_from_status(regstatus);
			mmguicore->device->regstatus = mmgui_module_cdma_registration_status_translate(regstatus);
			if (mmguicore->device->regstatus == MMGUI_REG_STATUS_UNKNOWN) {
				mmguicore->device->registered = mmgui_module_cdma_device_registered_from_status(regstatus2);
				mmguicore->device->regstatus = mmgui_module_cdma_registration_status_translate(regstatus2);
			}
			if (mmguicore->eventcb != NULL) {
				(mmguicore->eventcb)(MMGUI_EVENT_NETWORK_REGISTRATION_CHANGE, mmguicore, mmguicore->device);
			}
		}
	} else if (g_str_equal(signal_name, "StateChanged")) {
		if (mmguicore->device != NULL) {
			g_variant_get(parameters, "(uuu)", &oldstate, &newstate, &changereason);
			mmgui_module_devices_update_device_mode(mmguicore, oldstate, newstate, changereason);
		}
	}
	
	g_debug("SIGNAL: %s (%s) argtype: %s\n", signal_name, sender_name, g_variant_get_type_string(parameters));
}

static void mmgui_module_property_change_handler(GDBusProxy *proxy, GVariant *changed_properties, GStrv invalidated_properties, gpointer data)
{
	mmguicore_t mmguicore;
	GVariantIter *iter;
	const gchar *key;
	GVariant *value;
	
	if ((changed_properties == NULL) || (data == NULL)) return;
	
	mmguicore = (mmguicore_t)data;
	
	if (mmguicore->device == NULL) return;
	
	if (g_variant_n_children(changed_properties) > 0) {
		g_variant_get(changed_properties, "a{sv}", &iter);
		while (g_variant_iter_loop(iter, "{&sv}", &key, &value)) {
			if (g_str_equal(key, "Location")) {
				/*Update location*/
				if (mmgui_module_devices_update_location(mmguicore, mmguicore->device)) {
					if (mmguicore->eventcb != NULL) {
						(mmguicore->eventcb)(MMGUI_EVENT_LOCATION_CHANGE, mmguicore, mmguicore->device);
					}
				}
			} else if (g_str_equal(key, "AllowedMode")) {
				/*Update allowed mode*/
				mmguicore->device->allmode = g_variant_get_uint32(value);
				if (mmguicore->eventcb != NULL) {
					(mmguicore->eventcb)(MMGUI_EVENT_NETWORK_MODE_CHANGE, mmguicore, mmguicore->device);
				}
			} else if (g_str_equal(key, "AccessTechnology")) {
				/*Update access technology*/
				mmguicore->device->mode = g_variant_get_uint32(value);
				if (mmguicore->eventcb != NULL) {
					(mmguicore->eventcb)(MMGUI_EVENT_NETWORK_MODE_CHANGE, mmguicore, mmguicore->device);
				}
			}
			g_debug("Property changed: %s\n", key);
		}
		g_variant_iter_free(iter);
	}
}

static gboolean mmgui_module_device_enabled_from_state(guint state)
{
	gboolean enabled;
	
	switch (state) {
		case MODULE_INT_MODEM_STATE_UNKNOWN:
		case MODULE_INT_MODEM_STATE_DISABLED:
		case MODULE_INT_MODEM_STATE_DISABLING:
		case MODULE_INT_MODEM_STATE_ENABLING:
			enabled = FALSE;
			break;
		case MODULE_INT_MODEM_STATE_ENABLED:
		case MODULE_INT_MODEM_STATE_SEARCHING:
		case MODULE_INT_MODEM_STATE_REGISTERED:
		case MODULE_INT_MODEM_STATE_DISCONNECTING: 
		case MODULE_INT_MODEM_STATE_CONNECTING:
		case MODULE_INT_MODEM_STATE_CONNECTED:
			enabled = TRUE;
			break;
		default:
			enabled = FALSE;
			break;
	}
	
	return enabled;
}

static gboolean mmgui_module_device_locked_from_unlock_string(gchar *ustring)
{
	gboolean locked;
	
	if (ustring == NULL) return FALSE;
	
	if (ustring[0] == '\0') {
		locked = FALSE;
	} else {
		locked = TRUE;
	}
	
	return locked;
}

static gint mmgui_module_device_lock_type_from_unlock_string(gchar *ustring)
{
	gint locktype;
	
	locktype = MMGUI_LOCK_TYPE_OTHER;
	
	if (ustring == NULL) return locktype;
	
	if (ustring[0] == '\0') {
		locktype = MMGUI_LOCK_TYPE_NONE;
	} else if (g_str_equal(ustring, "sim-pin")) {
		locktype = MMGUI_LOCK_TYPE_PIN;
	} else if (g_str_equal(ustring, "sim-puk")) {
		locktype = MMGUI_LOCK_TYPE_PUK;
	} else {
		locktype = MMGUI_LOCK_TYPE_OTHER;
	}
	
	return locktype;
}

static gboolean mmgui_module_device_connected_from_state(gint state)
{
	gboolean connected;
	
	switch (state) {
		case MODULE_INT_MODEM_STATE_UNKNOWN:
		case MODULE_INT_MODEM_STATE_DISABLED:
		case MODULE_INT_MODEM_STATE_DISABLING:
		case MODULE_INT_MODEM_STATE_ENABLING:
		case MODULE_INT_MODEM_STATE_ENABLED:
		case MODULE_INT_MODEM_STATE_SEARCHING:
		case MODULE_INT_MODEM_STATE_REGISTERED:
		case MODULE_INT_MODEM_STATE_DISCONNECTING:
			connected = TRUE;
			break; 
		case MODULE_INT_MODEM_STATE_CONNECTING:
			connected = FALSE;
			break;
		case MODULE_INT_MODEM_STATE_CONNECTED:
			connected = TRUE;
			break;
		default:
			connected = FALSE;
			break;
	}
	
	return connected;
}

static gboolean mmgui_module_device_registered_from_state(gint state)
{
	gboolean registered;
	
	switch (state) {
		case MODULE_INT_MODEM_STATE_UNKNOWN:
		case MODULE_INT_MODEM_STATE_DISABLED:
		case MODULE_INT_MODEM_STATE_DISABLING:
		case MODULE_INT_MODEM_STATE_ENABLING:
		case MODULE_INT_MODEM_STATE_ENABLED:
		case MODULE_INT_MODEM_STATE_SEARCHING:
			registered = FALSE;
			break;
		case MODULE_INT_MODEM_STATE_REGISTERED:
		case MODULE_INT_MODEM_STATE_DISCONNECTING: 
		case MODULE_INT_MODEM_STATE_CONNECTING:
		case MODULE_INT_MODEM_STATE_CONNECTED:
			registered = TRUE;
			break;
		default:
			registered = FALSE;
			break;
	}
	
	return registered;
}

static gboolean mmgui_module_device_registered_from_status(guint status)
{
	gboolean registered;
	
	switch (status) {
		case MODULE_INT_GSM_NETWORK_REG_STATUS_IDLE:
			registered = FALSE;
			break;
		case MODULE_INT_GSM_NETWORK_REG_STATUS_HOME:
			registered = TRUE;
			break;
		case MODULE_INT_GSM_NETWORK_REG_STATUS_SEARCHING:
		case MODULE_INT_GSM_NETWORK_REG_STATUS_DENIED:
		case MODULE_INT_GSM_NETWORK_REG_STATUS_UNKNOWN:
			registered = FALSE;
			break;
		case MODULE_INT_GSM_NETWORK_REG_STATUS_ROAMING:
			registered = TRUE;
			break;
		default:
			registered = FALSE;
			break;
	}
	
	return registered;
}

static gboolean mmgui_module_cdma_device_registered_from_status(guint status)
{
	gboolean registered;
	
	switch (status) {
		case MODULE_INT_CDMA_REGISTRATION_STATE_UNKNOWN:
			registered = FALSE;
			break;
		case MODULE_INT_CDMA_REGISTRATION_STATE_REGISTERED:
		case MODULE_INT_CDMA_REGISTRATION_STATE_HOME:
		case MODULE_INT_CDMA_REGISTRATION_STATE_ROAMING:
			registered = TRUE;
			break;
		default:
			registered = FALSE;
			break;
	}
	
	return registered;
}

static enum _mmgui_reg_status mmgui_module_registration_status_translate(guint status)
{
	enum _mmgui_reg_status tstatus;
	
	switch (status) {
		case MODULE_INT_GSM_NETWORK_REG_STATUS_IDLE:
			tstatus = MMGUI_REG_STATUS_IDLE;
			break;
		case MODULE_INT_GSM_NETWORK_REG_STATUS_HOME:
			tstatus = MMGUI_REG_STATUS_HOME;
			break;
		case MODULE_INT_GSM_NETWORK_REG_STATUS_SEARCHING:
			tstatus = MMGUI_REG_STATUS_SEARCHING;
			break;
		case MODULE_INT_GSM_NETWORK_REG_STATUS_DENIED:
			tstatus = MMGUI_REG_STATUS_DENIED;
			break;
		case MODULE_INT_GSM_NETWORK_REG_STATUS_UNKNOWN:
			tstatus = MMGUI_REG_STATUS_UNKNOWN;
			break;
		case MODULE_INT_GSM_NETWORK_REG_STATUS_ROAMING:
			tstatus = MMGUI_REG_STATUS_ROAMING;
			break;
		default:
			tstatus = MMGUI_REG_STATUS_UNKNOWN;
			break;
	}
	
	return tstatus;
}

static enum _mmgui_reg_status mmgui_module_cdma_registration_status_translate(guint status)
{
	enum _mmgui_reg_status tstatus;
	
	switch (status) {
		case MODULE_INT_CDMA_REGISTRATION_STATE_UNKNOWN:
			tstatus = MMGUI_REG_STATUS_UNKNOWN;
			break;
		case MODULE_INT_CDMA_REGISTRATION_STATE_REGISTERED:
			tstatus = MMGUI_REG_STATUS_IDLE;
			break;	
		case MODULE_INT_CDMA_REGISTRATION_STATE_HOME:
			tstatus = MMGUI_REG_STATUS_HOME;
			break;
		case MODULE_INT_CDMA_REGISTRATION_STATE_ROAMING:
			tstatus = MMGUI_REG_STATUS_ROAMING;
			break;
		default:
			tstatus = MMGUI_REG_STATUS_UNKNOWN;
			break;
	}
	
	return tstatus;
}

static mmguidevice_t mmgui_module_device_new(mmguicore_t mmguicore, const gchar *devpath)
{
	mmguidevice_t device;
	moduledata_t moduledata;
	GDBusProxy *deviceproxy;
	GError *error;
	GVariant *deviceinfo;
	gchar *manufacturer, *model, *version, *blockstr;
	gsize strsize;
	
	if ((mmguicore == NULL) || (devpath == NULL)) return NULL;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	
	if (moduledata == NULL) return NULL;
	if (moduledata->connection == NULL) return NULL;
	
	device = g_new0(struct _mmguidevice, 1);
	
	//Save device identifier and object path
	device->id = mmgui_module_device_id(devpath);
	device->objectpath = g_strdup(devpath);
	
	//If service type not defined, guess it using device object path
	if (devpath != NULL) {
		if (moduledata->service == MODULE_INT_SERVICE_UNDEFINED) {
			if (strstr(devpath, "Modems") != NULL) {
				moduledata->service = MODULE_INT_SERVICE_MODEM_MANAGER;
			} else if (strstr(devpath, "Devices") != NULL) {
				moduledata->service = MODULE_INT_SERVICE_WADER;
			}
		}
	}
	
	device->operation = MMGUI_DEVICE_OPERATION_IDLE;
	//Zero values we can't get this moment
	//SMS
	device->smscaps = MMGUI_SMS_CAPS_NONE;
	device->smsdb = NULL;
	//Networks
	//Info
	device->operatorname = NULL;
	device->operatorcode = 0;
	device->imei = NULL;
	device->imsi = NULL;
	//USSD
	device->ussdcaps = MMGUI_USSD_CAPS_NONE;
	device->ussdencoding = MMGUI_USSD_ENCODING_GSM7;
	//Location
	device->locationcaps = MMGUI_LOCATION_CAPS_NONE;
	memset(device->loc3gppdata, 0, sizeof(device->loc3gppdata));
	memset(device->locgpsdata, 0, sizeof(device->locgpsdata));
	//Scan
	device->scancaps = MMGUI_SCAN_CAPS_NONE;
	//Traffic
	device->rxbytes = 0;
	device->txbytes = 0;
	device->sessiontime = 0;
	device->speedchecktime = 0;
	device->smschecktime = 0;
	device->speedindex = 0;
	device->connected = FALSE;
	memset(device->speedvalues, 0, sizeof(device->speedvalues));
	memset(device->interface, 0, sizeof(device->interface));
	//Contacts
	device->contactscaps = MMGUI_CONTACTS_CAPS_NONE;
	device->contactslist = NULL;
		
	error = NULL;
	
	deviceproxy = g_dbus_proxy_new_sync(moduledata->connection,
										G_DBUS_PROXY_FLAGS_NONE,
										NULL,
										"org.freedesktop.ModemManager",
										devpath,
										"org.freedesktop.ModemManager.Modem",
										NULL,
										&error);
	
	if ((deviceproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
		g_object_unref(deviceproxy);
		//Fill default values
		device->manufacturer = g_strdup(_("Unknown"));
		device->model = g_strdup(_("Unknown"));
		device->version = g_strdup(_("Unknown"));
		device->port = g_strdup(_("Unknown"));
		device->type = MMGUI_DEVICE_TYPE_GSM;
		return device;
	}
	
	//Is device enabled
	deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "Enabled");
	if (deviceinfo != NULL) {
		device->enabled = g_variant_get_boolean(deviceinfo);
		g_variant_unref(deviceinfo);
	} else {
		device->enabled = TRUE;
		g_debug("Failed to retrieve device enabled state, assuming enabled\n");
	}
	
	/*If device locked and what type of lock it is*/
	deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "UnlockRequired");
	if (deviceinfo != NULL) {
		strsize = 256;
		blockstr = (gchar *)g_variant_get_string(deviceinfo, &strsize);
		device->blocked = mmgui_module_device_locked_from_unlock_string(blockstr);
		device->locktype = mmgui_module_device_lock_type_from_unlock_string(blockstr);
		g_variant_unref(deviceinfo);
	} else {
		device->blocked = FALSE;
		g_debug("Failed to retrieve device blocked state, assuming not blocked\n");
	}
	
	/*Wader needs to enable modem before working with it*/
	if (moduledata->service == MODULE_INT_SERVICE_WADER) {
		if (!device->enabled) {
			error = NULL;
			g_dbus_proxy_call_sync(deviceproxy,
									"Enable",
									g_variant_new("(b)", TRUE),
									0,
									-1,
									NULL,
									&error);
			
			if (error != NULL) {
				mmgui_module_handle_error_message(mmguicore, error);
				g_error_free(error);
				g_object_unref(deviceproxy);
				//Fill default values
				device->manufacturer = g_strdup(_("Unknown"));
				device->model = g_strdup(_("Unknown"));
				device->version = g_strdup(_("Unknown"));
				device->port = g_strdup(_("Unknown"));
				device->type = MMGUI_DEVICE_TYPE_GSM;
				return device;
			}
		}
	}
		
	error = NULL;
	
	deviceinfo = g_dbus_proxy_call_sync(deviceproxy,
										"GetInfo",
										NULL,
										0,
										-1,
										NULL,
										&error);
	
	if ((deviceinfo == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
		g_object_unref(deviceproxy);
		//Fill default values
		device->manufacturer = g_strdup(_("Unknown"));
		device->model = g_strdup(_("Unknown"));
		device->version = g_strdup(_("Unknown"));
		device->port = g_strdup(_("Unknown"));
		device->type = MMGUI_DEVICE_TYPE_GSM;
		return device;
	}
	
	g_variant_get(deviceinfo, "((sss))", &manufacturer, &model, &version);
	
	if (manufacturer != NULL) {
		device->manufacturer = g_strdup(manufacturer);
	} else {
		device->manufacturer = g_strdup(_("Unknown"));
	}
	
	if (model != NULL) {
		device->model = g_strdup(model);
	} else {
		device->model = g_strdup(_("Unknown"));
	}
	
	if (version != NULL) {
		device->version = g_strdup(version);
	} else {
		device->version = g_strdup(_("Unknown"));
	}
	
	g_variant_unref(deviceinfo);
	
	//Device path
	deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "Device");
	if (deviceinfo != NULL) {
		strsize = 256;
		device->port = g_strdup(g_variant_get_string(deviceinfo, &strsize));
		g_variant_unref(deviceinfo);
	} else {
		device->sysfspath = NULL;
		g_debug("Failed to retrieve device path\n");
	}
	
	//Need to get usb device serial for fallback traffic monitoring
	deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "MasterDevice");
	if (deviceinfo != NULL) {
		strsize = 256;
		device->sysfspath = g_strdup(g_variant_get_string(deviceinfo, &strsize));
		g_variant_unref(deviceinfo);
	} else {
		device->sysfspath = NULL;
		g_debug("Failed to retrieve device serial specification\n");
	}
	
	//Device type
	deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "Type");
	if (deviceinfo != NULL) {
		device->type = g_variant_get_uint32(deviceinfo);
		g_variant_unref(deviceinfo);
	} else {
		device->type = MMGUI_DEVICE_TYPE_GSM;
		g_debug("Failed to retrieve device type, assuming GSM\n");
	}
	
	//Internal Modem Manager identifier (some services, e.g. Wader not using it)
	if (moduledata->service == MODULE_INT_SERVICE_MODEM_MANAGER) {
		deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "DeviceIdentifier");
		if (deviceinfo != NULL) {
			strsize = 256;
			device->internalid = g_strdup(g_variant_get_string(deviceinfo, &strsize));
			g_variant_unref(deviceinfo);
		} else {
			device->internalid = NULL;
			g_debug("Failed to retrieve device internal identifier\n");
		}
	} else {
		device->internalid = NULL;
	}
		
	//Persistent device identifier
	blockstr = g_strdup_printf("%s_%s_%s", device->manufacturer, device->model, device->version);
	device->persistentid = g_compute_checksum_for_string(G_CHECKSUM_MD5, (const gchar *)blockstr, -1);
	g_free(blockstr);
	
	g_object_unref(deviceproxy);
	
	return device;
}

G_MODULE_EXPORT gboolean mmgui_module_init(mmguimodule_t module)
{
	if (module == NULL) return FALSE;
	
	module->type = MMGUI_MODULE_TYPE_MODEM_MANAGER;
	module->requirement = MMGUI_MODULE_REQUIREMENT_SERVICE;
	module->priority = MMGUI_MODULE_PRIORITY_LOW;
	module->identifier = MMGUI_MODULE_IDENTIFIER;
	module->functions = MMGUI_MODULE_FUNCTION_BASIC;
	g_snprintf(module->servicename,   sizeof(module->servicename),   MMGUI_MODULE_SERVICE_NAME);
	g_snprintf(module->systemdname,   sizeof(module->systemdname),   MMGUI_MODULE_SYSTEMD_NAME);
	g_snprintf(module->description,   sizeof(module->description),   MMGUI_MODULE_DESCRIPTION);
	g_snprintf(module->compatibility, sizeof(module->compatibility), MMGUI_MODULE_COMPATIBILITY);
	
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_open(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t *moduledata;
	GError *error;
	
	if (mmguicore == NULL) return FALSE;
	
	mmguicorelc = (mmguicore_t)mmguicore;
	
	moduledata = (moduledata_t *)&mmguicorelc->moduledata;
	
	(*moduledata) = g_new0(struct _mmguimoduledata, 1);
	
	error = NULL;
	
	(*moduledata)->connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	
	(*moduledata)->errormessage = NULL;
	
	if (((*moduledata)->connection == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
		g_free(mmguicorelc->moduledata);
		return FALSE;
	}
	
	error = NULL;
	
	(*moduledata)->managerproxy = g_dbus_proxy_new_sync((*moduledata)->connection,
													G_DBUS_PROXY_FLAGS_NONE,
													NULL,
													"org.freedesktop.ModemManager",
													"/org/freedesktop/ModemManager",
													"org.freedesktop.ModemManager",
													NULL,
													&error);
		
	if (((*moduledata)->managerproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_object_unref((*moduledata)->connection);
		g_free(mmguicorelc->moduledata);
		return FALSE;
	}
	
	g_signal_connect(G_OBJECT((*moduledata)->managerproxy), "g-signal", G_CALLBACK(mmgui_module_signal_handler), mmguicore);
	
	//Set service type to undefined before using any functions
	(*moduledata)->service = MODULE_INT_SERVICE_UNDEFINED;
	/*Cancellable*/
	(*moduledata)->cancellable = g_cancellable_new();
	/*Operations timeouts*/
	(*moduledata)->timeouts[MMGUI_DEVICE_OPERATION_ENABLE] = MMGUI_MODULE_ENABLE_OPERATION_TIMEOUT;
	(*moduledata)->timeouts[MMGUI_DEVICE_OPERATION_SEND_SMS] = MMGUI_MODULE_SEND_SMS_OPERATION_TIMEOUT;
	(*moduledata)->timeouts[MMGUI_DEVICE_OPERATION_SEND_USSD] = MMGUI_MODULE_SEND_USSD_OPERATION_TIMEOUT;
	(*moduledata)->timeouts[MMGUI_DEVICE_OPERATION_SCAN] = MMGUI_MODULE_NETWORKS_SCAN_OPERATION_TIMEOUT;
	(*moduledata)->timeouts[MMGUI_DEVICE_OPERATION_UNLOCK] = MMGUI_MODULE_NETWORKS_UNLOCK_OPERATION_TIMEOUT;
	
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_close(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
		
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	moduledata = (moduledata_t)(mmguicorelc->moduledata);
	
	//Close device
	//Stop subsystems
	
	if (moduledata != NULL) {
		if (moduledata->errormessage != NULL) {
			g_free(moduledata->errormessage);
		}
		
		if (moduledata->cancellable != NULL) {
			g_object_unref(moduledata->cancellable);
			moduledata->cancellable = NULL;
		}
		
		if (moduledata->managerproxy != NULL) {
			g_object_unref(moduledata->managerproxy);
			moduledata->managerproxy = NULL;
		}
		
		if (moduledata->connection != NULL) {
			g_object_unref(moduledata->connection);
			moduledata->connection = NULL;
		}
		
		g_free(moduledata);
	}
	
	return TRUE;
}

G_MODULE_EXPORT gchar *mmgui_module_last_error(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	
	if (mmguicore == NULL) return NULL;
	
	mmguicorelc = (mmguicore_t)mmguicore;
	moduledata = (moduledata_t)(mmguicorelc->moduledata);
	
	return moduledata->errormessage;
}

G_MODULE_EXPORT gboolean mmgui_module_interrupt_operation(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	mmguidevice_t device;
						
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	device = mmguicorelc->device;
	
	if (device->operation == MMGUI_DEVICE_OPERATION_IDLE) return FALSE;
	
	if (moduledata->cancellable != NULL) {
		g_cancellable_cancel(moduledata->cancellable);
		return TRUE;
	} else {
		return FALSE;
	}
}

G_MODULE_EXPORT gboolean mmgui_module_set_timeout(gpointer mmguicore, guint operation, guint timeout)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
							
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (timeout < 1000) timeout *= 1000;
	
	if (operation < MMGUI_DEVICE_OPERATIONS) {
		moduledata->timeouts[operation] = timeout;
		return TRUE;
	} else {
		return FALSE;
	}
}

G_MODULE_EXPORT guint mmgui_module_devices_enum(gpointer mmguicore, GSList **devicelist)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *devices;
	guint devnum;
	GVariantIter diterl1, diterl2;
	GVariant *dnodel1, *dnodel2;
	gsize devpathsize;
	const gchar *devpath;
	
	if ((mmguicore == NULL) || (devicelist == NULL)) return 0;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return 0;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	error = NULL;
		
	devices = g_dbus_proxy_call_sync(moduledata->managerproxy,
									"EnumerateDevices",
									NULL,
									0,
									-1,
									NULL,
									&error);
	
	if ((devices == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return 0;
	}
	
	devnum = 0;
	g_variant_iter_init(&diterl1, devices);
	while ((dnodel1 = g_variant_iter_next_value(&diterl1)) != NULL) {
		g_variant_iter_init(&diterl2, dnodel1);
		while ((dnodel2 = g_variant_iter_next_value(&diterl2)) != NULL) {
			devpathsize = 256;
			devpath = g_variant_get_string(dnodel2, &devpathsize);
			if (devpath != NULL) {
				//Add device to list
				*devicelist = g_slist_prepend(*devicelist, mmgui_module_device_new(mmguicore, devpath));
				devnum++;
				g_variant_unref(dnodel2);
			}
		}
		g_variant_unref(dnodel1);
    }
	
	g_variant_unref(devices);
	
	return devnum;
}

G_MODULE_EXPORT gboolean mmgui_module_devices_state(gpointer mmguicore, enum _mmgui_device_state_request request)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	mmguidevice_t device;
	GVariant *data;
	GError *error;
	gsize strsize = 256;
	gchar *lockstr, *operatorcode, *operatorname;
	guint regstatus, intval1, intval2;
	gboolean res;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	device = mmguicorelc->device;
	
	res = FALSE;
	
	switch (request) {
		case MMGUI_DEVICE_STATE_REQUEST_ENABLED:
			/*Is device enabled*/
			if (moduledata->modemproxy != NULL) {
				data = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "Enabled");
				if (data != NULL) {
					res = g_variant_get_boolean(data);
					if (device->operation != MMGUI_DEVICE_OPERATION_ENABLE) {
						device->enabled = res;
					}
					g_variant_unref(data);
				} else {
					res = FALSE;
				}
			} else {
				res = FALSE;
			}
			break;
		case MMGUI_DEVICE_STATE_REQUEST_LOCKED:
			/*Is device blocked*/
			if (moduledata->modemproxy != NULL) {
				data = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "UnlockRequired");
				if (data != NULL) {
					lockstr = (gchar *)g_variant_get_string(data, &strsize);
					/*If device locked and what type of lock it is*/
					res = mmgui_module_device_locked_from_unlock_string(lockstr);
					device->locktype = mmgui_module_device_lock_type_from_unlock_string(lockstr);
					device->blocked = res;
					g_variant_unref(data);
				} else {
					res = FALSE;
				}
			} else {
				res = FALSE;
			}
			break;
		case MMGUI_DEVICE_STATE_REQUEST_REGISTERED:
			/*Is device registered in network*/
			if (moduledata->netproxy != NULL) {
				if (moduledata->netproxy != NULL) {
					if (device->type == MMGUI_DEVICE_TYPE_GSM) {
						error = NULL;
						data = g_dbus_proxy_call_sync(moduledata->netproxy,
														"GetRegistrationInfo",
														NULL,
														0,
														-1,
														NULL,
														&error);
						if ((data == NULL) && (error != NULL)) {
							mmgui_module_handle_error_message(mmguicorelc, error);
							g_error_free(error);
							res = FALSE;
						} else {
							g_variant_get(data, "((uss))", &regstatus, &operatorcode, &operatorname);
							res = mmgui_module_device_registered_from_status(regstatus);
							device->registered = res;
							g_variant_unref(data);
						}
					} else if (device->type == MMGUI_DEVICE_TYPE_CDMA) {
						error = NULL;
						data = g_dbus_proxy_call_sync(moduledata->netproxy,
														"GetRegistrationState",
														NULL,
														0,
														-1,
														NULL,
														&error);
						if ((data == NULL) && (error != NULL)) {
							mmgui_module_handle_error_message(mmguicorelc, error);
							g_error_free(error);
						} else {
							g_variant_get(data, "((uu))", &intval1, &intval2);
							res = mmgui_module_cdma_device_registered_from_status(intval1);
							device->registered = res;
							if (device->regstatus == MMGUI_REG_STATUS_UNKNOWN) {
								res = mmgui_module_cdma_device_registered_from_status(intval2);
								device->registered = res;
							}
							g_variant_unref(data);
						}
					}
				} else {
					res = FALSE;
				}
			} else {
				res = FALSE;
			}
			break;
		case MMGUI_DEVICE_STATE_REQUEST_CONNECTED:
			/*Is device connected (modem manager state)*/
			if (moduledata->modemproxy != NULL) {
				data = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "State");
				if (data != NULL) {
					res = mmgui_module_device_connected_from_state(g_variant_get_uint32(data));
					g_variant_unref(data);
				} else {
					res = FALSE;
				}
			} else {
				res = FALSE;
			}
			break;
		case MMGUI_DEVICE_STATE_REQUEST_PREPARED:
			/*Not clear what state property means - just return TRUE*/
			res = TRUE;
			break;
		default:
			res = FALSE;
			break;
	}
	
	return res;
}

G_MODULE_EXPORT gboolean mmgui_module_devices_update_state(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	mmguidevice_t device;
	time_t currenttime;
	guint msgnum;
	GError *error;
	GVariant *messages;
	GVariantIter miterl1, miterl2;
	GVariant *mnodel1, *mnodel2;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	device = mmguicorelc->device;
	
	if (moduledata->smsproxy == NULL) return FALSE;
	if (!device->enabled) return FALSE;
	if (!(device->smscaps & MMGUI_SMS_CAPS_RECEIVE)) return FALSE;
	
	if (moduledata->needsmspolling) {
		currenttime = time(NULL);
		if (abs((gint)difftime(moduledata->polltimestamp, currenttime)) >= MMGUI_MODULE_SMS_POLL_INTERVAL) {
			moduledata->polltimestamp = currenttime;
			error = NULL;
			messages = g_dbus_proxy_call_sync(moduledata->smsproxy,
												"List",
												NULL,
												0,
												-1,
												NULL,
												&error);
			
			if ((messages == NULL) && (error != NULL)) {
				g_error_free(error);
				msgnum = 0;
			} else {
				msgnum = 0;
				g_variant_iter_init(&miterl1, messages);
				while ((mnodel1 = g_variant_iter_next_value(&miterl1)) != NULL) {
					g_variant_iter_init(&miterl2, mnodel1);
					while ((mnodel2 = g_variant_iter_next_value(&miterl2)) != NULL) {
						msgnum++;
						g_variant_unref(mnodel2);
					}
					g_variant_unref(mnodel1);
				}
				g_variant_unref(messages);
			}
			
			g_debug("SMS messages number from polling handler: %u\n", msgnum);
			
			if (msgnum > 0) {
				if (mmguicorelc->eventcb != NULL) {
					(mmguicorelc->eventcb)(MMGUI_EVENT_SMS_LIST_READY, mmguicore, GUINT_TO_POINTER(TRUE));
				}
			}
		}
	}
	
	return TRUE;
}

static gboolean mmgui_module_devices_update_device_mode(gpointer mmguicore, gint oldstate, gint newstate, guint changereason)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	mmguidevice_t device;
	gboolean enabledsignal, blockedsignal, regsignal, oldenabled, oldblocked, oldregistered;
	gsize strsize;
	GVariant *data;
	GError *error;
	gchar *blockstr;
	guint intval1, intval2;
	gchar *strval1, *strval2;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	device = mmguicorelc->device;
	
	/*Device enabled status*/
	enabledsignal = FALSE;
	oldenabled = device->enabled;
	device->enabled = mmgui_module_device_enabled_from_state(newstate);
	/*Is enabled signal needed */
	enabledsignal = (oldenabled != device->enabled);
	
	/*Device blocked status and lock type*/
	blockedsignal = FALSE;
	if (moduledata->modemproxy != NULL) {
		oldblocked = device->blocked;
		data = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "UnlockRequired");
		if (data != NULL) {
			blockstr = (gchar *)g_variant_get_string(data, &strsize);
			device->blocked = mmgui_module_device_locked_from_unlock_string(blockstr);
			device->locktype = mmgui_module_device_lock_type_from_unlock_string(blockstr);
			g_variant_unref(data);
			/*Is blocked signal needed */
			blockedsignal = (oldblocked != device->blocked);
		} else {
			device->blocked = FALSE;
		}
	}
	/*Device registered status*/
	oldregistered = device->registered;
	device->registered = mmgui_module_device_registered_from_state(newstate);
	/*Is registered signal needed*/
	regsignal = (oldregistered != device->registered);
	
	/*Return if no signals will be sent*/
	if ((!enabledsignal) && (!blockedsignal) && (!regsignal)) return TRUE;
	
	/*Handle device registration*/
	if ((regsignal) && (device->registered)) {
		if (moduledata->netproxy != NULL) {
			if (device->type == MMGUI_DEVICE_TYPE_GSM) {
				/*Operator information*/
				device->regstatus = MMGUI_REG_STATUS_UNKNOWN;
				device->operatorcode = 0;
				if (device->operatorname != NULL) {
					g_free(device->operatorname);
					device->operatorname = NULL;
				}
				error = NULL;
				data = g_dbus_proxy_call_sync(moduledata->netproxy,
												"GetRegistrationInfo",
												NULL,
												0,
												-1,
												NULL,
												&error);
				if ((data == NULL) && (error != NULL)) {
					mmgui_module_handle_error_message(mmguicorelc, error);
					g_error_free(error);
				} else {
					g_variant_get(data, "((uss))", &intval1, &strval1, &strval2);
					device->regstatus = mmgui_module_registration_status_translate(intval1);
					device->operatorcode = mmgui_module_gsm_operator_code(strval1);
					device->operatorname = g_strdup(strval2);
					g_variant_unref(data);
				}
			} else if (device->type == MMGUI_DEVICE_TYPE_CDMA) {
				/*Operator information*/
				device->regstatus = MMGUI_REG_STATUS_UNKNOWN;
				device->operatorcode = 0;
				/*Registration state*/
				error = NULL;
				data = g_dbus_proxy_call_sync(moduledata->netproxy,
												"GetRegistrationState",
												NULL,
												0,
												-1,
												NULL,
												&error);
				if ((data == NULL) && (error != NULL)) {
					mmgui_module_handle_error_message(mmguicorelc, error);
					g_error_free(error);
				} else {
					g_variant_get(data, "((uu))", &intval1, &intval2);
					device->regstatus = mmgui_module_cdma_registration_status_translate(intval1);
					if (device->regstatus == MMGUI_REG_STATUS_UNKNOWN) {
						device->regstatus = mmgui_module_cdma_registration_status_translate(intval2);
					}
					g_variant_unref(data);
				}
				/*SID*/
				error = NULL;
				data = g_dbus_proxy_call_sync(moduledata->netproxy,
												"GetServingSystem",
												NULL,
												0,
												-1,
												NULL,
												&error);
				if ((data == NULL) && (error != NULL)) {
					mmgui_module_handle_error_message(mmguicorelc, error);
					g_error_free(error);
				} else {
					g_variant_get(data, "((usu))", &intval1, &strval1, &intval2);
					device->operatorcode = intval2;
					g_variant_unref(data);
				}
			}
		}
	}
	
	/*Handle device enablement*/
	if ((enabledsignal) && (device->enabled)) {
		if (device->type == MMGUI_DEVICE_TYPE_GSM) {
			if (moduledata->cardproxy != NULL) {
				/*IMEI*/
				if (device->imei != NULL) {
					g_free(device->imei);
					device->imei = NULL;
				}
				
				error = NULL;
				
				data = g_dbus_proxy_call_sync(moduledata->cardproxy,
												"GetImei",
												NULL,
												0,
												-1,
												NULL,
												&error);
				
				if ((data == NULL) && (error != NULL)) {
					mmgui_module_handle_error_message(mmguicorelc, error);
					g_error_free(error);
				} else {
					g_variant_get(data, "(s)", &device->imei);
					device->imei = g_strdup(device->imei);
					g_variant_unref(data);
				}
				/*IMSI*/
				if (device->imsi != NULL) {
					g_free(device->imsi);
					device->imsi = NULL;
				}
				
				error = NULL;
				
				data = g_dbus_proxy_call_sync(moduledata->cardproxy,
												"GetImsi",
												NULL,
												0,
												-1,
												NULL,
												&error);
				
				if ((data == NULL) && (error != NULL)) {
					mmgui_module_handle_error_message(mmguicorelc, error);
					g_error_free(error);
				} else {
					g_variant_get(data, "(s)", &device->imsi);
					device->imsi = g_strdup(device->imsi);
					g_variant_unref(data);
				}
			}
		} else if (device->type == MMGUI_DEVICE_TYPE_CDMA) {
			if (moduledata->netproxy != NULL) {
				/*ESN*/
				if (device->imei != NULL) {
					g_free(device->imei);
					device->imei = NULL;
				}
				
				error = NULL;
				
				data = g_dbus_proxy_call_sync(moduledata->netproxy,
												"GetEsn",
												NULL,
												0,
												-1,
												NULL,
												&error);
				
				if ((data == NULL) && (error != NULL)) {
					mmgui_module_handle_error_message(mmguicorelc, error);
					g_error_free(error);
				} else {
					g_variant_get(data, "(s)", &device->imsi);
					device->imsi = g_strdup(device->imsi);
					g_variant_unref(data);
				}
				/*No IMSI in CDMA*/
				if (device->imsi != NULL) {
					g_free(device->imsi);
					device->imsi = NULL;
				}
			}
		}
		
		if (moduledata->locationproxy != NULL) {
			/*Enable location interface and update location*/
			if (mmgui_module_devices_enable_location(mmguicorelc, device, TRUE)) {
				if (mmgui_module_devices_update_location(mmguicorelc, device)) {
					if (mmguicorelc->eventcb != NULL) {
						(mmguicorelc->eventcb)(MMGUI_EVENT_LOCATION_CHANGE, mmguicorelc, device);
					}
				}
			}
		}
	}
	
	/*Enabled status signal*/
	if (enabledsignal) {
		if (device->operation != MMGUI_DEVICE_OPERATION_ENABLE) {
			if (mmguicorelc->eventcb != NULL) {
				(mmguicorelc->eventcb)(MMGUI_EVENT_DEVICE_ENABLED_STATUS, mmguicorelc, GUINT_TO_POINTER(device->enabled));
			}
		} else {
			if (mmguicorelc->device != NULL) {
				mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_IDLE;
			}
			if (mmguicorelc->eventcb != NULL) {
				(mmguicorelc->eventcb)(MMGUI_EVENT_MODEM_ENABLE_RESULT, mmguicorelc, GUINT_TO_POINTER(TRUE));
			}
		}
	}
	/*Blocked status signal*/
	if (blockedsignal) {
		if (mmguicorelc->eventcb != NULL) {
			if (device->operation != MMGUI_DEVICE_OPERATION_UNLOCK) {
				(mmguicorelc->eventcb)(MMGUI_EVENT_DEVICE_BLOCKED_STATUS, mmguicorelc, GUINT_TO_POINTER(device->blocked));
			} else {
				if (mmguicorelc->device != NULL) {
					mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_IDLE;
				}
				if (mmguicorelc->eventcb != NULL) {
					(mmguicorelc->eventcb)(MMGUI_EVENT_MODEM_UNLOCK_WITH_PIN_RESULT, mmguicorelc, GUINT_TO_POINTER(TRUE));
				}
			}
		}
	}
	/*Registered status signal*/
	if (regsignal) {
		if (mmguicorelc->eventcb != NULL) {
			(mmguicorelc->eventcb)(MMGUI_EVENT_NETWORK_REGISTRATION_CHANGE, mmguicorelc, device);
		}
	}
		
	return TRUE;
}

static gboolean mmgui_module_devices_update_location(gpointer mmguicore, mmguidevice_t device)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GVariant *data;
	GVariantIter *iter;
	guint32 locationtype;
	GVariant *locationdata;
	gchar *locationstring;
	gsize strlength;
	GError *error;
			
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if ((!(device->locationcaps & MMGUI_LOCATION_CAPS_3GPP)) && (!(device->locationcaps & MMGUI_LOCATION_CAPS_GPS))) return FALSE;
	
	error = NULL;
		
	data = g_dbus_proxy_call_sync(moduledata->locationproxy,
									"GetLocation",
									NULL,
									0,
									-1,
									NULL,
									&error);
	
	if ((data != NULL) && (error == NULL)) {
		g_variant_get(data, "(a{uv})", &iter);
		while (g_variant_iter_next(iter, "{uv}", &locationtype, &locationdata)) {
			if ((locationtype == MODULE_INT_MODEM_LOCATION_CAPABILITY_GSM_LAC_CI) && (locationdata != NULL)) {
				//3GPP location
				strlength = 256;
				locationstring = g_strdup(g_variant_get_string(locationdata, &strlength));
				device->loc3gppdata[0] = (guint)strtol(strsep(&locationstring, ","), NULL, 10);
				device->loc3gppdata[1] = (guint)strtol(strsep(&locationstring, ","), NULL, 10);
				device->loc3gppdata[2] = (guint)strtol(strsep(&locationstring, ","), NULL, 16);
				device->loc3gppdata[3] = (guint)strtol(strsep(&locationstring, ","), NULL, 16);
				g_free(locationstring);
				g_variant_unref(locationdata);
				g_debug("3GPP location: %u, %u, %4x, %4x", device->loc3gppdata[0], device->loc3gppdata[1], device->loc3gppdata[2], device->loc3gppdata[3]);
			}
		}
		g_variant_unref(data);
		
		return TRUE;
	} else {
		if (device->locationcaps & MMGUI_LOCATION_CAPS_3GPP) {
			memset(device->loc3gppdata, 0, sizeof(device->loc3gppdata));
		}
		if (device->locationcaps & MMGUI_LOCATION_CAPS_GPS) {
			memset(device->locgpsdata, 0, sizeof(device->locgpsdata));
		}
		
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		
		return FALSE;
	}
}

static gboolean mmgui_module_devices_enable_location(gpointer mmguicore, mmguidevice_t device, gboolean enable)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GVariant *properties;
	guint locationtypes;
	GError *error;
	
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
		
	if (!device->enabled) return FALSE;
	if (moduledata->locationproxy == NULL) return FALSE;
	
	if ((enable) && ((device->locationcaps & MMGUI_LOCATION_CAPS_3GPP) || (device->locationcaps & MMGUI_LOCATION_CAPS_GPS))) return TRUE;
	if ((!enable) && ((!(device->locationcaps & MMGUI_LOCATION_CAPS_3GPP)) && (!(device->locationcaps & MMGUI_LOCATION_CAPS_GPS)))) return TRUE;
	
	if (enable) {
		//Determine supported capabilities and turn on location engine
		properties = g_dbus_proxy_get_cached_property(moduledata->locationproxy, "Capabilities");
		if (properties != NULL) {
			locationtypes = g_variant_get_uint32(properties);
			if (locationtypes & MODULE_INT_MODEM_LOCATION_CAPABILITY_GSM_LAC_CI) {
				error = NULL;
				//Apply new settings
				g_dbus_proxy_call_sync(moduledata->locationproxy, "Enable", g_variant_new("(bb)", TRUE, TRUE), 0, -1, NULL, &error);
				//Set enabled properties
				if (error == NULL) {
					//3gpp location
					if (locationtypes & MODULE_INT_MODEM_LOCATION_CAPABILITY_GSM_LAC_CI) {
						device->locationcaps |= MMGUI_LOCATION_CAPS_3GPP;
					}
					
					return TRUE;
				} else {
					mmgui_module_handle_error_message(mmguicorelc, error);
					g_error_free(error);
				}
			}
			g_variant_unref(properties);
		}
	} else {
		error = NULL;
		g_dbus_proxy_call_sync(moduledata->locationproxy, "Enable", g_variant_new("(bb)", FALSE, FALSE), 0, -1, NULL, &error);
		if (error == NULL) {
			return TRUE;
		} else {
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
		}
	}
	
	return FALSE;
}

G_MODULE_EXPORT gboolean mmgui_module_devices_information(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	mmguidevice_t device;
	GVariant *data;
	GError *error;
	gchar *blockstr;
	gsize strsize = 256;
	guint intval1, intval2;
	gchar *strval1, *strval2;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	device = mmguicorelc->device;
	
	if (moduledata->modemproxy != NULL) {
		//Is device enabled
		data = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "Enabled");
		if (data != NULL) {
			device->enabled = g_variant_get_boolean(data);
			g_variant_unref(data);
		} else {
			device->enabled = TRUE;
			g_debug("Failed to get device enabled state\n");
		}
		/*Is device locked and what type of lock it is*/
		data = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "UnlockRequired");
		if (data != NULL) {
			blockstr = (gchar *)g_variant_get_string(data, &strsize);
			device->blocked = mmgui_module_device_locked_from_unlock_string(blockstr);
			device->locktype = mmgui_module_device_lock_type_from_unlock_string(blockstr);
			g_variant_unref(data);
		} else {
			device->blocked = FALSE;
			g_debug("Failed to get device blocked state\n");
		}
	}
		
	if (moduledata->netproxy != NULL) {
		if (device->type == MMGUI_DEVICE_TYPE_GSM) {
			if (device->enabled) {
				/*Signal level*/
				device->siglevel = 0;
				error = NULL;
				data = g_dbus_proxy_call_sync(moduledata->netproxy,
												"GetSignalQuality",
												NULL,
												0,
												-1,
												NULL,
												&error);
				if ((data == NULL) && (error != NULL)) {
					mmgui_module_handle_error_message(mmguicorelc, error);
					g_error_free(error);
				} else {
					g_variant_get(data, "(u)", &device->siglevel);
					g_variant_unref(data);
				}
			}
			/*Operator information*/
			device->registered = FALSE;
			device->regstatus = MMGUI_REG_STATUS_UNKNOWN;
			device->operatorcode = 0;
			if (device->operatorname != NULL) {
				g_free(device->operatorname);
				device->operatorname = NULL;
			}
			error = NULL;
			data = g_dbus_proxy_call_sync(moduledata->netproxy,
											"GetRegistrationInfo",
											NULL,
											0,
											-1,
											NULL,
											&error);
			if ((data == NULL) && (error != NULL)) {
				mmgui_module_handle_error_message(mmguicorelc, error);
				g_error_free(error);
			} else {
				g_variant_get(data, "((uss))", &intval1, &strval1, &strval2);
				device->registered = mmgui_module_device_registered_from_status(intval1);
				device->regstatus = mmgui_module_registration_status_translate(intval1);
				device->operatorcode = mmgui_module_gsm_operator_code(strval1);
				device->operatorname = g_strdup(strval2);
				g_variant_unref(data);
			}
			/*Allowed mode*/
			data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "AllowedMode");
			if (data != NULL) {
				device->allmode = g_variant_get_uint32(data);
				g_variant_unref(data);
			} else {
				device->allmode = 0;
				g_debug("Failed to get device allowed mode\n");
			}
			/*Access technology*/
			data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "AccessTechnology");
			if (data != NULL) {
				device->mode = g_variant_get_uint32(data);
				g_variant_unref(data);
			} else {
				device->mode = 0;
				g_debug("Failed to get device access mode\n");
			}
		} else if (device->type == MMGUI_DEVICE_TYPE_CDMA) {
			/*Operator information*/
			device->registered = FALSE;
			device->regstatus = MMGUI_REG_STATUS_UNKNOWN;
			device->operatorcode = 0;
			/*Registration state*/
			error = NULL;
			data = g_dbus_proxy_call_sync(moduledata->netproxy,
											"GetRegistrationState",
											NULL,
											0,
											-1,
											NULL,
											&error);
			if ((data == NULL) && (error != NULL)) {
				mmgui_module_handle_error_message(mmguicorelc, error);
				g_error_free(error);
			} else {
				g_variant_get(data, "((uu))", &intval1, &intval2);
				device->registered = mmgui_module_cdma_device_registered_from_status(intval1);
				device->regstatus = mmgui_module_cdma_registration_status_translate(intval1);
				if (device->regstatus == MMGUI_REG_STATUS_UNKNOWN) {
					device->registered = mmgui_module_cdma_device_registered_from_status(intval2);
					device->regstatus = mmgui_module_cdma_registration_status_translate(intval2);
				}
				g_variant_unref(data);
			}
			/*SID*/
			error = NULL;
			data = g_dbus_proxy_call_sync(moduledata->netproxy,
											"GetServingSystem",
											NULL,
											0,
											-1,
											NULL,
											&error);
			if ((data == NULL) && (error != NULL)) {
				mmgui_module_handle_error_message(mmguicorelc, error);
				g_error_free(error);
			} else {
				g_variant_get(data, "((usu))", &intval1, &strval1, &intval2);
				device->operatorcode = intval2;
				g_variant_unref(data);
			}
			/*Identification*/
			if (device->enabled) {
				/*ESN*/
				if (device->imei != NULL) {
					g_free(device->imei);
					device->imei = NULL;
				}
				error = NULL;
				data = g_dbus_proxy_call_sync(moduledata->netproxy,
												"GetEsn",
												NULL,
												0,
												-1,
												NULL,
												&error);
				
				if ((data == NULL) && (error != NULL)) {
					mmgui_module_handle_error_message(mmguicorelc, error);
					g_error_free(error);
				} else {
					g_variant_get(data, "(s)", &device->imsi);
					device->imsi = g_strdup(device->imsi);
					g_variant_unref(data);
				}
			}
			/*No IMSI in CDMA*/
			if (device->imsi != NULL) {
				g_free(device->imsi);
				device->imsi = NULL;
			}
		}
	}
	
	if (moduledata->cardproxy != NULL) {
		if (device->type == MMGUI_DEVICE_TYPE_GSM) {
			if (device->enabled) {
				//IMEI
				if (device->imei != NULL) {
					g_free(device->imei);
					device->imei = NULL;
				}
				
				error = NULL;
				
				data = g_dbus_proxy_call_sync(moduledata->cardproxy,
												"GetImei",
												NULL,
												0,
												-1,
												NULL,
												&error);
				
				if ((data == NULL) && (error != NULL)) {
					mmgui_module_handle_error_message(mmguicorelc, error);
					g_error_free(error);
				} else {
					g_variant_get(data, "(s)", &device->imei);
					device->imei = g_strdup(device->imei);
					g_variant_unref(data);
				}
			}
			
			if (device->enabled) {
				//IMSI
				if (device->imsi != NULL) {
					g_free(device->imsi);
					device->imsi = NULL;
				}
				
				error = NULL;
				
				data = g_dbus_proxy_call_sync(moduledata->cardproxy,
												"GetImsi",
												NULL,
												0,
												-1,
												NULL,
												&error);
				
				if ((data == NULL) && (error != NULL)) {
					mmgui_module_handle_error_message(mmguicorelc, error);
					g_error_free(error);
				} else {
					g_variant_get(data, "(s)", &device->imsi);
					device->imsi = g_strdup(device->imsi);
					g_variant_unref(data);
				}
			}
		}
	}
	
	
	//Update location
	if (moduledata->service == MODULE_INT_SERVICE_MODEM_MANAGER) {
		mmgui_module_devices_update_location(mmguicore, device);
	}
	
	//Network time. This code makes ModemManager crash, so it commented out
	/*gchar *timev;
	
	if (moduledata->timeproxy != NULL) {
		error = NULL;
		
		data = g_dbus_proxy_call_sync(moduledata->timeproxy,
										"GetNetworkTime",
										NULL,
										0,
										-1,
										NULL,
										&error);
		
		if ((data == NULL) && (error != NULL)) {
			mmgui_module_print_error_message(error);
			g_error_free(error);
		} else {
			g_variant_get(data, "(s)", &timev);
			//device->imsi = g_strdup(device->imsi);
			g_variant_unref(data);
		}
	}*/
	
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_devices_open(gpointer mmguicore, mmguidevice_t device)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GHashTable *interfaces;
	
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	//ModemManager uses 'Modems' prefix and Wader uses 'Devices' prefix
	
	//SIM card interface
	error = NULL;
	moduledata->cardproxy = g_dbus_proxy_new_sync(moduledata->connection,
													G_DBUS_PROXY_FLAGS_NONE,
													NULL,
													"org.freedesktop.ModemManager",
													device->objectpath,
													"org.freedesktop.ModemManager.Modem.Gsm.Card",
													NULL,
													&error);
	if ((moduledata->cardproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
	}
	//Mobile network interface
	if (device->type == MMGUI_DEVICE_TYPE_GSM) {
		error = NULL;
		moduledata->netproxy = g_dbus_proxy_new_sync(moduledata->connection,
													G_DBUS_PROXY_FLAGS_NONE,
													NULL,
													"org.freedesktop.ModemManager",
													device->objectpath,
													"org.freedesktop.ModemManager.Modem.Gsm.Network",
													NULL,
													&error);
		if ((moduledata->netproxy == NULL) && (error != NULL)) {
			device->scancaps = MMGUI_SCAN_CAPS_NONE;
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
		} else {
			device->scancaps = MMGUI_SCAN_CAPS_OBSERVE;
			moduledata->netsignal = g_signal_connect(moduledata->netproxy, "g-signal", G_CALLBACK(mmgui_module_signal_handler), mmguicore);
			moduledata->netpropsignal = g_signal_connect(moduledata->netproxy, "g-properties-changed", G_CALLBACK(mmgui_module_property_change_handler), mmguicore);
		}
	} else if (device->type == MMGUI_DEVICE_TYPE_CDMA) {
		error = NULL;
		moduledata->netproxy = g_dbus_proxy_new_sync(moduledata->connection,
														G_DBUS_PROXY_FLAGS_NONE,
														NULL,
														"org.freedesktop.ModemManager",
														device->objectpath,
														"org.freedesktop.ModemManager.Modem.Cdma",
														NULL,
														&error);
		if ((moduledata->netproxy == NULL) && (error != NULL)) {
			device->scancaps = MMGUI_SCAN_CAPS_NONE;
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
		} else {
			device->scancaps = MMGUI_SCAN_CAPS_NONE;
			moduledata->netsignal = g_signal_connect(moduledata->netproxy, "g-signal", G_CALLBACK(mmgui_module_signal_handler), mmguicore);
		}
	}
	//Modem interface
	error = NULL;
	moduledata->modemproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.freedesktop.ModemManager",
												device->objectpath,
												"org.freedesktop.ModemManager.Modem",
												NULL,
												&error);
	
	if ((moduledata->modemproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
	} else {
		moduledata->statesignal = g_signal_connect(moduledata->modemproxy, "g-signal", G_CALLBACK(mmgui_module_signal_handler), mmguicore);
	}
	//SMS interface
	error = NULL;
	moduledata->smsproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.freedesktop.ModemManager",
												device->objectpath,
												"org.freedesktop.ModemManager.Modem.Gsm.SMS",
												NULL,
												&error);
	
	if ((moduledata->smsproxy == NULL) && (error != NULL)) {
		device->smscaps = MMGUI_SMS_CAPS_NONE;
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
	} else {
		device->smscaps = MMGUI_SMS_CAPS_RECEIVE | MMGUI_SMS_CAPS_SEND;
		moduledata->smssignal = g_signal_connect(moduledata->smsproxy, "g-signal", G_CALLBACK(mmgui_module_signal_handler), mmguicore);
	}
	
	//Assume fully-fuctional modem manager
	moduledata->needsmspolling = FALSE;
	
	if (moduledata->service == MODULE_INT_SERVICE_MODEM_MANAGER) {
		if (device->type == MMGUI_DEVICE_TYPE_GSM) {
			//USSD interface
			error = NULL;
			moduledata->ussdproxy = g_dbus_proxy_new_sync(moduledata->connection,
														G_DBUS_PROXY_FLAGS_NONE,
														NULL,
														"org.freedesktop.ModemManager",
														device->objectpath,
														"org.freedesktop.ModemManager.Modem.Gsm.Ussd",
														NULL,
														&error);
			
			if ((moduledata->ussdproxy == NULL) && (error != NULL)) {
				device->ussdcaps = MMGUI_USSD_CAPS_NONE;
				mmgui_module_handle_error_message(mmguicorelc, error);
				g_error_free(error);
			} else {
				device->ussdcaps = MMGUI_USSD_CAPS_SEND;
			}
		} else if (device->type == MMGUI_DEVICE_TYPE_CDMA) {
			/*No USSD in CDMA*/
			moduledata->ussdproxy = NULL;
			device->ussdcaps = MMGUI_USSD_CAPS_NONE;
		}
		//Location interface (capabilities will be defined later)
		error = NULL;
		moduledata->locationproxy = g_dbus_proxy_new_sync(moduledata->connection,
														G_DBUS_PROXY_FLAGS_NONE,
														NULL,
														"org.freedesktop.ModemManager",
														device->objectpath,
														"org.freedesktop.ModemManager.Modem.Location",
														NULL,
														&error);
		
		if ((moduledata->locationproxy == NULL) && (error != NULL)) {
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
		} else {
			moduledata->locationpropsignal = g_signal_connect(moduledata->locationproxy, "g-properties-changed", G_CALLBACK(mmgui_module_property_change_handler), mmguicore);
			mmgui_module_devices_enable_location(mmguicore, device, TRUE);
		}
		
		/*Supplimentary interfaces*/
		interfaces = mmgui_dbus_utils_list_service_interfaces(moduledata->connection, "org.freedesktop.ModemManager", device->objectpath);
		
		if ((interfaces != NULL) && (g_hash_table_contains(interfaces, "org.freedesktop.ModemManager.Modem.Time"))) {
			//Time interface
			error = NULL;
			moduledata->timeproxy = g_dbus_proxy_new_sync(moduledata->connection,
														G_DBUS_PROXY_FLAGS_NONE,
														NULL,
														"org.freedesktop.ModemManager",
														device->objectpath,
														"org.freedesktop.ModemManager.Modem.Time",
														NULL,
														&error);
			
			if ((moduledata->timeproxy == NULL) && (error != NULL)) {
				moduledata->needsmspolling = TRUE;
				moduledata->polltimestamp = time(NULL);
				device->smscaps &= ~MMGUI_SMS_CAPS_SEND;
				g_error_free(error);
			} else {
				g_debug("SMS messages polling disabled\n");
				moduledata->needsmspolling = FALSE;
			}
		} else {
			g_debug("SMS messages polling enabled\n");
			moduledata->timeproxy = NULL;
			moduledata->needsmspolling = TRUE;
			moduledata->polltimestamp = time(NULL);
			device->smscaps &= ~MMGUI_SMS_CAPS_SEND;
		}
		
		if (interfaces != NULL) {
			g_hash_table_destroy(interfaces);
		}		
		
		//No contacts API
		device->contactscaps = MMGUI_CONTACTS_CAPS_NONE;
	} else if (moduledata->service == MODULE_INT_SERVICE_WADER) {
		//Contacts manipulation interface supported only by Wader
		error = NULL;
		moduledata->contactsproxy = g_dbus_proxy_new_sync(moduledata->connection,
														G_DBUS_PROXY_FLAGS_NONE,
														NULL,
														"org.freedesktop.ModemManager",
														device->objectpath,
														"org.freedesktop.ModemManager.Modem.Gsm.Contacts",
														NULL,
														&error);
		
		if ((moduledata->contactsproxy == NULL) && (error != NULL)) {
			device->contactscaps = MMGUI_CONTACTS_CAPS_NONE;
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
		} else {
			device->contactscaps = MMGUI_CONTACTS_CAPS_EXPORT | MMGUI_CONTACTS_CAPS_EDIT;
		}
		//USSD interface is broken
		device->ussdcaps = MMGUI_USSD_CAPS_NONE;
		//No location API
		device->locationcaps = MMGUI_LOCATION_CAPS_NONE;
	}
		
	//Update device information using created proxy objects
	mmgui_module_devices_information(mmguicore);
	
	//Initialize SMS database
	
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_devices_close(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
			
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	//Close SMS database
	//Free resources
	//Change device pointer
	
	if (moduledata->cardproxy != NULL) {
		g_object_unref(moduledata->cardproxy);
		moduledata->cardproxy = NULL;
	}
	if (moduledata->netproxy != NULL) {
		if (g_signal_handler_is_connected(moduledata->netproxy, moduledata->netsignal)) {
			g_signal_handler_disconnect(moduledata->netproxy, moduledata->netsignal);
		}
		if (g_signal_handler_is_connected(moduledata->netproxy, moduledata->netpropsignal)) {
			g_signal_handler_disconnect(moduledata->netproxy, moduledata->netpropsignal);
		}
		g_object_unref(moduledata->netproxy);
		moduledata->netproxy = NULL;
	}
	if (moduledata->modemproxy != NULL) {
		if (g_signal_handler_is_connected(moduledata->modemproxy, moduledata->statesignal)) {
			g_signal_handler_disconnect(moduledata->modemproxy, moduledata->statesignal);
		}
		g_object_unref(moduledata->modemproxy);
		moduledata->modemproxy = NULL;
	}
	if (moduledata->smsproxy != NULL) {
		if (g_signal_handler_is_connected(moduledata->smsproxy, moduledata->smssignal)) {
			g_signal_handler_disconnect(moduledata->smsproxy, moduledata->smssignal);
		}
		g_object_unref(moduledata->smsproxy);
		moduledata->smsproxy = NULL;
	}
	
	if (moduledata->service == MODULE_INT_SERVICE_MODEM_MANAGER) {
		if (moduledata->ussdproxy != NULL) {
			g_object_unref(moduledata->ussdproxy);
			moduledata->ussdproxy = NULL;
		}
	}
	
	if (moduledata->service == MODULE_INT_SERVICE_MODEM_MANAGER) {
		if (moduledata->locationproxy != NULL) {
			if (g_signal_handler_is_connected(moduledata->locationproxy, moduledata->locationpropsignal)) {
				g_signal_handler_disconnect(moduledata->locationproxy, moduledata->locationpropsignal);
			}
			g_object_unref(moduledata->locationproxy);
			moduledata->locationproxy = NULL;
		}
		if (moduledata->timeproxy != NULL) {
			g_object_unref(moduledata->timeproxy);
			moduledata->timeproxy = NULL;
		}
	} else if (moduledata->service == MODULE_INT_SERVICE_WADER) {
		if (moduledata->contactsproxy != NULL) {
			g_object_unref(moduledata->contactsproxy);
			moduledata->contactsproxy = NULL;
		}
	}
			
	return TRUE;
}

static gboolean mmgui_module_devices_restart_ussd(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	mmguidevice_t device;
	GError *error;
					
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	device = mmguicorelc->device;
	
	if (moduledata->ussdproxy != NULL) {
		device->ussdcaps = MMGUI_USSD_CAPS_NONE;
		g_object_unref(moduledata->ussdproxy);
	}
	
	if (moduledata->service == MODULE_INT_SERVICE_MODEM_MANAGER) {
		error = NULL;
		moduledata->ussdproxy = g_dbus_proxy_new_sync(moduledata->connection,
													G_DBUS_PROXY_FLAGS_NONE,
													NULL,
													"org.freedesktop.ModemManager",
													device->objectpath,
													"org.freedesktop.ModemManager.Modem.Gsm.Ussd",
													NULL,
													&error);
		
		if ((moduledata->ussdproxy == NULL) && (error != NULL)) {
			device->ussdcaps = MMGUI_USSD_CAPS_NONE;
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
			return FALSE;
		} else {
			device->ussdcaps = MMGUI_USSD_CAPS_SEND;
			return TRUE;
		}
	}	
	
	return FALSE;
}

static void mmgui_module_devices_enable_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *result;
		
	mmguicorelc = (mmguicore_t)user_data;
	if (mmguicorelc == NULL) return;
	
	if (mmguicorelc->moduledata == NULL) return;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	error = NULL;
	
	result = g_dbus_proxy_call_finish(proxy, res, &error);
	
	if ((result == NULL) && (error != NULL)) {
		if ((moduledata->cancellable == NULL) || ((moduledata->cancellable != NULL) && (!g_cancellable_is_cancelled(moduledata->cancellable)))) {
			mmgui_module_handle_error_message(mmguicorelc, error);
		}
		
		g_error_free(error);
		
		if (mmguicorelc->device != NULL) {
			mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_IDLE;
		}
		
		if (mmguicorelc->eventcb != NULL) {
			(mmguicorelc->eventcb)(MMGUI_EVENT_MODEM_ENABLE_RESULT, user_data, GUINT_TO_POINTER(FALSE));
		}
	} else {
		/*Handle event if state transition was successful*/
		g_variant_unref(result);
	}
}

G_MODULE_EXPORT gboolean mmgui_module_devices_enable(gpointer mmguicore, gboolean enabled)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
			
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->modemproxy == NULL) return FALSE;
	if (mmguicorelc->device == NULL) return FALSE;
	
	//Device already in requested state
	if (mmguicorelc->device->enabled == enabled) return TRUE;
	
	mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_ENABLE;
	
	if (moduledata->cancellable != NULL) {
		g_cancellable_reset(moduledata->cancellable);
	}
	
	g_dbus_proxy_call(moduledata->modemproxy,
						"Enable",
						g_variant_new("(b)", enabled),
						G_DBUS_CALL_FLAGS_NONE,
						moduledata->timeouts[MMGUI_DEVICE_OPERATION_ENABLE],
						moduledata->cancellable,
						(GAsyncReadyCallback)mmgui_module_devices_enable_handler,
						mmguicore);
	
	return TRUE;
}

static void mmgui_module_devices_unlock_with_pin_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *data;
		
	mmguicorelc = (mmguicore_t)user_data;
	if (mmguicorelc == NULL) return;
	
	if (mmguicorelc->moduledata == NULL) return;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	error = NULL;
	
	data = g_dbus_proxy_call_finish(proxy, res, &error);
	
	if ((data == NULL) && (error != NULL)) {
		/*Handle error*/
		if ((moduledata->cancellable == NULL) || ((moduledata->cancellable != NULL) && (!g_cancellable_is_cancelled(moduledata->cancellable)))) {
			mmgui_module_handle_error_message(mmguicorelc, error);
		}
		
		g_error_free(error);
		
		if (mmguicorelc->device != NULL) {
			mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_IDLE;
		}
		
		if (mmguicorelc->eventcb != NULL) {
			(mmguicorelc->eventcb)(MMGUI_EVENT_MODEM_UNLOCK_WITH_PIN_RESULT, user_data, GUINT_TO_POINTER(FALSE));
		}
	} else {
		/*Handle event if state transition was successful*/
		g_variant_unref(data);
	}
}

G_MODULE_EXPORT gboolean mmgui_module_devices_unlock_with_pin(gpointer mmguicore, gchar *pin)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (moduledata->cardproxy == NULL) return FALSE;
	if (mmguicorelc->device->locktype != MMGUI_LOCK_TYPE_PIN) return FALSE;
	
	mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_UNLOCK;
	
	if (moduledata->cancellable != NULL) {
		g_cancellable_reset(moduledata->cancellable);
	}
	
	g_dbus_proxy_call(moduledata->cardproxy,
						"SendPin",
						g_variant_new("(s)", pin),
						G_DBUS_CALL_FLAGS_NONE,
						moduledata->timeouts[MMGUI_DEVICE_OPERATION_UNLOCK],
						moduledata->cancellable,
						(GAsyncReadyCallback)mmgui_module_devices_unlock_with_pin_handler,
						mmguicore);
	
	return TRUE;
}

static time_t mmgui_module_str_to_time(const gchar *str)
{
	guint i, len;
	gchar strbuf[3];
	struct tm btime;
	time_t timestamp;
	gint *fields[] = {&btime.tm_year, &btime.tm_mon, &btime.tm_mday,
						&btime.tm_hour,	&btime.tm_min, &btime.tm_sec};
	
	timestamp = time(NULL);
	
	if (str == NULL) return timestamp; 
	
	len = strlen(str);
	
	if (len > 12) {
		if (str[12] == '+') {
			//v.0.4.998 timestamp format
			for (i=0; i<6; i++) {
				strncpy(strbuf, str+(i*2), 2);
				strbuf[2] = '\0';
				*fields[i] = atoi(strbuf);
			}
		} else if (str[8] == ',') {
			//v.0.5.2 timestamp format
			for (i=0; i<6; i++) {
				strncpy(strbuf, str+(i*3), 2);
				strbuf[2] = '\0';
				*fields[i] = atoi(strbuf);
			}
		}
		btime.tm_year += 100;
		btime.tm_mon -= 1;
		timestamp = mktime(&btime);
	}
		
	return timestamp;
}

static mmgui_sms_message_t mmgui_module_sms_retrieve(mmguicore_t mmguicore, GVariant *messagev)
{
	moduledata_t moduledata;
	mmgui_sms_message_t message;
	GVariant *value;
	gsize strlength, declength;
	const gchar *valuestr;
	gchar *decstr;
	gboolean gottext, gotindex;
	guint index;
	
	if ((mmguicore == NULL) || (messagev == NULL)) return NULL;
	
	if (mmguicore->moduledata == NULL) return NULL;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	
	index = 0;
	
	message = mmgui_smsdb_message_create();
	
	/*Sender number*/
	value  = g_variant_lookup_value(messagev, "number", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		if ((valuestr != NULL) && (valuestr[0] != '\0')) {
			if (moduledata->needsmspolling) {
				/*Old ModemManager versions tend to not bcd-decode sender numbers, doing it*/
				decstr = (gchar *)bcd_to_utf8_ascii_part((const guchar *)valuestr, strlength, &declength);
				if (decstr != NULL) {
					mmgui_smsdb_message_set_number(message, decstr);
					g_debug("SMS number: %s\n", decstr);
					g_free(decstr);
				} else {
					mmgui_smsdb_message_set_number(message, valuestr);
					g_debug("SMS number: %s\n", valuestr);
				}
			} else {
				mmgui_smsdb_message_set_number(message, valuestr);
				g_debug("SMS number: %s\n", valuestr);
			}
		} else {
			mmgui_smsdb_message_set_number(message, _("Unknown"));
		}
		g_variant_unref(value);
	} else {
		mmgui_smsdb_message_set_number(message, _("Unknown"));
	}
	
	/*Service center number*/
	value = g_variant_lookup_value(messagev, "smsc", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		if ((valuestr != NULL) && (valuestr[0] != '\0')) {
			mmgui_smsdb_message_set_service_number(message, valuestr);
			g_debug("SMS service center number: %s\n", valuestr);
		} else {
			mmgui_smsdb_message_set_service_number(message, _("Unknown"));
		}
		g_variant_unref(value);
	} else {
		mmgui_smsdb_message_set_service_number(message, _("Unknown"));
	}
	
	/*Try to get decoded message text first*/
	gottext = FALSE;
	value = g_variant_lookup_value(messagev, "text", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256*160;
		valuestr = g_variant_get_string(value, &strlength);
		if ((valuestr != NULL) && (valuestr[0] != '\0')) {
			mmgui_smsdb_message_set_text(message, valuestr, FALSE);
			gottext = TRUE;
			g_debug("SMS text: %s\n", valuestr);
		}
		g_variant_unref(value);
	}
	
	/*If there is no text (message isn't decoded), try to get binary data*/
	if (!gottext) {
		value = g_variant_lookup_value(messagev, "data", G_VARIANT_TYPE_BYTESTRING);
		if (value != NULL) {
			strlength = g_variant_get_size(value);
			if (strlength > 0) {
				valuestr = g_variant_get_data(value);
				mmgui_smsdb_message_set_binary(message, TRUE);
				mmgui_smsdb_message_set_data(message, valuestr, strlength, FALSE);
				gottext = TRUE;
			}
			g_variant_unref(value);
		}
	}
	
	/*Message timestamp (ModemManager uses string, Wader uses double)*/
	if (moduledata->service == MODULE_INT_SERVICE_MODEM_MANAGER) {
		value = g_variant_lookup_value(messagev, "timestamp", G_VARIANT_TYPE_STRING);
		if (value != NULL) {
			strlength = 256;
			valuestr = g_variant_get_string(value, &strlength);
			if ((valuestr != NULL) && (valuestr[0] != '\0')) {
				mmgui_smsdb_message_set_timestamp(message, mmgui_module_str_to_time(valuestr));
				g_debug("SMS timestamp: %s", ctime((const time_t *)&message->timestamp));
			}
			g_variant_unref(value);
		}
	} else if (moduledata->service == MODULE_INT_SERVICE_WADER) {
		value = g_variant_lookup_value(messagev, "timestamp", G_VARIANT_TYPE_DOUBLE);
		if (value != NULL) {
			mmgui_smsdb_message_set_timestamp(message, (time_t)g_variant_get_double(value));
			g_debug("SMS timestamp: %s", ctime((const time_t *)&message->timestamp));
			g_variant_unref(value);
		}
	}
	
	/*Message index (ModemManager uses unsigned integer, Wader uses signed one.)
	  This is a critical parameter, so message will be skipped if index value unknown.*/
	gotindex = FALSE;
	if (moduledata->service == MODULE_INT_SERVICE_MODEM_MANAGER) {
		value = g_variant_lookup_value(messagev, "index", G_VARIANT_TYPE_UINT32);
		if (value != NULL) {
			index = g_variant_get_uint32(value);
			mmgui_smsdb_message_set_identifier(message, index, FALSE);
			g_debug("SMS index: %u\n", index);
			gotindex = TRUE;
			g_variant_unref(value);
		}
	} else if (moduledata->service == MODULE_INT_SERVICE_WADER) {
		value = g_variant_lookup_value(messagev, "index", G_VARIANT_TYPE_INT32);
		if (value != NULL) {
			index = (guint)g_variant_get_int32(value);
			mmgui_smsdb_message_set_identifier(message, index, FALSE);
			g_debug("SMS index: %u\n", index);
			gotindex = TRUE;
			g_variant_unref(value);
		}
	}
	
	if ((!gotindex) || (!gottext)) {
		/*Message identifier unknown or no text - skip it*/
		mmgui_smsdb_message_free(message);
		return NULL;
	}
	
	return message;
}

G_MODULE_EXPORT guint mmgui_module_sms_enum(gpointer mmguicore, GSList **smslist)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *messages;
	guint msgnum;
	GVariantIter miterl1, miterl2;
	GVariant *mnodel1, *mnodel2;
	mmgui_sms_message_t message;
		
	if ((mmguicore == NULL) || (smslist == NULL)) return 0;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return 0;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->smsproxy == NULL) return 0;
	if (mmguicorelc->device == NULL) return 0;
	if (!mmguicorelc->device->enabled) return 0;
	if (!(mmguicorelc->device->smscaps & MMGUI_SMS_CAPS_RECEIVE)) return 0;
	
	error = NULL;
		
	messages = g_dbus_proxy_call_sync(moduledata->smsproxy,
									"List",
									NULL,
									0,
									-1,
									NULL,
									&error);
	
	if ((messages == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return 0;
	}
	
	msgnum = 0;
	
	g_variant_iter_init(&miterl1, messages);
	while ((mnodel1 = g_variant_iter_next_value(&miterl1)) != NULL) {
		g_variant_iter_init(&miterl2, mnodel1);
		while ((mnodel2 = g_variant_iter_next_value(&miterl2)) != NULL) {
			message = mmgui_module_sms_retrieve(mmguicore, mnodel2);
			if (message != NULL) {
				*smslist = g_slist_prepend(*smslist, message);
				msgnum++;
			}
			g_variant_unref(mnodel2);
		}
		g_variant_unref(mnodel1);
    }
    
    g_variant_unref(messages);
	
	return msgnum;
}

G_MODULE_EXPORT mmgui_sms_message_t mmgui_module_sms_get(gpointer mmguicore, guint index)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *indexv;
	GVariant *messagevt, *messagev;
	mmgui_sms_message_t message;
	
	if (mmguicore == NULL) return NULL;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return NULL;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->smsproxy == NULL) return NULL;
	if (mmguicorelc->device == NULL) return NULL;
	if (!mmguicorelc->device->enabled) return NULL;
	if (!(mmguicorelc->device->smscaps & MMGUI_SMS_CAPS_RECEIVE)) return NULL;
	
	error = NULL;
	
	indexv = g_variant_new("(u)", index);
	
	messagevt = g_dbus_proxy_call_sync(moduledata->smsproxy,
									"Get",
									indexv,
									0,
									-1,
									NULL,
									&error);
	
	if ((messagevt == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return NULL;
	}
	
	messagev = g_variant_get_child_value(messagevt, 0);
	
	message = mmgui_module_sms_retrieve(mmguicore, messagev);
	
	g_variant_unref(messagev);
	
	g_variant_unref(messagevt);
	
	return message;
}

G_MODULE_EXPORT gboolean mmgui_module_sms_delete(gpointer mmguicore, guint index)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *indexv;
			
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->smsproxy == NULL) return FALSE;
	if (mmguicorelc->device == NULL) return FALSE;
	if (!mmguicorelc->device->enabled) return FALSE;
	if (!(mmguicorelc->device->smscaps & MMGUI_SMS_CAPS_RECEIVE)) return FALSE;
	
	error = NULL;
	
	indexv = g_variant_new("(u)", index);
	
	g_dbus_proxy_call_sync(moduledata->smsproxy,
							"Delete",
							indexv,
							0,
							-1,
							NULL,
							&error);
	
	if (error != NULL) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	}
	
	return TRUE;
}

static void mmgui_module_sms_send_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *result;
	gboolean sent;
	
	mmguicorelc = (mmguicore_t)user_data;
	if (mmguicorelc == NULL) return;
	
	if (mmguicorelc->moduledata == NULL) return;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	error = NULL;
	
	result = g_dbus_proxy_call_finish(proxy, res, &error);
	
	if ((result == NULL) && (error != NULL)) {
		if ((moduledata->cancellable == NULL) || ((moduledata->cancellable != NULL) && (!g_cancellable_is_cancelled(moduledata->cancellable)))) {
			mmgui_module_handle_error_message(mmguicorelc, error);
		}
		g_error_free(error);
		sent = FALSE;
	} else {
		g_variant_unref(result);
		sent = TRUE;
	}
	
	if (mmguicorelc->device != NULL) {
		mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_IDLE;
	}
	
	if ((mmguicorelc->eventcb != NULL) && ((moduledata->cancellable == NULL) || ((moduledata->cancellable != NULL) && (!g_cancellable_is_cancelled(moduledata->cancellable))))) {
		(mmguicorelc->eventcb)(MMGUI_EVENT_SMS_SENT, user_data, GUINT_TO_POINTER(sent));
	}
}

G_MODULE_EXPORT gboolean mmgui_module_sms_send(gpointer mmguicore, gchar* number, gchar *text, gint validity, gboolean report)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GVariantBuilder *builder;
	GVariant *array, *message;
	
	if ((number == NULL) || (text == NULL)) return FALSE;
			
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->smsproxy == NULL) return FALSE;
	if (mmguicorelc->device == NULL) return FALSE;
	if (!mmguicorelc->device->enabled) return FALSE;
	if (!(mmguicorelc->device->smscaps & MMGUI_SMS_CAPS_SEND)) return FALSE;
	
	builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add_parsed(builder, "{'number', <%s>}", number);
	g_variant_builder_add_parsed(builder, "{'text', <%s>}", text);
	if ((validity > -1) && (validity <= 255)) {
		g_variant_builder_add_parsed(builder, "{'validity', <%u>}", (guint)validity);
	}
	array = g_variant_builder_end(builder);
	
	builder = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
	g_variant_builder_add_value(builder, array);
	message = g_variant_builder_end(builder);
	
	mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_SEND_SMS;
	
	if (moduledata->cancellable != NULL) {
		g_cancellable_reset(moduledata->cancellable);
	}
	
	g_dbus_proxy_call(moduledata->smsproxy,
					"Send",
					message,
					G_DBUS_CALL_FLAGS_NONE,
					moduledata->timeouts[MMGUI_DEVICE_OPERATION_SEND_SMS],
					moduledata->cancellable,
					(GAsyncReadyCallback)mmgui_module_sms_send_handler,
					mmguicore);
	
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_ussd_cancel_session(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->ussdproxy == NULL) return FALSE;
	if (mmguicorelc->device == NULL) return FALSE;
	if (!mmguicorelc->device->enabled) return FALSE;
	if (!(mmguicorelc->device->ussdcaps & MMGUI_USSD_CAPS_SEND)) return FALSE;
	
	error = NULL;
	
	g_dbus_proxy_call_sync(moduledata->ussdproxy,
							"Cancel",
							NULL,
							0,
							-1,
							NULL,
							&error);
	
	if (error != NULL) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	}
	
	return TRUE;
}

G_MODULE_EXPORT enum _mmgui_ussd_state mmgui_module_ussd_get_state(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GVariant *session;
	gchar *state;
	enum _mmgui_ussd_state stateid;
	gsize strsize;
	
	stateid = MMGUI_USSD_STATE_UNKNOWN;
	
	if (mmguicore == NULL) return stateid;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return stateid;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->ussdproxy == NULL) return stateid;
	if (mmguicorelc->device == NULL) return stateid;
	if (!mmguicorelc->device->enabled) return stateid;
	if (!(mmguicorelc->device->ussdcaps & MMGUI_USSD_CAPS_SEND)) return stateid;
	
	session = g_dbus_proxy_get_cached_property(moduledata->ussdproxy, "State");
	
	if (session == NULL) return stateid;
	
	strsize = 256;
	
	state = (gchar *)g_variant_get_string(session, &strsize);
	
	if (state != NULL) {
		if (g_str_equal(state, "idle")) {
			stateid = MMGUI_USSD_STATE_IDLE;
		} else if (g_str_equal(state, "active")) {
			stateid = MMGUI_USSD_STATE_ACTIVE;
		} else if (g_str_equal(state, "user-response")) {
			stateid = MMGUI_USSD_STATE_USER_RESPONSE;
		}
	}
	
	g_variant_unref(session);
	
	return stateid;
}

static void mmgui_module_ussd_send_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *result;
	gchar *answer;
		
	mmguicorelc = (mmguicore_t)user_data;
	if (mmguicorelc == NULL) return;
	
	if (mmguicorelc->moduledata == NULL) return;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
		
	error = NULL;
	answer = NULL;
	
	result = g_dbus_proxy_call_finish(proxy, res, &error);
	
	if ((result == NULL) && (error != NULL)) {
		/*For some reason after timeout ussd does not work - restart it*/
		mmgui_module_devices_restart_ussd(mmguicorelc);
		if ((moduledata->cancellable == NULL) || ((moduledata->cancellable != NULL) && (!g_cancellable_is_cancelled(moduledata->cancellable)))) {
			mmgui_module_handle_error_message(mmguicorelc, error);
		}
		g_error_free(error);
	} else {
		g_variant_get(result, "(s)", &answer);
		if (moduledata->reencodeussd) {
			/*Fix answer broken encoding*/
			answer = encoding_ussd_gsm7_to_ucs2(answer);
		} else {
			/*Do not touch answer*/
			answer = g_strdup(answer);
		}
		g_variant_unref(result);
	}
		
	if (mmguicorelc->device != NULL) {
		mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_IDLE;
	}
	
	if ((mmguicorelc->eventcb != NULL) && ((moduledata->cancellable == NULL) || ((moduledata->cancellable != NULL) && (!g_cancellable_is_cancelled(moduledata->cancellable))))) {
		(mmguicorelc->eventcb)(MMGUI_EVENT_USSD_RESULT, user_data, answer);
	}
}

G_MODULE_EXPORT gboolean mmgui_module_ussd_send(gpointer mmguicore, gchar *request, enum _mmgui_ussd_validation validationid, gboolean reencode)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	enum _mmgui_ussd_state sessionstate;
	GVariant *ussdreq;
	gchar *command;
	
	if ((mmguicore == NULL) || (request == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->ussdproxy == NULL) return FALSE;
	if (mmguicorelc->device == NULL) return FALSE;
	if (!mmguicorelc->device->enabled) return FALSE;
	if (!(mmguicorelc->device->ussdcaps & MMGUI_USSD_CAPS_SEND)) return FALSE;
	
	sessionstate = mmgui_module_ussd_get_state(mmguicore);
	
	if ((sessionstate == MMGUI_USSD_STATE_UNKNOWN) || 
		(sessionstate == MMGUI_USSD_STATE_ACTIVE)) {
		mmgui_module_ussd_cancel_session(mmguicore);
	}
	
	ussdreq = g_variant_new("(s)", request);
	
	command = NULL;
	
	if (sessionstate == MMGUI_USSD_STATE_IDLE) {
		command = "Initiate";
	} else if (sessionstate == MMGUI_USSD_STATE_USER_RESPONSE) {
		if (validationid == MMGUI_USSD_VALIDATION_REQUEST) {
			mmgui_module_ussd_cancel_session(mmguicore);
			command = "Initiate";
		} else {
			command = "Respond";
		}
	}
	
	moduledata->reencodeussd = reencode;
	
	mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_SEND_USSD;
	
	if (moduledata->cancellable != NULL) {
		g_cancellable_reset(moduledata->cancellable);
	}
	
	g_dbus_proxy_call(moduledata->ussdproxy,
					command,
					ussdreq,
					G_DBUS_CALL_FLAGS_NONE,
					moduledata->timeouts[MMGUI_DEVICE_OPERATION_SEND_USSD],
					moduledata->cancellable,
					(GAsyncReadyCallback)mmgui_module_ussd_send_handler,
					mmguicore);
    
    return TRUE;
}

static mmgui_scanned_network_t mmgui_module_network_retrieve(GVariant *networkv)
{
	mmgui_scanned_network_t network;
	GVariant *value;
	gsize strlength;
	const gchar *valuestr;
	/*guint i;*/
	
	if (networkv == NULL) return NULL;
		
	network = g_new0(struct _mmgui_scanned_network, 1);
	//Mobile operator code (MCCMNC)
	value  = g_variant_lookup_value(networkv, "operator-num", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		network->operator_num = atoi(valuestr);
		g_variant_unref(value);
	} else {
		network->operator_num = 0;
	}
	//Network access technology
	value = g_variant_lookup_value(networkv, "access-tech", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		network->access_tech = atoi(valuestr);
		g_variant_unref(value);
	} else {
		network->access_tech = MMGUI_ACCESS_TECH_GSM;
	}
	//Long-format name of operator
	value = g_variant_lookup_value(networkv, "operator-long", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		network->operator_long = g_strdup(valuestr);
		g_variant_unref(value);
	} else {
		network->operator_long = g_strdup("--");
	}
	//Short-format name of operator
	value = g_variant_lookup_value(networkv, "operator-short", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		network->operator_short = g_strdup(valuestr);
		g_variant_unref(value);
	} else {
		network->operator_short = g_strdup("--");
	}
	//Network availability status (this is a critical parameter, so entry will be skipped if value is unknown)
	value = g_variant_lookup_value(networkv, "status", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		network->status = atoi(valuestr);
		g_variant_unref(value);
		return network;
	} else {
		if (network->operator_long != NULL) g_free(network->operator_long);
		if (network->operator_short != NULL) g_free(network->operator_short);
		g_free(network);
		return NULL;
	}
}

static void mmgui_module_networks_scan_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *result;
	GSList *networks;
	GVariantIter niterl1, niterl2;
	GVariant *nnodel1, *nnodel2;
	mmgui_scanned_network_t network;
	
	mmguicorelc = (mmguicore_t)user_data;
	if (mmguicorelc == NULL) return;
	
	if (mmguicorelc->moduledata == NULL) return;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	error = NULL;
	networks = NULL;
	
	result = g_dbus_proxy_call_finish(proxy, res, &error);
	
	if ((result == NULL) && (error != NULL)) {
		if ((moduledata->cancellable == NULL) || ((moduledata->cancellable != NULL) && (!g_cancellable_is_cancelled(moduledata->cancellable)))) {
			mmgui_module_handle_error_message(mmguicorelc, error);
		}
		g_error_free(error);
	} else {
		g_variant_iter_init(&niterl1, result);
		while ((nnodel1 = g_variant_iter_next_value(&niterl1)) != NULL) {
			g_variant_iter_init(&niterl2, nnodel1);
			while ((nnodel2 = g_variant_iter_next_value(&niterl2)) != NULL) {
				network = mmgui_module_network_retrieve(nnodel2);
				if (network != NULL) {
					networks = g_slist_prepend(networks, network);
				}
				g_variant_unref(nnodel2);
			}
			g_variant_unref(nnodel1);
		}
		g_variant_unref(result);
	}
	
	if (mmguicorelc->device != NULL) {
		mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_IDLE;
	}
	
	if ((mmguicorelc->eventcb != NULL) && ((moduledata->cancellable == NULL) || ((moduledata->cancellable != NULL) && (!g_cancellable_is_cancelled(moduledata->cancellable))))) {
		(mmguicorelc->eventcb)(MMGUI_EVENT_SCAN_RESULT, user_data, networks);
	}
}

G_MODULE_EXPORT gboolean mmgui_module_networks_scan(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
		
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->netproxy == NULL) return FALSE;
	if (mmguicorelc->device == NULL) return FALSE;
	if (!mmguicorelc->device->enabled) return FALSE;
	if (!(mmguicorelc->device->scancaps & MMGUI_SCAN_CAPS_OBSERVE)) return FALSE;
	
	mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_SCAN;
	
	if (moduledata->cancellable != NULL) {
		g_cancellable_reset(moduledata->cancellable);
	}
	
	g_dbus_proxy_call(moduledata->netproxy,
					"Scan",
					NULL,
					G_DBUS_CALL_FLAGS_NONE,
					moduledata->timeouts[MMGUI_DEVICE_OPERATION_SCAN],
					moduledata->cancellable,
					(GAsyncReadyCallback)mmgui_module_networks_scan_handler,
					mmguicore);
	
	return TRUE;
}

static mmgui_contact_t mmgui_module_contact_retrieve(GVariant *contactv)
{
	mmgui_contact_t contact;
			
	if (contactv == NULL) return NULL;
		
	contact = g_new0(struct _mmgui_contact, 1);
	
	g_variant_get(contactv, "(uss)", &contact->id, &contact->name, &contact->number);
	
	//Full name of the contact
	if (contact->name != NULL) {
		contact->name = g_strdup(contact->name);
	} else {
		g_free(contact);
		return NULL;
	}
	//Telephone number
	if (contact->number != NULL) {
		contact->number = g_strdup(contact->number);
	} else {
		g_free(contact->name);
		g_free(contact);
		return NULL;
	}
	//Email address
	contact->email = NULL;
	//Group this contact belongs to
	contact->group = g_strdup("SIM");
	//Additional contact name
	contact->name2 = NULL;
	//Additional contact telephone number
	contact->number2 = NULL;
	//Boolean flag to specify whether this entry is hidden or not
	contact->hidden = FALSE;
	//Phonebook in which the contact is stored
	contact->storage = MMGUI_MODEM_CONTACTS_STORAGE_ME;
	
	return contact;
}

G_MODULE_EXPORT guint mmgui_module_contacts_enum(gpointer mmguicore, GSList **contactslist)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *contacts;
	guint contactsnum;
	GVariantIter citerl1, citerl2;
	GVariant *cnodel1, *cnodel2;
	mmgui_contact_t contact;
			
	if ((mmguicore == NULL) || (contactslist == NULL)) return 0;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return 0;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->service != MODULE_INT_SERVICE_WADER) return 0;
	if (moduledata->contactsproxy == NULL) return 0;
	if (mmguicorelc->device == NULL) return 0;
	if (!mmguicorelc->device->enabled) return 0;
	if (!(mmguicorelc->device->contactscaps & MMGUI_CONTACTS_CAPS_EXPORT)) return 0;
		
	error = NULL;
		
	contacts = g_dbus_proxy_call_sync(moduledata->contactsproxy,
									"List",
									NULL,
									0,
									-1,
									NULL,
									&error);
	
	if ((contacts == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return 0;
	}
	
	contactsnum = 0;
	
	g_variant_iter_init(&citerl1, contacts);
	while ((cnodel1 = g_variant_iter_next_value(&citerl1)) != NULL) {
		g_variant_iter_init(&citerl2, cnodel1);
		while ((cnodel2 = g_variant_iter_next_value(&citerl2)) != NULL) {
			contact = mmgui_module_contact_retrieve(cnodel2);
			if (contact != NULL) {
				*contactslist = g_slist_prepend(*contactslist, contact);
				contactsnum++;
			}
			g_variant_unref(cnodel2);
		}
		g_variant_unref(cnodel1);
	}
	
	g_variant_unref(contacts);
	
	if (contactsnum > 0) {
		*contactslist = g_slist_reverse(*contactslist);
	}
	
	return contactsnum;
}

G_MODULE_EXPORT gboolean mmgui_module_contacts_delete(gpointer mmguicore, guint index)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->contactsproxy == NULL) return FALSE;
	if (mmguicorelc->device == NULL) return FALSE;
	if (!mmguicorelc->device->enabled) return FALSE;
	if (!(mmguicorelc->device->contactscaps & MMGUI_CONTACTS_CAPS_EDIT)) return FALSE;
	
	error = NULL;
	
	g_dbus_proxy_call_sync(moduledata->contactsproxy,
							"Delete",
							g_variant_new("(u)", index),
							0,
							-1,
							NULL,
							&error);
	
	if (error != NULL) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	}
	
	return TRUE;
}

G_MODULE_EXPORT gint mmgui_module_contacts_add(gpointer mmguicore, mmgui_contact_t contact)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GVariant *contactv, *idv;
	GError *error;
	guint id;
	
	if ((mmguicore == NULL) || (contact == NULL)) return -1;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return -1;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if ((contact->name == NULL) || (contact->number == NULL)) return -1;
	if (moduledata->contactsproxy == NULL) return -1;
	if (mmguicorelc->device == NULL) return -1;
	if (!mmguicorelc->device->enabled) return -1;
	if (!(mmguicorelc->device->contactscaps & MMGUI_CONTACTS_CAPS_EDIT)) return -1;
	
	contactv = g_variant_new("(ss)", contact->name, contact->number);
	
	error = NULL;
	
	idv = g_dbus_proxy_call_sync(moduledata->contactsproxy,
									"Add",
									contactv,
									0,
									-1,
									NULL,
									&error);
	
	if ((idv == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return -1;
	}
	
	g_variant_get(idv, "(u)", &id);
	g_variant_unref(idv);
	
	contact->id = id;
	
	return id;
}

