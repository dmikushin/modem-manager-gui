/*
 *      mm07.c
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
#include <stdlib.h>

#include "../mmguicore.h"
#include "../smsdb.h"
#include "../encoding.h"
#include "../dbus-utils.h"

#define MMGUI_MODULE_SERVICE_NAME  "org.freedesktop.ModemManager1"
#define MMGUI_MODULE_SYSTEMD_NAME  "ModemManager.service"
#define MMGUI_MODULE_IDENTIFIER    70
#define MMGUI_MODULE_DESCRIPTION   "Modem Manager >= 0.7.0"
#define MMGUI_MODULE_COMPATIBILITY "org.freedesktop.NetworkManager;/usr/sbin/pppd;"

#define MMGUI_MODULE_ENABLE_OPERATION_TIMEOUT           20000
#define MMGUI_MODULE_SEND_SMS_OPERATION_TIMEOUT         35000
#define MMGUI_MODULE_SEND_USSD_OPERATION_TIMEOUT        25000
#define MMGUI_MODULE_NETWORKS_SCAN_OPERATION_TIMEOUT    60000
#define MMGUI_MODULE_NETWORKS_UNLOCK_OPERATION_TIMEOUT  20000

/*Internal enumerations*/
/*Modem state internal flags*/
typedef enum {
	MODULE_INT_MODEM_STATE_FAILED        = -1,
	MODULE_INT_MODEM_STATE_UNKNOWN       = 0,
	MODULE_INT_MODEM_STATE_INITIALIZING  = 1,
	MODULE_INT_MODEM_STATE_LOCKED        = 2,
	MODULE_INT_MODEM_STATE_DISABLED      = 3,
	MODULE_INT_MODEM_STATE_DISABLING     = 4,
	MODULE_INT_MODEM_STATE_ENABLING      = 5,
	MODULE_INT_MODEM_STATE_ENABLED       = 6,
	MODULE_INT_MODEM_STATE_SEARCHING     = 7,
	MODULE_INT_MODEM_STATE_REGISTERED    = 8,
	MODULE_INT_MODEM_STATE_DISCONNECTING = 9,
	MODULE_INT_MODEM_STATE_CONNECTING    = 10,
	MODULE_INT_MODEM_STATE_CONNECTED     = 11
} ModuleIntModemState;

/*Modem lock flags*/
typedef enum {
	MODULE_INT_MODEM_LOCK_UNKNOWN        = 0,
	MODULE_INT_MODEM_LOCK_NONE           = 1,
	MODULE_INT_MODEM_LOCK_SIM_PIN        = 2,
	MODULE_INT_MODEM_LOCK_SIM_PIN2       = 3,
	MODULE_INT_MODEM_LOCK_SIM_PUK        = 4,
	MODULE_INT_MODEM_LOCK_SIM_PUK2       = 5,
	MODULE_INT_MODEM_LOCK_PH_SP_PIN      = 6,
	MODULE_INT_MODEM_LOCK_PH_SP_PUK      = 7,
	MODULE_INT_MODEM_LOCK_PH_NET_PIN     = 8,
	MODULE_INT_MODEM_LOCK_PH_NET_PUK     = 9,
	MODULE_INT_MODEM_LOCK_PH_SIM_PIN     = 10,
	MODULE_INT_MODEM_LOCK_PH_CORP_PIN    = 11,
	MODULE_INT_MODEM_LOCK_PH_CORP_PUK    = 12,
	MODULE_INT_MODEM_LOCK_PH_FSIM_PIN    = 13,
	MODULE_INT_MODEM_LOCK_PH_FSIM_PUK    = 14,
	MODULE_INT_MODEM_LOCK_PH_NETSUB_PIN  = 15,
	MODULE_INT_MODEM_LOCK_PH_NETSUB_PUK  = 16
} ModuleIntModemLock;

/*Modem capability internal flags*/
typedef enum {
	MODULE_INT_MODEM_CAPABILITY_NONE         = 0,
	MODULE_INT_MODEM_CAPABILITY_POTS         = 1 << 0,
	MODULE_INT_MODEM_CAPABILITY_CDMA_EVDO    = 1 << 1,
	MODULE_INT_MODEM_CAPABILITY_GSM_UMTS     = 1 << 2,
	MODULE_INT_MODEM_CAPABILITY_LTE          = 1 << 3,
	MODULE_INT_MODEM_CAPABILITY_LTE_ADVANCED = 1 << 4,
	MODULE_INT_MODEM_CAPABILITY_IRIDIUM      = 1 << 5,
} ModuleIntModemCapability;

/*Modem registration internal flags*/
typedef enum {
    MODULE_INT_MODEM_3GPP_REGISTRATION_STATE_IDLE      = 0,
    MODULE_INT_MODEM_3GPP_REGISTRATION_STATE_HOME      = 1,
    MODULE_INT_MODEM_3GPP_REGISTRATION_STATE_SEARCHING = 2,
    MODULE_INT_MODEM_3GPP_REGISTRATION_STATE_DENIED    = 3,
    MODULE_INT_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN   = 4,
    MODULE_INT_MODEM_3GPP_REGISTRATION_STATE_ROAMING   = 5,
} ModuleIntModem3gppRegistrationState;

/*CDMA modem registration internal flags*/
typedef enum {
    MODULE_INT_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN    = 0,
    MODULE_INT_MODEM_CDMA_REGISTRATION_STATE_REGISTERED = 1,
    MODULE_INT_MODEM_CDMA_REGISTRATION_STATE_HOME       = 2,
    MODULE_INT_MODEM_CDMA_REGISTRATION_STATE_ROAMING    = 3,
} ModuleIntModemCdmaRegistrationState;

/*Modem USSD state internal flags*/
typedef enum {
	MODULE_INT_MODEM_3GPP_USSD_SESSION_STATE_UNKNOWN       = 0,
	MODULE_INT_MODEM_3GPP_USSD_SESSION_STATE_IDLE          = 1,
	MODULE_INT_MODEM_3GPP_USSD_SESSION_STATE_ACTIVE        = 2,
	MODULE_INT_MODEM_3GPP_USSD_SESSION_STATE_USER_RESPONSE = 3,
} ModuleIntModem3gppUssdSessionState;

/*Modem network availability internal flags*/
typedef enum {
    MODULE_INT_MODEM_3GPP_NETWORK_AVAILABILITY_UNKNOWN   = 0,
    MODULE_INT_MODEM_3GPP_NETWORK_AVAILABILITY_AVAILABLE = 1,
    MODULE_INT_MODEM_3GPP_NETWORK_AVAILABILITY_CURRENT   = 2,
    MODULE_INT_MODEM_3GPP_NETWORK_AVAILABILITY_FORBIDDEN = 3,
} ModuleIntModem3gppNetworkAvailability;

/*Modem acess technology internal flags*/
typedef enum {
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_UNKNOWN     = 0,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_POTS        = 1 << 0,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_GSM         = 1 << 1,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT = 1 << 2,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_GPRS        = 1 << 3,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_EDGE        = 1 << 4,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_UMTS        = 1 << 5,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_HSDPA       = 1 << 6,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_HSUPA       = 1 << 7,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_HSPA        = 1 << 8,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS   = 1 << 9,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_1XRTT       = 1 << 10,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_EVDO0       = 1 << 11,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_EVDOA       = 1 << 12,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_EVDOB       = 1 << 13,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_LTE         = 1 << 14,
    MODULE_INT_MODEM_ACCESS_TECHNOLOGY_ANY         = 0xFFFFFFFF,
} ModuleIntModemAccessTechnology;

/*Location types internal flags*/
typedef enum {
    MODULE_INT_MODEM_LOCATION_SOURCE_NONE        = 0,
    MODULE_INT_MODEM_LOCATION_SOURCE_3GPP_LAC_CI = 1 << 0,
    MODULE_INT_MODEM_LOCATION_SOURCE_GPS_RAW     = 1 << 1,
    MODULE_INT_MODEM_LOCATION_SOURCE_GPS_NMEA    = 1 << 2,
} ModuleIntModemLocationSource;

/*SMS message state internal flags*/
typedef enum {
	MODULE_INT_SMS_STATE_UNKNOWN   = 0,
	MODULE_INT_SMS_STATE_STORED    = 1,
	MODULE_INT_SMS_STATE_RECEIVING = 2,
	MODULE_INT_SMS_STATE_RECEIVED  = 3,
	MODULE_INT_SMS_STATE_SENDING   = 4,
	MODULE_INT_SMS_STATE_SENT      = 5,
} ModuleIntSmsState;

/*SMS message internal type*/
typedef enum {
	MODULE_INT_PDU_TYPE_UNKNOWN       = 0,
	MODULE_INT_PDU_TYPE_DELIVER       = 1,
	MODULE_INT_PDU_TYPE_SUBMIT        = 2,
	MODULE_INT_PDU_TYPE_STATUS_REPORT = 3
} ModuleIntSmsPduType;

/*SMS validity internal types*/
typedef enum {
	MODULE_INT_SMS_VALIDITY_TYPE_UNKNOWN  = 0,
    MODULE_INT_SMS_VALIDITY_TYPE_RELATIVE = 1,
    MODULE_INT_SMS_VALIDITY_TYPE_ABSOLUTE = 2,
    MODULE_INT_SMS_VALIDITY_TYPE_ENHANCED = 3,
} ModuleIntSmsValidityType;


/*Private module variables*/
struct _mmguimoduledata {
	//DBus connection
	GDBusConnection *connection;
	//DBus proxy objects
	GDBusObjectManager *objectmanager;
	GDBusProxy *cardproxy;
	GDBusProxy *netproxy;
	GDBusProxy *modemproxy;
	GDBusProxy *smsproxy;
	GDBusProxy *ussdproxy;
	GDBusProxy *locationproxy;
	GDBusProxy *timeproxy;
	GDBusProxy *contactsproxy;
	GDBusProxy *signalproxy;
	//Attached signal handlers
	gulong netpropsignal;
	gulong statesignal;
	gulong modempropsignal;
	gulong smssignal;
	gulong locationpropsignal;
	gulong timesignal;
	//Partial SMS messages
	GList *partialsms;
	//USSD reencoding flag
	gboolean reencodeussd;
	/*Location enablement flag*/
	gboolean locationenabled;
	//Last error message
	gchar *errormessage;
	//Cancellable
	GCancellable *cancellable;
	//Operations timeouts
	guint timeouts[MMGUI_DEVICE_OPERATIONS];
};

typedef struct _mmguimoduledata *moduledata_t;

static void mmgui_module_handle_error_message(mmguicore_t mmguicore, GError *error);
static void mmgui_module_custom_error_message(mmguicore_t mmguicore, gchar *message);
static guint mmgui_module_get_object_path_index(const gchar *objectpath);
static gint mmgui_module_gsm_operator_code(const gchar *opcodestr);
static void mmgui_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data);
static void mmgui_property_change_handler(GDBusProxy *proxy, GVariant *changed_properties, GStrv invalidated_properties, gpointer data);
static void mmgui_objectmanager_added_signal_handler(GDBusObjectManager *manager, GDBusObject *object, gpointer user_data);
static void mmgui_objectmanager_removed_signal_handler(GDBusObjectManager *manager, GDBusObject *object, gpointer user_data);
static gboolean mmgui_module_device_enabled_from_state(gint state);
static gboolean mmgui_module_device_locked_from_state(gint state);
static gboolean mmgui_module_device_connected_from_state(gint state);
static gboolean mmgui_module_device_registered_from_state(gint state);
static gboolean mmgui_module_device_prepared_from_state(gint state);
static enum _mmgui_device_types mmgui_module_device_type_from_caps(gint caps);
static enum _mmgui_reg_status mmgui_module_registration_status_translate(guint status);
static enum _mmgui_reg_status mmgui_module_cdma_registration_status_translate(guint status);
static enum _mmgui_network_availability mmgui_module_network_availability_status_translate(guint status);
static enum _mmgui_access_tech mmgui_module_access_technology_translate(guint technology);
static enum _mmgui_device_modes mmgui_module_access_mode_translate(guint mode);
static gboolean mmgui_module_devices_update_device_mode(gpointer mmguicore, gint oldstate, gint newstate, guint changereason);
static gboolean mmgui_module_devices_update_location(gpointer mmguicore, mmguidevice_t device);
static gboolean mmgui_module_devices_enable_location(gpointer mmguicore, mmguidevice_t device, gboolean enable);
static mmguidevice_t mmgui_module_device_new(mmguicore_t mmguicore, const gchar *devpath);
static mmgui_sms_message_t mmgui_module_sms_retrieve(mmguicore_t mmguicore, const gchar *smspath);
static gint mmgui_module_sms_get_id(mmguicore_t mmguicore, const gchar *smspath);


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

static void mmgui_module_custom_error_message(mmguicore_t mmguicore, gchar *message)
{
	moduledata_t moduledata;
	
	if (mmguicore == NULL) return;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	
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

static guint mmgui_module_get_object_path_index(const gchar *objectpath)
{
	guint id;
	gchar *objectpathid;
	
	objectpathid = strrchr(objectpath, '/') + 1;
	if ((objectpathid != NULL) && (objectpathid[0] != '\0')) {
		id = atoi(objectpathid);
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

static void mmgui_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data)
{
	mmguicore_t mmguicore;
	moduledata_t moduledata;
	gchar *statusstr;
	gboolean statusflag;
	gint oldstate, newstate;
	guint changereason;
		
	if (data == NULL) return;
	
	mmguicore = (mmguicore_t)data;
	moduledata = (moduledata_t)mmguicore->moduledata;
	
	if (g_str_equal(signal_name, "Added")) {
		g_variant_get(parameters, "(ob)", &statusstr, &statusflag);
		if (statusflag)	{
			/*Message received from network*/
			moduledata->partialsms = g_list_prepend(moduledata->partialsms, g_strdup(statusstr));
		}
	} else if (g_str_equal(signal_name, "StateChanged")) {
		g_variant_get(parameters, "(iiu)", &oldstate, &newstate, &changereason);
		/*Send signals if needed*/
		mmgui_module_devices_update_device_mode(mmguicore, oldstate, newstate, changereason);
	}
	
	g_debug("SIGNAL: %s (%s) argtype: %s\n", signal_name, sender_name, g_variant_get_type_string(parameters));
}

static void mmgui_property_change_handler(GDBusProxy *proxy, GVariant *changed_properties, GStrv invalidated_properties, gpointer data)
{
	mmguicore_t mmguicore;
	mmguidevice_t device;
	GVariantIter *iter;
	/*GVariant *item;*/
	const gchar *key;
	GVariant *value;
	guint statevalue;
	gboolean stateflag;
	
	if ((changed_properties == NULL) || (data == NULL)) return;
	
	mmguicore = (mmguicore_t)data;
	
	if (mmguicore->device == NULL) return;
	device = mmguicore->device;
	
	if (g_variant_n_children(changed_properties) > 0) {
		g_variant_get(changed_properties, "a{sv}", &iter);
		while (g_variant_iter_loop(iter, "{&sv}", &key, &value)) {
			if (g_str_equal(key, "SignalQuality")) {
				g_variant_get(value, "(ub)", &statevalue, &stateflag);
				if (statevalue != device->siglevel) {
					device->siglevel = statevalue;
					if (mmguicore->eventcb != NULL) {
						(mmguicore->eventcb)(MMGUI_EVENT_SIGNAL_LEVEL_CHANGE, mmguicore, mmguicore->device);
					}
				}
			} else if (g_str_equal(key, "AccessTechnologies")) {
				statevalue = mmgui_module_access_mode_translate(g_variant_get_uint32(value));
				if (statevalue != device->mode) {
					device->mode = statevalue;
					if (mmguicore->eventcb != NULL) {
						(mmguicore->eventcb)(MMGUI_EVENT_NETWORK_MODE_CHANGE, mmguicore, mmguicore->device);
					}
				}
				
			} else if (g_str_equal(key, "Location")) {
				if (mmgui_module_devices_update_location(mmguicore, mmguicore->device)) {
					if (mmguicore->eventcb != NULL) {
						(mmguicore->eventcb)(MMGUI_EVENT_LOCATION_CHANGE, mmguicore, mmguicore->device);
					}
				}
			}
			g_debug("Property changed: %s\n", key);
		}
		g_variant_iter_free(iter);
	}
}

static void mmgui_objectmanager_added_signal_handler(GDBusObjectManager *manager, GDBusObject *object, gpointer user_data)
{
	mmguicore_t mmguicore;
	const gchar *devpath;
	mmguidevice_t device;
	
	if ((user_data == NULL) || (object == NULL)) return;
	
	mmguicore = (mmguicore_t)user_data;
	
	if (mmguicore->eventcb != NULL) {
		devpath = g_dbus_object_get_object_path(object);
		g_debug("Device added: %s\n", devpath);
		if (devpath != NULL) {
			device = mmgui_module_device_new(mmguicore, devpath);
			(mmguicore->eventcb)(MMGUI_EVENT_DEVICE_ADDED, mmguicore, device);
		}
	}
}

static void mmgui_objectmanager_removed_signal_handler(GDBusObjectManager *manager, GDBusObject *object, gpointer user_data)
{
	mmguicore_t mmguicore;
	const gchar *devpath;
	guint id;
	
	if ((user_data == NULL) || (object == NULL)) return;
	
	mmguicore = (mmguicore_t)user_data;
	
	if (mmguicore->eventcb != NULL) {
		devpath = g_dbus_object_get_object_path(object);
		g_debug("Device removed: %s\n", devpath);
		if (devpath != NULL) {
			id = mmgui_module_get_object_path_index(devpath);
			(mmguicore->eventcb)(MMGUI_EVENT_DEVICE_REMOVED, mmguicore, GUINT_TO_POINTER(id));
		}
	}
}

static gboolean mmgui_module_device_enabled_from_state(gint state)
{
	gboolean enabled;
	
	switch (state) {
		case MODULE_INT_MODEM_STATE_FAILED:
		case MODULE_INT_MODEM_STATE_UNKNOWN:
		case MODULE_INT_MODEM_STATE_INITIALIZING:
		case MODULE_INT_MODEM_STATE_LOCKED:
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

static gboolean mmgui_module_device_locked_from_state(gint state)
{
	gboolean locked;
	
	switch (state) {
		case MODULE_INT_MODEM_STATE_FAILED:
		case MODULE_INT_MODEM_STATE_UNKNOWN:
		case MODULE_INT_MODEM_STATE_INITIALIZING:
			locked = FALSE;
			break;
		case MODULE_INT_MODEM_STATE_LOCKED:
			locked = TRUE;
			break;
		case MODULE_INT_MODEM_STATE_DISABLED:
		case MODULE_INT_MODEM_STATE_DISABLING:
		case MODULE_INT_MODEM_STATE_ENABLING:
		case MODULE_INT_MODEM_STATE_ENABLED:
		case MODULE_INT_MODEM_STATE_SEARCHING:
		case MODULE_INT_MODEM_STATE_REGISTERED:
		case MODULE_INT_MODEM_STATE_DISCONNECTING:
		case MODULE_INT_MODEM_STATE_CONNECTING:
		case MODULE_INT_MODEM_STATE_CONNECTED:
			locked = FALSE;
			break;
		default:
			locked = FALSE;
			break;
	}
	
	return locked;
}

static gboolean mmgui_module_device_connected_from_state(gint state)
{
	gboolean connected;
	
	switch (state) {
		case MODULE_INT_MODEM_STATE_FAILED:
		case MODULE_INT_MODEM_STATE_UNKNOWN:
		case MODULE_INT_MODEM_STATE_INITIALIZING:
		case MODULE_INT_MODEM_STATE_LOCKED:
		case MODULE_INT_MODEM_STATE_DISABLED:
		case MODULE_INT_MODEM_STATE_DISABLING:
		case MODULE_INT_MODEM_STATE_ENABLING:
		case MODULE_INT_MODEM_STATE_ENABLED:
		case MODULE_INT_MODEM_STATE_SEARCHING:
		case MODULE_INT_MODEM_STATE_REGISTERED:
			connected = FALSE;
			break;
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
		case MODULE_INT_MODEM_STATE_FAILED:
		case MODULE_INT_MODEM_STATE_UNKNOWN:
		case MODULE_INT_MODEM_STATE_INITIALIZING:
		case MODULE_INT_MODEM_STATE_LOCKED:
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

static gboolean mmgui_module_device_prepared_from_state(gint state)
{
	gboolean prepared;
	
	switch (state) {
		case MODULE_INT_MODEM_STATE_FAILED:
		case MODULE_INT_MODEM_STATE_UNKNOWN:
		case MODULE_INT_MODEM_STATE_INITIALIZING:
		case MODULE_INT_MODEM_STATE_DISABLING:
		case MODULE_INT_MODEM_STATE_ENABLING:
			prepared = FALSE;
			break;
		case MODULE_INT_MODEM_STATE_LOCKED:
		case MODULE_INT_MODEM_STATE_DISABLED:
		case MODULE_INT_MODEM_STATE_ENABLED:
		case MODULE_INT_MODEM_STATE_SEARCHING:
		case MODULE_INT_MODEM_STATE_REGISTERED:
		case MODULE_INT_MODEM_STATE_DISCONNECTING:
		case MODULE_INT_MODEM_STATE_CONNECTING:
		case MODULE_INT_MODEM_STATE_CONNECTED:
			prepared = TRUE;
			break;
		default:
			prepared = FALSE;
			break;
	}
	
	return prepared;
}

static enum _mmgui_device_types mmgui_module_device_type_from_caps(gint caps)
{
	enum _mmgui_device_types type;
	
	switch (caps) {
		case MODULE_INT_MODEM_CAPABILITY_NONE:
		case MODULE_INT_MODEM_CAPABILITY_POTS:
			type = MMGUI_DEVICE_TYPE_GSM;
			break;
		case MODULE_INT_MODEM_CAPABILITY_CDMA_EVDO:
			type = MMGUI_DEVICE_TYPE_CDMA;
			break;
		case MODULE_INT_MODEM_CAPABILITY_GSM_UMTS:
		case MODULE_INT_MODEM_CAPABILITY_LTE:
		case MODULE_INT_MODEM_CAPABILITY_LTE_ADVANCED:
		case MODULE_INT_MODEM_CAPABILITY_IRIDIUM:
			type = MMGUI_DEVICE_TYPE_GSM;
			break;
		default:
			type = MMGUI_DEVICE_TYPE_GSM;
			break;
	}
	
	return type;
}

static enum _mmgui_reg_status mmgui_module_registration_status_translate(guint status)
{
	enum _mmgui_reg_status tstatus;
	
	switch (status) {
		case MODULE_INT_MODEM_3GPP_REGISTRATION_STATE_IDLE:
			tstatus = MMGUI_REG_STATUS_IDLE;
			break;
		case MODULE_INT_MODEM_3GPP_REGISTRATION_STATE_HOME:
			tstatus = MMGUI_REG_STATUS_HOME;
			break;
		case MODULE_INT_MODEM_3GPP_REGISTRATION_STATE_SEARCHING:
			tstatus = MMGUI_REG_STATUS_SEARCHING;
			break;
		case MODULE_INT_MODEM_3GPP_REGISTRATION_STATE_DENIED:
			tstatus = MMGUI_REG_STATUS_DENIED;
			break;
		case MODULE_INT_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN:
			tstatus = MMGUI_REG_STATUS_UNKNOWN;
			break;
		case MODULE_INT_MODEM_3GPP_REGISTRATION_STATE_ROAMING:
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
		case MODULE_INT_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN:
			tstatus = MMGUI_REG_STATUS_UNKNOWN;
			break;
		case MODULE_INT_MODEM_CDMA_REGISTRATION_STATE_REGISTERED:
			tstatus = MMGUI_REG_STATUS_IDLE;
			break;	
		case MODULE_INT_MODEM_CDMA_REGISTRATION_STATE_HOME:
			tstatus = MMGUI_REG_STATUS_HOME;
			break;
		case MODULE_INT_MODEM_CDMA_REGISTRATION_STATE_ROAMING:
			tstatus = MMGUI_REG_STATUS_ROAMING;
			break;
		default:
			tstatus = MMGUI_REG_STATUS_UNKNOWN;
			break;
	}
	
	return tstatus;
}

static enum _mmgui_network_availability mmgui_module_network_availability_status_translate(guint status)
{
	guint tstatus;
	
	switch (status) {
		case MODULE_INT_MODEM_3GPP_NETWORK_AVAILABILITY_UNKNOWN:
			tstatus = MMGUI_NA_UNKNOWN;
			break;
		case MODULE_INT_MODEM_3GPP_NETWORK_AVAILABILITY_AVAILABLE:
			tstatus = MMGUI_NA_AVAILABLE;
			break;
		case MODULE_INT_MODEM_3GPP_NETWORK_AVAILABILITY_CURRENT:
			tstatus = MMGUI_NA_CURRENT;
			break;
		case MODULE_INT_MODEM_3GPP_NETWORK_AVAILABILITY_FORBIDDEN:
			tstatus = MMGUI_NA_FORBIDDEN;
			break;
		default:
			tstatus = MMGUI_NA_UNKNOWN;
			break;
	}
	
	return tstatus;	
}

static enum _mmgui_access_tech mmgui_module_access_technology_translate(guint technology)
{
	enum _mmgui_access_tech ttechnology;
	
	switch (technology) {
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_UNKNOWN:
			ttechnology = MMGUI_ACCESS_TECH_UNKNOWN;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_POTS:
			ttechnology = MMGUI_ACCESS_TECH_UNKNOWN;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_GSM:
			ttechnology = MMGUI_ACCESS_TECH_GSM;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT:
			ttechnology = MMGUI_ACCESS_TECH_GSM_COMPACT;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_GPRS:
			ttechnology = MMGUI_ACCESS_TECH_EDGE;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_EDGE:
			ttechnology = MMGUI_ACCESS_TECH_EDGE;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_UMTS:
			ttechnology = MMGUI_ACCESS_TECH_UMTS;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_HSDPA:
			ttechnology = MMGUI_ACCESS_TECH_HSDPA;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_HSUPA:
			ttechnology = MMGUI_ACCESS_TECH_HSUPA;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_HSPA:
			ttechnology = MMGUI_ACCESS_TECH_HSPA;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS:
			ttechnology = MMGUI_ACCESS_TECH_HSPA_PLUS;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_1XRTT:
			ttechnology = MMGUI_ACCESS_TECH_1XRTT;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_EVDO0:
			ttechnology = MMGUI_ACCESS_TECH_EVDO0;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_EVDOA:
			ttechnology = MMGUI_ACCESS_TECH_EVDOA;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_EVDOB:
			ttechnology = MMGUI_ACCESS_TECH_EVDOB;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_LTE:
			ttechnology = MMGUI_ACCESS_TECH_LTE;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_ANY:
			ttechnology = MMGUI_ACCESS_TECH_UNKNOWN;
			break;
		default:
			ttechnology = MMGUI_ACCESS_TECH_UNKNOWN;
			break;
	}

	return ttechnology;
}

static enum _mmgui_device_modes mmgui_module_access_mode_translate(guint mode)
{
	enum _mmgui_device_modes tmode;
	
	switch (mode) {
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_UNKNOWN:
			tmode = MMGUI_DEVICE_MODE_UNKNOWN;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_POTS:
			tmode = MMGUI_DEVICE_MODE_UNKNOWN;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_GSM:
			tmode = MMGUI_DEVICE_MODE_GSM;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT:
			tmode = MMGUI_DEVICE_MODE_GSM_COMPACT;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_GPRS:
			tmode = MMGUI_DEVICE_MODE_GPRS;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_EDGE:
			tmode = MMGUI_DEVICE_MODE_EDGE;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_UMTS:
			tmode = MMGUI_DEVICE_MODE_UMTS;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_HSDPA:
			tmode = MMGUI_DEVICE_MODE_HSDPA;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_HSUPA:
			tmode = MMGUI_DEVICE_MODE_HSUPA;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_HSPA:
			tmode = MMGUI_DEVICE_MODE_HSPA;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS:
			tmode = MMGUI_DEVICE_MODE_HSPA_PLUS;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_1XRTT:
			tmode = MMGUI_DEVICE_MODE_1XRTT;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_EVDO0:
			tmode = MMGUI_DEVICE_MODE_EVDO0;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_EVDOA:
			tmode = MMGUI_DEVICE_MODE_EVDOA;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_EVDOB:
			tmode = MMGUI_DEVICE_MODE_EVDOB;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_LTE:
			tmode = MMGUI_DEVICE_MODE_LTE;
			break;
		case MODULE_INT_MODEM_ACCESS_TECHNOLOGY_ANY:
			tmode = MMGUI_DEVICE_MODE_UNKNOWN;
			break;
		default:
			tmode = MMGUI_DEVICE_MODE_UNKNOWN;
			break;
	}

	return tmode;
}

static mmguidevice_t mmgui_module_device_new(mmguicore_t mmguicore, const gchar *devpath)
{
	mmguidevice_t device;
	moduledata_t moduledata;
	GDBusProxy *deviceproxy;
	GError *error;
	GVariant *deviceinfo;
	gsize strsize;
	guint statevalue;
	gchar *blockstr;
	
	if ((mmguicore == NULL) || (devpath == NULL)) return NULL;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	
	if (moduledata->connection == NULL) return NULL;
	
	device = g_new0(struct _mmguidevice, 1);
	
	device->id = mmgui_module_get_object_path_index(devpath);
	device->objectpath = g_strdup(devpath);
	
	device->operation = MMGUI_DEVICE_OPERATION_IDLE;
	/*Zero values we can't get this moment*/
	/*SMS*/
	device->smscaps = MMGUI_SMS_CAPS_NONE;
	device->smsdb = NULL;
	/*Networks*/
	/*Info*/
	device->manufacturer = NULL;
	device->model = NULL;
	device->version = NULL;
	device->operatorname = NULL;
	device->operatorcode = 0;
	device->imei = NULL;
	device->imsi = NULL;
	device->port = NULL;
	device->internalid = NULL;
	device->persistentid = NULL;
	device->sysfspath = NULL;
	/*USSD*/
	device->ussdcaps = MMGUI_USSD_CAPS_NONE;
	device->ussdencoding = MMGUI_USSD_ENCODING_GSM7;
	/*Location*/
	device->locationcaps = MMGUI_LOCATION_CAPS_NONE;
	memset(device->loc3gppdata, 0, sizeof(device->loc3gppdata));
	memset(device->locgpsdata, 0, sizeof(device->locgpsdata));
	/*Scan*/
	device->scancaps = MMGUI_SCAN_CAPS_NONE;
	/*Traffic*/
	device->rxbytes = 0;
	device->txbytes = 0;
	device->sessiontime = 0;
	device->speedchecktime = 0;
	device->smschecktime = 0;
	device->speedindex = 0;
	device->connected = FALSE;
	memset(device->speedvalues, 0, sizeof(device->speedvalues));
	memset(device->interface, 0, sizeof(device->interface));
	/*Contacts*/
	device->contactscaps = MMGUI_CONTACTS_CAPS_NONE;
	device->contactslist = NULL;
	
	error = NULL;
	
	deviceproxy = g_dbus_proxy_new_sync(moduledata->connection,
										G_DBUS_PROXY_FLAGS_NONE,
										NULL,
										"org.freedesktop.ModemManager1",
										devpath,
										"org.freedesktop.ModemManager1.Modem",
										NULL,
										&error);
	
	if ((deviceproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
		g_object_unref(deviceproxy);
		/*Fill default values*/
		device->manufacturer = g_strdup(_("Unknown"));
		device->model = g_strdup(_("Unknown"));
		device->version = g_strdup(_("Unknown"));
		device->port = g_strdup(_("Unknown"));
		device->type = MMGUI_DEVICE_TYPE_GSM;
		return device;
	}
	
	/*Device manufacturer*/
	deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "Manufacturer");
	if (deviceinfo != NULL) {
		strsize = 256;
		device->manufacturer = g_strdup(g_variant_get_string(deviceinfo, &strsize));
		g_variant_unref(deviceinfo);
	} else {
		device->manufacturer = g_strdup(_("Unknown"));
	}
	
	/*Device model*/
	deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "Model");
	if (deviceinfo != NULL) {
		strsize = 256;
		device->model = g_strdup(g_variant_get_string(deviceinfo, &strsize));
		g_variant_unref(deviceinfo);
	} else {
		device->model = g_strdup(_("Unknown"));
	}
	
	/*Device revision*/
	deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "Revision");
	if (deviceinfo != NULL) {
		strsize = 256;
		device->version = g_strdup(g_variant_get_string(deviceinfo, &strsize));
		g_variant_unref(deviceinfo);
	} else {
		device->version = g_strdup(_("Unknown"));
	}
	
	/*Device port*/
	deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "PrimaryPort");
	if (deviceinfo != NULL) {
		strsize = 256;
		device->port = g_strdup(g_variant_get_string(deviceinfo, &strsize));
		g_variant_unref(deviceinfo);
	} else {
		device->port = g_strdup("");
	}
	
	/*Need to get usb device serial for fallback traffic monitoring*/
	deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "Device");
	if (deviceinfo != NULL) {
		strsize = 256;
		device->sysfspath = g_strdup(g_variant_get_string(deviceinfo, &strsize));
		g_variant_unref(deviceinfo);
	} else {
		device->sysfspath = g_strdup("");
	}
	
	/*Device type (version 0.7.990 property)*/
	deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "ModemCapabilities");
	if (deviceinfo != NULL) {
		statevalue = g_variant_get_uint32(deviceinfo);
		device->type = mmgui_module_device_type_from_caps(statevalue);
		g_variant_unref(deviceinfo);
	} else {
		/*Device type (version 0.7.991 property)*/
		deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "CurrentCapabilities");
		if (deviceinfo != NULL) {
			statevalue = g_variant_get_uint32(deviceinfo);
			device->type = mmgui_module_device_type_from_caps(statevalue);
			g_variant_unref(deviceinfo);
		} else {
			device->type = MODULE_INT_MODEM_CAPABILITY_GSM_UMTS;
		}
	}
	
	/*Is device enabled, blocked and registered*/
	deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "State");
	if (deviceinfo != NULL) {
		statevalue = g_variant_get_int32(deviceinfo);
		device->enabled = mmgui_module_device_enabled_from_state(statevalue);
		device->blocked = mmgui_module_device_locked_from_state(statevalue);
		device->registered = mmgui_module_device_registered_from_state(statevalue);
		device->prepared = mmgui_module_device_prepared_from_state(statevalue);
		g_variant_unref(deviceinfo);
	} else {
		device->enabled = TRUE;
		device->blocked = FALSE;
		device->registered =  TRUE;
		device->prepared = TRUE;
	}
	
	/*What type of lock it is*/
	deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "UnlockRequired");
	if (deviceinfo != NULL) {
		switch (g_variant_get_uint32(deviceinfo)) {
			case MODULE_INT_MODEM_LOCK_NONE:
				device->locktype = MMGUI_LOCK_TYPE_NONE;
				break;
			case MODULE_INT_MODEM_LOCK_SIM_PIN:
				device->locktype = MMGUI_LOCK_TYPE_PIN;
				break;
			case MODULE_INT_MODEM_LOCK_SIM_PUK:
				device->locktype = MMGUI_LOCK_TYPE_PUK;
				break;
			default:
				device->locktype = MMGUI_LOCK_TYPE_OTHER;
				break;
		}
		g_variant_unref(deviceinfo);
	} else {
		device->locktype = MMGUI_LOCK_TYPE_OTHER;
	}
	
	/*Internal Modem Manager identifier*/
	deviceinfo = g_dbus_proxy_get_cached_property(deviceproxy, "DeviceIdentifier");
	if (deviceinfo != NULL) {
		strsize = 256;
		device->internalid = g_strdup(g_variant_get_string(deviceinfo, &strsize));
		g_variant_unref(deviceinfo);
	} else {
		device->internalid = NULL;
	}
	
	/*Persistent device identifier*/
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
	module->priority = MMGUI_MODULE_PRIORITY_NORMAL;
	module->identifier = MMGUI_MODULE_IDENTIFIER;
	module->functions = MMGUI_MODULE_FUNCTION_BASIC | MMGUI_MODULE_FUNCTION_POLKIT_PROTECTION;
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
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_free(mmguicorelc->moduledata);
		return FALSE;
	}
	
	error = NULL;
	
	(*moduledata)->objectmanager =  g_dbus_object_manager_client_new_sync((*moduledata)->connection,
																			G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
																			"org.freedesktop.ModemManager1",
																			"/org/freedesktop/ModemManager1",
																			NULL,
																			NULL,
																			NULL,
																			NULL,
																			&error);
	
	g_signal_connect(G_OBJECT((*moduledata)->objectmanager), "object-added", G_CALLBACK(mmgui_objectmanager_added_signal_handler), mmguicore);
	g_signal_connect(G_OBJECT((*moduledata)->objectmanager), "object-removed", G_CALLBACK(mmgui_objectmanager_removed_signal_handler), mmguicore);
	
	if (((*moduledata)->objectmanager == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_object_unref((*moduledata)->connection);
		g_free(mmguicorelc->moduledata);
		return FALSE;
	}
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
	/*GError *error;*/
	
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
		
		if (moduledata->objectmanager != NULL) {
			g_object_unref(moduledata->objectmanager);
			moduledata->objectmanager = NULL;
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
	guint devnum;
	GList *objects, *object;
	const gchar *devpath;
	
	if ((mmguicore == NULL) || (devicelist == NULL)) return 0;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return 0;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	devnum = 0;
		
	objects = g_dbus_object_manager_get_objects(moduledata->objectmanager);
	for (object = objects; object != NULL; object = object->next) {
		devpath = g_dbus_object_get_object_path(G_DBUS_OBJECT(object->data));
		g_debug("Device object path: %s\n", devpath);
		*devicelist = g_slist_prepend(*devicelist, mmgui_module_device_new(mmguicore, devpath));
		devnum++;
	}
	g_list_foreach(objects, (GFunc)g_object_unref, NULL);
	g_list_free(objects);
	
	return devnum;
}

G_MODULE_EXPORT gboolean mmgui_module_devices_state(gpointer mmguicore, enum _mmgui_device_state_request request)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	mmguidevice_t device;
	GVariant *data;
	gint statevalue;
	gboolean res;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	device = mmguicorelc->device;
	
	if (moduledata->modemproxy == NULL) return FALSE;
	
	data = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "State");
	if (data != NULL) {
		statevalue = g_variant_get_int32(data);
		g_variant_unref(data);
	} else {
		return FALSE;
	}
	
	switch (request) {
		case MMGUI_DEVICE_STATE_REQUEST_ENABLED:
			/*Is device enabled*/
			res = mmgui_module_device_enabled_from_state(statevalue);
			if (device->operation != MMGUI_DEVICE_OPERATION_ENABLE) {
				device->enabled = res;
			}
			break;
		case MMGUI_DEVICE_STATE_REQUEST_LOCKED:
			/*Is device blocked*/
			res = mmgui_module_device_locked_from_state(statevalue);
			/*Update lock type*/
			data = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "UnlockRequired");
			if (data != NULL) {
				switch (g_variant_get_uint32(data)) {
					case MODULE_INT_MODEM_LOCK_NONE:
						device->locktype = MMGUI_LOCK_TYPE_NONE;
						break;
					case MODULE_INT_MODEM_LOCK_SIM_PIN:
						device->locktype = MMGUI_LOCK_TYPE_PIN;
						break;
					case MODULE_INT_MODEM_LOCK_SIM_PUK:
						device->locktype = MMGUI_LOCK_TYPE_PUK;
						break;
					default:
						device->locktype = MMGUI_LOCK_TYPE_OTHER;
						break;
				}
				g_variant_unref(data);
			} else {
				device->locktype = MMGUI_LOCK_TYPE_OTHER;
			}
			device->blocked = res;
			break;
		case MMGUI_DEVICE_STATE_REQUEST_REGISTERED:
			/*Is device registered in network*/
			res = mmgui_module_device_registered_from_state(statevalue);
			device->registered = res;
			break;
		case MMGUI_DEVICE_STATE_REQUEST_CONNECTED:
			/*Is device connected (modem manager state)*/
			res = mmgui_module_device_connected_from_state(statevalue);
			break;
		case MMGUI_DEVICE_STATE_REQUEST_PREPARED:
			/*Is device not ready for opearation*/
			res = mmgui_module_device_prepared_from_state(statevalue);
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
	gint messageid; 
	GList *pslnode;
	GList *pslnext;
	gchar *pslpath;
		
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	device = mmguicorelc->device;
	
	if (device->enabled) {
		//Search for completed messages in partial sms list
		if ((moduledata->partialsms != NULL) && (mmguicorelc->eventcb != NULL)) {
			pslnode = moduledata->partialsms;
			while (pslnode != NULL) {
				pslpath = (gchar *)pslnode->data;
				pslnext = g_list_next(pslnode);
				//If messageid is -1 then it is incomplete
				messageid = mmgui_module_sms_get_id(mmguicore, pslpath);
				if (messageid != -1) {
					//Free resources
					g_free(pslpath);
					//Remove list node
					moduledata->partialsms = g_list_delete_link(moduledata->partialsms, pslnode);
					//Send notification
					(mmguicorelc->eventcb)(MMGUI_EVENT_SMS_COMPLETED, mmguicore, GUINT_TO_POINTER((guint)messageid));
				}
				pslnode = pslnext;
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
	gboolean enabledsignal, blockedsignal, regsignal, prepsignal;
	gsize strsize;
	GVariant *data;
		
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	device = mmguicorelc->device;
	
	/*Upadate state flags*/
	if (device->operation != MMGUI_DEVICE_OPERATION_ENABLE) {
		device->enabled = mmgui_module_device_enabled_from_state(newstate);
	}
	/*Handle new state*/
	device->blocked = mmgui_module_device_locked_from_state(newstate);
	device->registered = mmgui_module_device_registered_from_state(newstate);
	device->prepared = mmgui_module_device_prepared_from_state(newstate);
	/*Is enabled signal needed */
	enabledsignal = (mmgui_module_device_enabled_from_state(oldstate) != device->enabled);
	/*Is blocked signal needed */
	blockedsignal = (mmgui_module_device_locked_from_state(oldstate) != device->blocked);
	/*Is registered signal needed */
	regsignal = (mmgui_module_device_registered_from_state(oldstate) != device->registered);
	/*Is prepared signal needed*/
	prepsignal = (mmgui_module_device_prepared_from_state(oldstate) != device->prepared);
		
	/*Return if no signals will be sent*/
	if ((!enabledsignal) && (!blockedsignal) && (!regsignal) && (!prepsignal)) return TRUE;
	
	/*Handle device registration*/
	if ((regsignal) && (device->registered)) {
		if (moduledata->netproxy != NULL) {
			if (device->type == MMGUI_DEVICE_TYPE_GSM) {
				/*Operator information*/
				/*Registration state*/
				data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "RegistrationState");
				if (data != NULL) {
					device->regstatus = mmgui_module_registration_status_translate(g_variant_get_uint32(data));
					g_variant_unref(data);
				} else {
					device->regstatus = MMGUI_REG_STATUS_UNKNOWN;
				}
				/*Operator code*/
				data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "OperatorCode");
				if (data != NULL) {
					strsize = 256;
					device->operatorcode = mmgui_module_gsm_operator_code(g_variant_get_string(data, &strsize));
					g_variant_unref(data);
				} else {
					device->operatorcode = 0;
				}
				/*Operator name*/
				if (device->operatorname != NULL) {
					g_free(device->operatorname);
					device->operatorname = NULL;
				}
				data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "OperatorName");
				if (data != NULL) {
					strsize = 256;
					device->operatorname = g_strdup(g_variant_get_string(data, &strsize));
					g_variant_unref(data);
				} else {
					device->operatorname = NULL;
				}
			} else if (device->type == MMGUI_DEVICE_TYPE_CDMA) {
				/*Operator information*/
				/*Registration state*/
				data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "Cdma1xRegistrationState");
				if (data != NULL) {
					device->regstatus = mmgui_module_cdma_registration_status_translate(g_variant_get_uint32(data));
					g_variant_unref(data);
				} else {
					data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "EvdoRegistrationState");
					if (data != NULL) {
						device->regstatus = mmgui_module_cdma_registration_status_translate(g_variant_get_uint32(data));
						g_variant_unref(data);
					} else {
						device->regstatus = MMGUI_REG_STATUS_UNKNOWN;
					}
				}
				/*Operator code*/
				data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "Sid");
				if (data != NULL) {
					device->operatorcode = g_variant_get_uint32(data);
					g_variant_unref(data);
				} else {
					device->operatorcode = 0;
				}
				/*Operator name - no in CDMA*/
				if (device->operatorname != NULL) {
					g_free(device->operatorname);
					device->operatorname = NULL;
				}
				/*Device identifier (ESN)*/
				if (device->imei != NULL) {
					g_free(device->imei);
					device->imei = NULL;
				}
				data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "Esn");
				if (data != NULL) {
					strsize = 256;
					device->imei = g_strdup(g_variant_get_string(data, &strsize));
					g_variant_unref(data);
				} else {
					device->imei = NULL;
				}
			}
		}
	}
	
	/*Handle device enablement*/
	if ((enabledsignal) && (device->enabled)) {
		if (moduledata->modemproxy != NULL) {
			/*Device identifier (IMEI)*/
			if (device->imei != NULL) {
				g_free(device->imei);
				device->imei = NULL;
			}
			data = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "EquipmentIdentifier");
			if (data != NULL) {
				strsize = 256;
				device->imei = g_strdup(g_variant_get_string(data, &strsize));
				g_variant_unref(data);
			} else {
				device->imei = NULL;
			}
		}
		
		if (moduledata->cardproxy != NULL) {
			if (device->type == MMGUI_DEVICE_TYPE_GSM) {
				/*IMSI*/
				if (device->imsi != NULL) {
					g_free(device->imsi);
					device->imsi = NULL;
				}
				data = g_dbus_proxy_get_cached_property(moduledata->cardproxy, "Imsi");
				if (data != NULL) {
					strsize = 256;
					device->imsi = g_strdup(g_variant_get_string(data, &strsize));
					g_variant_unref(data);
				} else {
					device->imsi = NULL;
				}
			} else if (device->type == MMGUI_DEVICE_TYPE_CDMA) {
				/*No IMSI in CDMA*/
				if (device->imsi != NULL) {
					g_free(device->imsi);
					device->imsi = NULL;
				}
			}
		}
		
		if (moduledata->locationproxy != NULL) {
			if (moduledata->locationenabled) {
				/*Update location*/
				if (mmgui_module_devices_update_location(mmguicorelc, device)) {
					if (mmguicorelc->eventcb != NULL) {
						(mmguicorelc->eventcb)(MMGUI_EVENT_LOCATION_CHANGE, mmguicorelc, device);
					}
				}
			} else {
				/*Enable location API*/
				if (mmgui_module_devices_enable_location(mmguicorelc, mmguicorelc->device, TRUE)) {
					/*Update location*/
					if (mmgui_module_devices_update_location(mmguicorelc, device)) {
						if (mmguicorelc->eventcb != NULL) {
							(mmguicorelc->eventcb)(MMGUI_EVENT_LOCATION_CHANGE, mmguicorelc, device);
						}
					}
					/*Set signal handler*/
					moduledata->locationpropsignal = g_signal_connect(moduledata->locationproxy, "g-properties-changed", G_CALLBACK(mmgui_property_change_handler), mmguicorelc);
					/*Set enabled*/
					moduledata->locationenabled = TRUE;
				}
			}
			
		}
	}
	
	/*Is prepared signal needed */
	if (prepsignal) {
		if (mmguicorelc->eventcb != NULL) {
			(mmguicorelc->eventcb)(MMGUI_EVENT_DEVICE_PREPARED_STATUS, mmguicorelc, GUINT_TO_POINTER(device->prepared));
		}
	}
	/*Enabled signal */
	if (enabledsignal) {
		if (mmguicorelc->eventcb != NULL) {
			if (device->operation != MMGUI_DEVICE_OPERATION_ENABLE) {
				(mmguicorelc->eventcb)(MMGUI_EVENT_DEVICE_ENABLED_STATUS, mmguicorelc, GUINT_TO_POINTER(device->enabled));
			} else {
				if (mmguicorelc->device != NULL) {
					mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_IDLE;
				}
				if (mmguicorelc->eventcb != NULL) {
					(mmguicorelc->eventcb)(MMGUI_EVENT_MODEM_ENABLE_RESULT, mmguicorelc, GUINT_TO_POINTER(TRUE));
				}				
			}
		}
	}
	/*Is blocked signal needed */
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
	/*Is registered signal needed */
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
			if ((locationtype == MODULE_INT_MODEM_LOCATION_SOURCE_3GPP_LAC_CI) && (locationdata != NULL)) {
				/*3GPP location*/
				strlength = 256;
				locationstring = g_strdup(g_variant_get_string(locationdata, &strlength));
				device->loc3gppdata[0] = (guint)strtol(strsep(&locationstring, ","), NULL, 10);
				device->loc3gppdata[1] = (guint)strtol(strsep(&locationstring, ","), NULL, 10);
				device->loc3gppdata[2] = (guint)strtol(strsep(&locationstring, ","), NULL, 16);
				device->loc3gppdata[3] = (guint)strtol(strsep(&locationstring, ","), NULL, 16);
				g_free(locationstring);
				g_variant_unref(locationdata);
				g_debug("3GPP location: %u, %u, %4x, %4x\n", device->loc3gppdata[0], device->loc3gppdata[1], device->loc3gppdata[2], device->loc3gppdata[3]);
			} else if ((locationtype == MODULE_INT_MODEM_LOCATION_SOURCE_GPS_RAW) && (locationdata != NULL)) {
				/*GPS location*/
				locationdata = g_variant_lookup_value(data, "latitude", G_VARIANT_TYPE_STRING);
				if (locationdata != NULL) {
					strlength = 256;
					locationstring = (gchar *)g_variant_get_string(locationdata, &strlength);
					device->locgpsdata[0] = atof(locationstring);
					g_variant_unref(locationdata);
				} else {
					device->locgpsdata[0] = 0.0;
				}
				locationdata = g_variant_lookup_value(data, "longitude", G_VARIANT_TYPE_STRING);
				if (locationdata != NULL) {
					strlength = 256;
					locationstring = (gchar *)g_variant_get_string(locationdata, &strlength);
					device->locgpsdata[1] = atof(locationstring);
					g_variant_unref(locationdata);
				} else {
					device->locgpsdata[1] = 0.0;
				}
				locationdata = g_variant_lookup_value(data, "altitude", G_VARIANT_TYPE_STRING);
				if (locationdata != NULL) {
					strlength = 256;
					locationstring = (gchar *)g_variant_get_string(locationdata, &strlength);
					device->locgpsdata[2] = atof(locationstring);
					g_variant_unref(locationdata);
				} else {
					device->locgpsdata[2] = 0.0;
				}
				locationdata = g_variant_lookup_value(data, "utc-time", G_VARIANT_TYPE_STRING);
				if (locationdata != NULL) {
					strlength = 256;
					locationstring = (gchar *)g_variant_get_string(locationdata, &strlength);
					device->locgpsdata[3] = atof(locationstring);
					g_variant_unref(locationdata);
				} else {
					device->locgpsdata[3] = 0.0;
				}
				g_debug("GPS location: %2.3f, %2.3f, %2.3f, %6.0f", device->locgpsdata[0], device->locgpsdata[1], device->locgpsdata[2], device->locgpsdata[3]);
			}
			g_variant_unref(locationdata);
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
	guint locationtypes, enabledtypes;
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
		/*Determine supported capabilities and turn on location engine*/
		properties = g_dbus_proxy_get_cached_property(moduledata->locationproxy, "Capabilities");
		if (properties != NULL) {
			locationtypes = g_variant_get_uint32(properties);
			if ((locationtypes & MODULE_INT_MODEM_LOCATION_SOURCE_3GPP_LAC_CI) || (locationtypes & MODULE_INT_MODEM_LOCATION_SOURCE_GPS_RAW)) {
				error = NULL;
				/*Enable only needed capabilities*/
				enabledtypes = ((locationtypes & MODULE_INT_MODEM_LOCATION_SOURCE_3GPP_LAC_CI) | (locationtypes & MODULE_INT_MODEM_LOCATION_SOURCE_GPS_RAW));
				/*Apply new settings*/
				g_dbus_proxy_call_sync(moduledata->locationproxy, "Setup", g_variant_new("(ub)", enabledtypes, TRUE), 0, -1, NULL, &error);
				/*Set enabled properties*/
				if (error == NULL) {
					/*3GPP location*/
					if (locationtypes & MODULE_INT_MODEM_LOCATION_SOURCE_3GPP_LAC_CI) {
						device->locationcaps |= MMGUI_LOCATION_CAPS_3GPP;
					}
					/*GPS location*/
					if (locationtypes & MODULE_INT_MODEM_LOCATION_SOURCE_GPS_RAW) {
						device->locationcaps |= MMGUI_LOCATION_CAPS_GPS;
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
		g_dbus_proxy_call_sync(moduledata->locationproxy, "Setup", g_variant_new("(ub)", MODULE_INT_MODEM_LOCATION_SOURCE_NONE, FALSE), 0, -1, NULL, &error);
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
	gsize strsize = 256;
	gint statevalue;
	gboolean stateflag;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	device = mmguicorelc->device;
	
	if (moduledata->modemproxy != NULL) {
		/*Is device enabled and blocked*/
		data = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "State");
		if (data != NULL) {
			statevalue = g_variant_get_int32(data);
			device->enabled = mmgui_module_device_enabled_from_state(statevalue);
			device->blocked = mmgui_module_device_locked_from_state(statevalue);
			device->registered = mmgui_module_device_registered_from_state(statevalue);
			g_variant_unref(data);
		} else {
			device->enabled = FALSE;
			device->blocked = TRUE;
		}
		
		/*Lock type*/
		data = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "UnlockRequired");
		if (data != NULL) {
			switch (g_variant_get_uint32(data)) {
				case MODULE_INT_MODEM_LOCK_NONE:
					device->locktype = MMGUI_LOCK_TYPE_NONE;
					break;
				case MODULE_INT_MODEM_LOCK_SIM_PIN:
					device->locktype = MMGUI_LOCK_TYPE_PIN;
					break;
				case MODULE_INT_MODEM_LOCK_SIM_PUK:
					device->locktype = MMGUI_LOCK_TYPE_PUK;
					break;
				default:
					device->locktype = MMGUI_LOCK_TYPE_OTHER;
					break;
			}
			g_variant_unref(data);
		} else {
			device->locktype = MMGUI_LOCK_TYPE_OTHER;
		}
		
		if (device->enabled) {
			/*Device identifier (IMEI)*/
			if (device->imei != NULL) {
				g_free(device->imei);
				device->imei = NULL;
			}
			data = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "EquipmentIdentifier");
			if (data != NULL) {
				strsize = 256;
				device->imei = g_strdup(g_variant_get_string(data, &strsize));
				g_variant_unref(data);
			} else {
				device->imei = NULL;
			}
			/*Signal level*/
			data = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "SignalQuality");
			if (data != NULL) {
				g_variant_get(data, "(ub)", &statevalue, &stateflag);
				device->siglevel = statevalue;
				g_variant_unref(data);
			} else {
				device->siglevel = 0;
			}
			/*Used access technology*/
			data = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "AccessTechnologies");
			if (data != NULL) {
				device->mode = mmgui_module_access_mode_translate(g_variant_get_uint32(data));
				g_variant_unref(data);
			} else {
				device->mode = MMGUI_DEVICE_MODE_UNKNOWN;
			}
		}
	}
	
	if (moduledata->netproxy != NULL) {
		if (device->type == MMGUI_DEVICE_TYPE_GSM) {
			/*Operator information*/
			/*Registration state*/
			data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "RegistrationState");
			if (data != NULL) {
				device->regstatus = mmgui_module_registration_status_translate(g_variant_get_uint32(data));
				g_variant_unref(data);
			} else {
				device->regstatus = MMGUI_REG_STATUS_UNKNOWN;
			}
			/*Operator code*/
			data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "OperatorCode");
			if (data != NULL) {
				strsize = 256;
				device->operatorcode = mmgui_module_gsm_operator_code(g_variant_get_string(data, &strsize));
				g_variant_unref(data);
			} else {
				device->operatorcode = 0;
			}
			/*Operator name*/
			if (device->operatorname != NULL) {
				g_free(device->operatorname);
				device->operatorname = NULL;
			}
			data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "OperatorName");
			if (data != NULL) {
				strsize = 256;
				device->operatorname = g_strdup(g_variant_get_string(data, &strsize));
				g_variant_unref(data);
			} else {
				device->operatorname = NULL;
			}
		} else if (device->type == MMGUI_DEVICE_TYPE_CDMA) {
			/*Operator information*/
			/*Registration state*/
			data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "Cdma1xRegistrationState");
			if (data != NULL) {
				device->regstatus = mmgui_module_cdma_registration_status_translate(g_variant_get_uint32(data));
				g_variant_unref(data);
			} else {
				data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "EvdoRegistrationState");
				if (data != NULL) {
					device->regstatus = mmgui_module_cdma_registration_status_translate(g_variant_get_uint32(data));
					g_variant_unref(data);
				} else {
					device->regstatus = MMGUI_REG_STATUS_UNKNOWN;
				}
			}
			/*Operator code*/
			data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "Sid");
			if (data != NULL) {
				device->operatorcode = g_variant_get_uint32(data);
				g_variant_unref(data);
			} else {
				device->operatorcode = 0;
			}
			/*Operator name - no in CDMA*/
			if (device->operatorname != NULL) {
				g_free(device->operatorname);
				device->operatorname = NULL;
			}
			/*Device identifier (ESN)*/
			if (device->imei != NULL) {
				g_free(device->imei);
				device->imei = NULL;
			}
			data = g_dbus_proxy_get_cached_property(moduledata->netproxy, "Esn");
			if (data != NULL) {
				strsize = 256;
				device->imei = g_strdup(g_variant_get_string(data, &strsize));
				g_variant_unref(data);
			} else {
				device->imei = NULL;
			}
		}
	}
	
	if (moduledata->cardproxy != NULL) {
		if (device->type == MMGUI_DEVICE_TYPE_GSM) {
			if (device->enabled) {
				/*IMSI*/
				if (device->imsi != NULL) {
					g_free(device->imsi);
					device->imsi = NULL;
				}
				data = g_dbus_proxy_get_cached_property(moduledata->cardproxy, "Imsi");
				if (data != NULL) {
					strsize = 256;
					device->imsi = g_strdup(g_variant_get_string(data, &strsize));
					g_variant_unref(data);
				} else {
					device->imsi = NULL;
				}
			}
		} else if (device->type == MMGUI_DEVICE_TYPE_CDMA) {
			/*No IMSI in CDMA*/
			if (device->imsi != NULL) {
				g_free(device->imsi);
				device->imsi = NULL;
			}
		}
	}
	
	/*Update location*/
	if (moduledata->locationenabled) {
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
	const gchar *simpath;
	GVariant *simdata;
	GError *error;
	gsize strlength;
	GHashTable *interfaces;
			
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (device == NULL) return FALSE;
	if (device->objectpath == NULL) return FALSE;
	
	error = NULL;
	
	if (device->type == MMGUI_DEVICE_TYPE_GSM) {
		moduledata->netproxy = g_dbus_proxy_new_sync(moduledata->connection,
													G_DBUS_PROXY_FLAGS_NONE,
													NULL,
													"org.freedesktop.ModemManager1",
													device->objectpath,
													"org.freedesktop.ModemManager1.Modem.Modem3gpp",
													NULL,
													&error);
		
		if ((moduledata->netproxy == NULL) && (error != NULL)) {
			device->scancaps = MMGUI_SCAN_CAPS_NONE;
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
		} else {
			device->scancaps = MMGUI_SCAN_CAPS_OBSERVE;
			moduledata->netpropsignal = g_signal_connect(moduledata->netproxy, "g-properties-changed", G_CALLBACK(mmgui_property_change_handler), mmguicore);
		}
	} else if (device->type == MMGUI_DEVICE_TYPE_CDMA) {
		moduledata->netproxy = g_dbus_proxy_new_sync(moduledata->connection,
													G_DBUS_PROXY_FLAGS_NONE,
													NULL,
													"org.freedesktop.ModemManager1",
													device->objectpath,
													"org.freedesktop.ModemManager1.Modem.ModemCdma",
													NULL,
													&error);
		
		if ((moduledata->netproxy == NULL) && (error != NULL)) {
			device->scancaps = MMGUI_SCAN_CAPS_NONE;
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
		} else {
			device->scancaps = MMGUI_SCAN_CAPS_NONE;
			moduledata->netpropsignal = g_signal_connect(moduledata->netproxy, "g-properties-changed", G_CALLBACK(mmgui_property_change_handler), mmguicore);
		}
	}
	
	error = NULL;
	
	moduledata->modemproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.freedesktop.ModemManager1",
												device->objectpath,
												"org.freedesktop.ModemManager1.Modem",
												NULL,
												&error);
	
	if ((moduledata->modemproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
	} else {
		moduledata->statesignal = g_signal_connect(moduledata->modemproxy, "g-signal", G_CALLBACK(mmgui_signal_handler), mmguicore);
		moduledata->modempropsignal = g_signal_connect(moduledata->modemproxy, "g-properties-changed", G_CALLBACK(mmgui_property_change_handler), mmguicore);
		//Get path for SIM object
		simdata = g_dbus_proxy_get_cached_property(moduledata->modemproxy, "Sim");
		strlength = 256;
		simpath = g_variant_get_string(simdata, &strlength);
		//If SIM object exists
		if (simpath != NULL) {
			error = NULL;
			moduledata->cardproxy = g_dbus_proxy_new_sync(moduledata->connection,
															G_DBUS_PROXY_FLAGS_NONE,
															NULL,
															"org.freedesktop.ModemManager1",
															simpath,
															"org.freedesktop.ModemManager1.Sim",
															NULL,
															&error);
			
			if ((moduledata->cardproxy == NULL) && (error != NULL)) {
				mmgui_module_handle_error_message(mmguicorelc, error);
				g_error_free(error);
			}
		} else {
			moduledata->cardproxy = NULL;
		}
		g_variant_unref(simdata);
	}
	
	error = NULL;
	
	moduledata->smsproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.freedesktop.ModemManager1",
												device->objectpath,
												"org.freedesktop.ModemManager1.Modem.Messaging",
												NULL,
												&error);
	
	if ((moduledata->smsproxy == NULL) && (error != NULL)) {
		device->smscaps = MMGUI_SMS_CAPS_NONE;
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
	} else {
		device->smscaps = MMGUI_SMS_CAPS_RECEIVE | MMGUI_SMS_CAPS_SEND;
		moduledata->smssignal = g_signal_connect(moduledata->smsproxy, "g-signal", G_CALLBACK(mmgui_signal_handler), mmguicore);
	}
	
	error = NULL;
	
	if (device->type == MMGUI_DEVICE_TYPE_GSM) {
		moduledata->ussdproxy = g_dbus_proxy_new_sync(moduledata->connection,
													G_DBUS_PROXY_FLAGS_NONE,
													NULL,
													"org.freedesktop.ModemManager1",
													device->objectpath,
													"org.freedesktop.ModemManager1.Modem.Modem3gpp.Ussd",
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
	
	error = NULL;
	
	moduledata->locationproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.freedesktop.ModemManager1",
												device->objectpath,
												"org.freedesktop.ModemManager1.Modem.Location",
												NULL,
												&error);
	
	if ((moduledata->locationproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
	} else {
		moduledata->locationenabled = mmgui_module_devices_enable_location(mmguicore, device, TRUE);
		if (moduledata->locationenabled) {
			moduledata->locationpropsignal = g_signal_connect(moduledata->locationproxy, "g-properties-changed", G_CALLBACK(mmgui_property_change_handler), mmguicore);
		}
	}
	
	/*Supplimentary interfaces*/
	interfaces = mmgui_dbus_utils_list_service_interfaces(moduledata->connection, "org.freedesktop.ModemManager1", device->objectpath);
	
	if ((interfaces != NULL) && (g_hash_table_contains(interfaces, "org.freedesktop.ModemManager1.Modem.Time"))) {
		error = NULL;
		moduledata->timeproxy = g_dbus_proxy_new_sync(moduledata->connection,
													G_DBUS_PROXY_FLAGS_NONE,
													NULL,
													"org.freedesktop.ModemManager1",
													device->objectpath,
													"org.freedesktop.ModemManager1.Modem.Time",
													NULL,
													&error);
		
		if ((moduledata->timeproxy == NULL) && (error != NULL)) {
			moduledata->timesignal = 0;
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
		} else {
			moduledata->timesignal = g_signal_connect(moduledata->timeproxy, "g-signal", G_CALLBACK(mmgui_signal_handler), mmguicore);
		}
	} else {
		/*No Time interface*/
		moduledata->timeproxy = NULL;
		moduledata->timesignal = 0;
	}
	
	if ((interfaces != NULL) && (g_hash_table_contains(interfaces, "org.freedesktop.ModemManager1.Modem.Contacts"))) {
		error = NULL;
		moduledata->contactsproxy = g_dbus_proxy_new_sync(moduledata->connection,
													G_DBUS_PROXY_FLAGS_NONE,
													NULL,
													"org.freedesktop.ModemManager1",
													device->objectpath,
													"org.freedesktop.ModemManager1.Modem.Contacts",
													NULL,
													&error);
		
		if ((moduledata->contactsproxy == NULL) && (error != NULL)) {
			device->contactscaps = MMGUI_CONTACTS_CAPS_NONE;
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
		} else {
			device->contactscaps = MMGUI_CONTACTS_CAPS_EXPORT | MMGUI_CONTACTS_CAPS_EDIT | MMGUI_CONTACTS_CAPS_EXTENDED;			
		}
	} else {
		/*No Time interface*/
		moduledata->contactsproxy = NULL;
		device->contactscaps = MMGUI_CONTACTS_CAPS_NONE;
	}
	
	if (interfaces != NULL) {
		g_hash_table_destroy(interfaces);
	}
		
	//Update device information using created proxy objects
	mmgui_module_devices_information(mmguicore);
	
	//Add fresh partial sms list
	moduledata->partialsms = NULL;
	
	//Initialize SMS database
	
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_devices_close(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GList *pslnode;
	GList *pslnext;
	gchar *pslpath;
				
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	//Close SMS database
	//Free resources
	//Change device pointer
	
	//Free partial sms list
	if (moduledata->partialsms != NULL) {
		pslnode = moduledata->partialsms;
		while (pslnode != NULL) {
			pslpath = (gchar *)pslnode->data;
			pslnext = g_list_next(pslnode);
			if (pslpath) {
				g_free(pslpath);
			}
			pslnode = pslnext;
		}
		g_list_free(moduledata->partialsms);
		moduledata->partialsms = NULL;
	}
	
	if (moduledata->cardproxy != NULL) {
		g_object_unref(moduledata->cardproxy);
		moduledata->cardproxy = NULL;
	}
	if (moduledata->netproxy != NULL) {
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
		if (g_signal_handler_is_connected(moduledata->modemproxy, moduledata->modempropsignal)) {
			g_signal_handler_disconnect(moduledata->modemproxy, moduledata->modempropsignal);
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
	if (moduledata->ussdproxy != NULL) {
		g_object_unref(moduledata->ussdproxy);
		moduledata->ussdproxy = NULL;
	}
	if (moduledata->locationproxy != NULL) {
		if (g_signal_handler_is_connected(moduledata->locationproxy, moduledata->locationpropsignal)) {
			g_signal_handler_disconnect(moduledata->locationproxy, moduledata->locationpropsignal);
		}
		g_object_unref(moduledata->locationproxy);
		moduledata->locationproxy = NULL;
	}
	if (moduledata->timeproxy != NULL) {
		if (g_signal_handler_is_connected(moduledata->timeproxy, moduledata->timesignal)) {
			g_signal_handler_disconnect(moduledata->timeproxy, moduledata->timesignal);
		}
		g_object_unref(moduledata->timeproxy);
		moduledata->timeproxy = NULL;
	}
	if (moduledata->contactsproxy != NULL) {
		g_object_unref(moduledata->contactsproxy);
		moduledata->contactsproxy = NULL;
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
	
	error = NULL;
	
	moduledata->ussdproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.freedesktop.ModemManager1",
												device->objectpath,
												"org.freedesktop.ModemManager1.Modem.Modem3gpp.Ussd",
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

static void mmgui_module_devices_enable_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data)
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
		g_variant_unref(data);
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
	
	/*Device already in requested state*/
	if (mmguicorelc->device->enabled == enabled) {
		mmgui_module_custom_error_message(mmguicorelc, _("Device already in requested state"));
		return FALSE;
	}
	
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
		if ((str[12] == '+') || (str[12] == '-')) {
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

static mmgui_sms_message_t mmgui_module_sms_retrieve(mmguicore_t mmguicore, const gchar *smspath/*, gboolean listpartial*/)
{
	moduledata_t moduledata;
	mmgui_sms_message_t message;
	GDBusProxy *smsproxy;
	GError *error;
	GVariant *value;
	gsize strlength;
	const gchar *valuestr;
	guint index, state;
	gboolean gottext;
	
	if ((mmguicore == NULL) || (smspath == NULL)) return NULL;
	if (mmguicore->moduledata == NULL) return NULL;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	
	error = NULL;
	
	smsproxy = g_dbus_proxy_new_sync(moduledata->connection,
									G_DBUS_PROXY_FLAGS_NONE,
									NULL,
									"org.freedesktop.ModemManager1",
									smspath,
									"org.freedesktop.ModemManager1.Sms",
									NULL,
									&error);
	
	if ((smsproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
		return NULL;
	}
	
	/*SMS message state*/
	value  = g_dbus_proxy_get_cached_property(smsproxy, "State");
	if (value != NULL) {
		state = g_variant_get_uint32(value);
		g_debug("STATE: %u\n", state);
		if (state != MODULE_INT_SMS_STATE_RECEIVED) {
			/*//Message is not fully received - skip it and add to list if needed
			if ((state == MODULE_INT_SMS_STATE_RECEIVING) && (listpartial)) {
				moduledata->partialsms = g_list_prepend(moduledata->partialsms, g_strdup(smspath));
			}*/
			g_variant_unref(value);
			g_object_unref(smsproxy);
			return NULL;
		}
		g_variant_unref(value);
	} else {
		/*Something strange with this message - skip it*/
		g_object_unref(smsproxy);
		return NULL;
	}
	
	/*SMS message type*/
	value  = g_dbus_proxy_get_cached_property(smsproxy, "PduType");
	if (value != NULL) {
		state = g_variant_get_uint32(value);
		g_debug("PDU: %u\n", state);
		if ((state == MODULE_INT_PDU_TYPE_UNKNOWN) || (state == MODULE_INT_PDU_TYPE_SUBMIT)) {
			/*Only delivered messages and status reports needed this moment - maybe remove other?*/
			g_variant_unref(value);
			g_object_unref(smsproxy);
			return NULL;
		}
		g_variant_unref(value);
	} else {
		/*Something strange with this message - skip it*/
		g_object_unref(smsproxy);
		return NULL;
	}
		
	message = mmgui_smsdb_message_create();
	
	/*Sender number*/
	value  = g_dbus_proxy_get_cached_property(smsproxy, "Number");
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		if ((valuestr != NULL) && (valuestr[0] != '\0')) {
			mmgui_smsdb_message_set_number(message, valuestr);
			g_debug("SMS number: %s\n", valuestr);
		} else {
			mmgui_smsdb_message_set_number(message, _("Unknown"));
		}
		g_variant_unref(value);
	} else {
		mmgui_smsdb_message_set_number(message, _("Unknown"));
	}
	
	/*Service center number*/
	value = g_dbus_proxy_get_cached_property(smsproxy, "SMSC");
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		if ((valuestr != NULL) && (valuestr[0] != '\0')) {
			mmgui_smsdb_message_set_service_number(message, valuestr);
			g_debug("SMS service number: %s\n", valuestr);
		} else {
			mmgui_smsdb_message_set_service_number(message, _("Unknown"));
		}
		g_variant_unref(value);
	} else {
		mmgui_smsdb_message_set_service_number(message, _("Unknown"));
	}
	
	/*Try to get decoded message text first*/
	gottext = FALSE;
	value = g_dbus_proxy_get_cached_property(smsproxy, "Text");
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
		value = g_dbus_proxy_get_cached_property(smsproxy, "Data");
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
	
	/*No valuable information at all, skip message*/
	if (!gottext) {
		mmgui_smsdb_message_free(message);
		return NULL;
	}
	
	/*Message timestamp*/
	value = g_dbus_proxy_get_cached_property(smsproxy, "Timestamp");
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		if ((valuestr != NULL) && (valuestr[0] != '\0')) {
			mmgui_smsdb_message_set_timestamp(message, mmgui_module_str_to_time(valuestr));
			g_debug("SMS timestamp: %s", ctime((const time_t *)&message->timestamp));
		}
		g_variant_unref(value);
	}
	
	/*Message index*/
	index = mmgui_module_get_object_path_index(smspath);
	mmgui_smsdb_message_set_identifier(message, index, FALSE);
	g_debug("SMS index: %u\n", index);
	
	return message;
}

static gint mmgui_module_sms_get_id(mmguicore_t mmguicore, const gchar *smspath)
{
	moduledata_t moduledata;
	GDBusProxy *smsproxy;
	GError *error;
	GVariant *value;
	gint state;
	
	if ((mmguicore == NULL) || (smspath == NULL)) return -1;
	if (mmguicore->moduledata == NULL) return -1;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	
	error = NULL;
	
	smsproxy = g_dbus_proxy_new_sync(moduledata->connection,
									G_DBUS_PROXY_FLAGS_NONE,
									NULL,
									"org.freedesktop.ModemManager1",
									smspath,
									"org.freedesktop.ModemManager1.Sms",
									NULL,
									&error);
	
	if ((smsproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
		return -1;
	}
	//SMS message state
	value  = g_dbus_proxy_get_cached_property(smsproxy, "State");
	if (value != NULL) {
		state = g_variant_get_uint32(value);
		if (state == MODULE_INT_SMS_STATE_RECEIVED) {
			g_variant_unref(value);
			g_object_unref(smsproxy);
			return mmgui_module_get_object_path_index(smspath);
		} else {
			g_variant_unref(value);
			g_object_unref(smsproxy);
			return -1;
		}
		g_variant_unref(value);
	} else {
		//Something strange with this message
		g_object_unref(smsproxy);
		return -1;
	}
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
	
	gsize strlength;
	const gchar *smspath;
	
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
			strlength = 256;
			smspath = g_variant_get_string(mnodel2, &strlength);
			g_debug("SMS message object path: %s\n", smspath);
			if ((smspath != NULL) && (smspath[0] != '\0')) {
				message = mmgui_module_sms_retrieve(mmguicorelc, smspath);
				if (message != NULL) {
					*smslist = g_slist_prepend(*smslist, message);
					msgnum++;
				}
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
	/*moduledata_t moduledata;*/
	gchar *smspath;
	mmgui_sms_message_t message;
	
	if (mmguicore == NULL) return NULL;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	/*if (mmguicorelc->moduledata == NULL) return NULL;
	moduledata = (moduledata_t)mmguicorelc->moduledata;*/
	
	if (mmguicorelc->device == NULL) return NULL;
	if (!mmguicorelc->device->enabled) return NULL;
	if (!(mmguicorelc->device->smscaps & MMGUI_SMS_CAPS_RECEIVE)) return NULL;
	
	smspath = g_strdup_printf("/org/freedesktop/ModemManager1/SMS/%u", index);
	
	message = mmgui_module_sms_retrieve(mmguicorelc, smspath);
	
	g_free(smspath);
	
	return message;
}

G_MODULE_EXPORT gboolean mmgui_module_sms_delete(gpointer mmguicore, guint index)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	gchar *smspath;
	GError *error;
				
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->smsproxy == NULL) return FALSE;
	if (mmguicorelc->device == NULL) return FALSE;
	if (!mmguicorelc->device->enabled) return FALSE;
	if (!(mmguicorelc->device->smscaps & MMGUI_SMS_CAPS_RECEIVE)) return FALSE;
	
	smspath = g_strdup_printf("/org/freedesktop/ModemManager1/SMS/%u", index);
	
	error = NULL;
	
	g_dbus_proxy_call_sync(moduledata->smsproxy,
							"Delete",
							g_variant_new("(o)", smspath),
							0,
							-1,
							NULL,
							&error);
	
	if (error != NULL) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_free(smspath);
		return FALSE;
	}
	
	g_free(smspath);
	
	return TRUE;
}

static void mmgui_module_sms_send_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	gboolean sent;
	const gchar *smspath;
		
	if (user_data == NULL) return;
	mmguicorelc = (mmguicore_t)user_data;
	
	if (mmguicorelc->moduledata == NULL) return;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	error = NULL;
	
	g_dbus_proxy_call_finish(proxy, res, &error);
	
	//Operation result
	if (error != NULL) {
		if ((moduledata->cancellable == NULL) || ((moduledata->cancellable != NULL) && (!g_cancellable_is_cancelled(moduledata->cancellable)))) {
			mmgui_module_handle_error_message(mmguicorelc, error);
		}
		g_error_free(error);
		sent = FALSE;
	} else {
		sent = TRUE;
	}
	
	smspath = g_dbus_proxy_get_object_path(proxy);
	if (smspath != NULL) {
		error = NULL;
		//Remove message from storage
		g_dbus_proxy_call_sync(moduledata->smsproxy,
								"Delete",
								g_variant_new("(o)", smspath),
								0,
								-1,
								NULL,
								&error);
		if (error != NULL) {
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
		}
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
	GVariant *array, *message, *smspathv;
	GError *error;
	/*gsize strlength;*/
	gchar *smspath;
	GDBusProxy *messageproxy;
	
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
		g_variant_builder_add_parsed(builder, "{'validity', %v}", g_variant_new("(uv)", MODULE_INT_SMS_VALIDITY_TYPE_RELATIVE, g_variant_new_uint32((guint)validity)));
	}
	g_variant_builder_add_parsed(builder, "{'delivery-report-request', <%b>}", report);
	array = g_variant_builder_end(builder);
	
	builder = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
	g_variant_builder_add_value(builder, array);
	message = g_variant_builder_end(builder);
	
	error = NULL;
	
	//Create new message
	smspathv = g_dbus_proxy_call_sync(moduledata->smsproxy,
									"Create",
									message,
									0,
									-1,
									NULL,
									&error);
	
	if ((smspathv == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	}
	
	//Created SMS object path
	g_variant_get(smspathv, "(o)", &smspath);
	if (smspath == NULL) {
		g_debug("Failed to obtain object path for saved SMS message\n");
		return FALSE;
	}
	
	error = NULL;
	
	//Create message proxy
	messageproxy = g_dbus_proxy_new_sync(moduledata->connection,
									G_DBUS_PROXY_FLAGS_NONE,
									NULL,
									"org.freedesktop.ModemManager1",
									smspath,
									"org.freedesktop.ModemManager1.Sms",
									NULL,
									&error);
	
	if ((messageproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_free(smspath);
		return FALSE;
	}
	
	g_free(smspath);
	
	mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_SEND_SMS;
	
	if (moduledata->cancellable != NULL) {
		g_cancellable_reset(moduledata->cancellable);
	}
	
	//Send message
	g_dbus_proxy_call(messageproxy,
					"Send",
					NULL,
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
	guint state;
	enum _mmgui_ussd_state stateid;
	
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
	
	state = g_variant_get_uint32(session);
	
	switch (state) {
		case MODULE_INT_MODEM_3GPP_USSD_SESSION_STATE_UNKNOWN:
			stateid = MMGUI_USSD_STATE_UNKNOWN;
			break;
		case MODULE_INT_MODEM_3GPP_USSD_SESSION_STATE_IDLE:
			stateid = MMGUI_USSD_STATE_IDLE;
			break;
		case MODULE_INT_MODEM_3GPP_USSD_SESSION_STATE_ACTIVE:
			stateid = MMGUI_USSD_STATE_ACTIVE;
			break;
		case MODULE_INT_MODEM_3GPP_USSD_SESSION_STATE_USER_RESPONSE:
			stateid = MMGUI_USSD_STATE_USER_RESPONSE;
			break;
		default:
			stateid = MMGUI_USSD_STATE_UNKNOWN;
			break;
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
	value  = g_variant_lookup_value(networkv, "operator-code", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		network->operator_num = atoi(valuestr);
		g_variant_unref(value);
	} else {
		network->operator_num = 0;
	}
	//Network access technology
	value = g_variant_lookup_value(networkv, "access-technology", G_VARIANT_TYPE_UINT32);
	if (value != NULL) {
		network->access_tech = mmgui_module_access_technology_translate(g_variant_get_uint32(value));
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
		network->operator_long = g_strdup(_("Unknown"));
	}
	//Short-format name of operator
	value = g_variant_lookup_value(networkv, "operator-short", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		network->operator_short = g_strdup(valuestr);
		g_variant_unref(value);
	} else {
		network->operator_short = g_strdup(_("Unknown"));
	}
	//Network availability status (this is a critical parameter, so entry will be skipped if value is unknown)
	value = g_variant_lookup_value(networkv, "status", G_VARIANT_TYPE_UINT32);
	if (value != NULL) {
		network->status = mmgui_module_network_availability_status_translate(g_variant_get_uint32(value));
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
	GVariantIter niterl1, niterl2/*, niterl3*/;
	GVariant *nnodel1, *nnodel2/*, *nnodel3*/;
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
	GVariant *value;
	gsize strlength;
	const gchar *valuestr;
		
	if (contactv == NULL) return NULL;
		
	contact = g_new0(struct _mmgui_contact, 1);
	//Full name of the contact
	value  = g_variant_lookup_value(contactv, "name", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		contact->name = g_strdup(valuestr);
		g_variant_unref(value);
	} else {
		contact->name = g_strdup(_("Unknown"));
	}
	//Telephone number
	value  = g_variant_lookup_value(contactv, "number", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		contact->number = g_strdup(valuestr);
		g_variant_unref(value);
	} else {
		contact->number = g_strdup(_("Unknown"));
	}
	//Email address
	value  = g_variant_lookup_value(contactv, "email", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		contact->email = g_strdup(valuestr);
		g_variant_unref(value);
	} else {
		contact->email = g_strdup(_("Unknown"));
	}
	//Group this contact belongs to
	value  = g_variant_lookup_value(contactv, "group", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		contact->group = g_strdup(valuestr);
		g_variant_unref(value);
	} else {
		contact->group = g_strdup(_("Unknown"));
	}
	//Additional contact name
	value  = g_variant_lookup_value(contactv, "name2", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		contact->name2 = g_strdup(valuestr);
		g_variant_unref(value);
	} else {
		contact->name2 = g_strdup(_("Unknown"));
	}
	//Additional contact telephone number
	value  = g_variant_lookup_value(contactv, "number2", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		contact->number2 = g_strdup(valuestr);
		g_variant_unref(value);
	} else {
		contact->number2 = g_strdup(_("Unknown"));
	}
	//Boolean flag to specify whether this entry is hidden or not
	value  = g_variant_lookup_value(contactv, "hidden", G_VARIANT_TYPE_BOOLEAN);
	if (value != NULL) {
		contact->hidden = g_variant_get_boolean(value);
		g_variant_unref(value);
	} else {
		contact->hidden = FALSE;
	}
	//Phonebook in which the contact is stored
	value  = g_variant_lookup_value(contactv, "storage", G_VARIANT_TYPE_UINT32);
	if (value != NULL) {
		contact->storage = g_variant_get_uint32(value);
		g_variant_unref(value);
	} else {
		contact->storage = FALSE;
	}
	//Internal private number (this is a critical parameter, so entry will be skipped if value is unknown)
	value = g_variant_lookup_value(contactv, "index", G_VARIANT_TYPE_UINT32);
	if (value != NULL) {
		contact->id = g_variant_get_uint32(value);
		g_variant_unref(value);
		return contact;
	} else {
		if (contact->name != NULL) g_free(contact->name);
		if (contact->number != NULL) g_free(contact->number);
		if (contact->email != NULL) g_free(contact->email);
		if (contact->group != NULL) g_free(contact->group);
		if (contact->name2 != NULL) g_free(contact->name2);
		if (contact->number2 != NULL) g_free(contact->number2);
		g_free(contact);
		return NULL;
	}
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
							g_variant_new("(i)", index),
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

G_MODULE_EXPORT gint mmgui_module_contacts_add(gpointer mmguicore, gchar* name, gchar *number)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GVariantBuilder *builder;
	GVariant *array, *contact;
	GError *error;
	GVariant *idv;
	guint id;
	
	if ((mmguicore == NULL) || (name == NULL) || (number == NULL)) return -1;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return -1;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->contactsproxy == NULL) return -1;
	if (mmguicorelc->device == NULL) return -1;
	if (!mmguicorelc->device->enabled) return -1;
	if (!(mmguicorelc->device->contactscaps & MMGUI_CONTACTS_CAPS_EDIT)) return -1;
	
	builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add_parsed(builder, "{'name', <%s>}", name);
	g_variant_builder_add_parsed(builder, "{'number', <%s>}", number);
	g_variant_builder_add_parsed(builder, "{'hidden', <%b>}", FALSE);
	array = g_variant_builder_end(builder);
	
	builder = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
	g_variant_builder_add_value(builder, array);
	contact = g_variant_builder_end(builder);
	
	error = NULL;
	
	idv = g_dbus_proxy_call_sync(moduledata->contactsproxy,
									"Add",
									contact,
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
	
	return id;
}
