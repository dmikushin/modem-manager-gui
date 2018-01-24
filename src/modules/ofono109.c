/*
 *      ofono109.c
 *      
 *      Copyright 2013-2017 Alex <alex@linuxonly.ru>
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

#include "historyshm.h"
#include "../vcard.h"

#include "../mmguicore.h"
#include "../smsdb.h"

#define MMGUI_MODULE_SERVICE_NAME  "org.ofono"
#define MMGUI_MODULE_SYSTEMD_NAME  "ofono.service"
#define MMGUI_MODULE_IDENTIFIER    109
#define MMGUI_MODULE_DESCRIPTION   "oFono >= 1.9"

#define MMGUI_MODULE_ENABLE_OPERATION_TIMEOUT           20000
#define MMGUI_MODULE_SEND_SMS_OPERATION_TIMEOUT         35000
#define MMGUI_MODULE_SEND_USSD_OPERATION_TIMEOUT        25000
#define MMGUI_MODULE_NETWORKS_SCAN_OPERATION_TIMEOUT    60000
#define MMGUI_MODULE_NETWORKS_UNLOCK_OPERATION_TIMEOUT  20000

//Location data bitmask
typedef enum {
	MODULE_INT_MODEM_LOCATION_NULL = 0x00,
    MODULE_INT_MODEM_LOCATION_MCC  = 0x01,
    MODULE_INT_MODEM_LOCATION_MNC  = 0x02,
    MODULE_INT_MODEM_LOCATION_LAC  = 0x04,
    MODULE_INT_MODEM_LOCATION_CID  = 0x08,
    MODULE_INT_MODEM_LOCATION_ALL  = 0x0f,
} ModuleIntModemLocationBitmask;

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
	GDBusProxy *contactsproxy;
	GDBusProxy *connectionproxy;
	//Attached signal handlers
	gulong netsignal;
	gulong netopsignal;
	gulong modemsignal;
	gulong cardsignal;
	gulong smssignal;
	gulong connectionsignal;
	gulong broadcastsignal;
	//Error message
	gchar *errormessage;
	//Pending power devices queue, sms messages queue
	GList *devqueue, *msgqueue;
	//Available location data
	gint location;
	//History storage
	mmgui_history_shm_client_t historyshm;
	//Cancellable
	GCancellable *cancellable;
	//Operations timeouts
	guint timeouts[MMGUI_DEVICE_OPERATIONS];
};

typedef struct _mmguimoduledata *moduledata_t;


static enum _mmgui_device_modes mmgui_module_access_mode_translate(const gchar *mode);
static enum _mmgui_reg_status mmgui_module_registration_status_translate(const gchar *status);
static gboolean mmgui_module_device_get_locked_from_unlock_string(const gchar *ustring);
static gint mmgui_module_device_get_lock_type_from_unlock_string(const gchar *ustring);
static GVariant *mmgui_module_proxy_get_property(GDBusProxy *proxy, const gchar *name, const GVariantType *type);
static gboolean mmgui_module_device_get_enabled(mmguicore_t mmguicore);
static mmguidevice_t mmgui_module_device_new(mmguicore_t mmguicore, const gchar *devpath, GVariant *devprops);
static gboolean mmgui_module_devices_queue_remove(mmguicore_t mmguicore, const gchar *devpath);
static mmgui_sms_message_t mmgui_module_sms_retrieve(mmguicore_t mmguicore, GVariant *smsdata);
gboolean mmgui_module_devices_information(gpointer mmguicore);
/*Dynamic interfaces*/
static gboolean mmgui_module_open_network_registration_interface(gpointer mmguicore, mmguidevice_t device);
static gboolean mmgui_module_open_cdma_network_registration_interface(gpointer mmguicore, mmguidevice_t device);
static gboolean mmgui_module_open_sim_manager_interface(gpointer mmguicore, mmguidevice_t device);
static gboolean mmgui_module_open_message_manager_interface(gpointer mmguicore, mmguidevice_t device);
static gboolean mmgui_module_open_cdma_message_manager_interface(gpointer mmguicore, mmguidevice_t device);
static gboolean mmgui_module_open_supplementary_services_interface(gpointer mmguicore, mmguidevice_t device);
static gboolean mmgui_module_open_phonebook_interface(gpointer mmguicore, mmguidevice_t device);
static gboolean mmgui_module_open_connection_manager_interface(gpointer mmguicore, mmguidevice_t device);
static gboolean mmgui_module_open_cdma_connection_manager_interface(gpointer mmguicore, mmguidevice_t device);

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
	guint pathlen;
	guint id;
	guint i;
	
	id = 0; i = 0;
	
	if (devpath == NULL) return id;
	
	pathlen = strlen(devpath);
	
	if (pathlen == 0) return id;
	
	/*SDBM Hash Function*/
	for (i=0; i<pathlen; i++) {
		id = devpath[i] + (id << 6) + (id << 16) - id;		
	}
	
	return id;
}

static void mmgui_module_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data)
{
	mmguicore_t mmguicore;
	moduledata_t moduledata;
	GVariant *devpathv;
	const gchar *devpath;
	gsize devpathsize;
	GVariant *devpropsv;
	GVariant *devtypev;
	const gchar *typestr;
	gsize typestrsize;
	gboolean devreal;
	guint id;
	
	mmguicore = (mmguicore_t)data;
	if (mmguicore == NULL) return;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	if (moduledata == NULL) return;
	
	if (mmguicore->eventcb != NULL) {
		if (g_str_equal(signal_name, "ModemAdded")) {
			devpathv = g_variant_get_child_value(parameters, 0);
			devpropsv = g_variant_get_child_value(parameters, 1);
			if ((devpathv != NULL) && (devpropsv != NULL)) {
				/*Determine if modem is not emulated*/
				devreal = FALSE;
				devtypev = g_variant_lookup_value(devpropsv, "Type", G_VARIANT_TYPE_STRING);
				if (devtypev != NULL) {
					typestrsize = 256;
					typestr = g_variant_get_string(devtypev, &typestrsize);
					if ((typestr != NULL) && (typestr[0] != '\0')) {
						if (g_str_equal(typestr, "hardware")) {
							devreal = TRUE;
						}
					}
					g_variant_unref(devtypev);
				}
				/*If modem is not emulated, work with it*/
				if (devreal) {
					/*Determine path and add device to queue*/
					devpathsize = 256;
					devpath = g_variant_get_string(devpathv, &devpathsize);
					if ((devpath != NULL) && (devpath[0] != '\0')) {
						moduledata->devqueue = g_list_prepend(moduledata->devqueue, g_strdup(devpath));
					}
				}
				g_variant_unref(devpathv);
				g_variant_unref(devpropsv);
			}
		} else if (g_str_equal(signal_name, "ModemRemoved")) {
			g_variant_get(parameters, "(o)", &devpath);
			if (devpath != NULL) {
				/*First we need to test devices queue*/
				if (!mmgui_module_devices_queue_remove(mmguicore, devpath)) {
					/*Then remove already opened device*/
					id = mmgui_module_device_id(devpath);
					(mmguicore->eventcb)(MMGUI_EVENT_DEVICE_REMOVED, mmguicore, GUINT_TO_POINTER(id));
				}
			}
		}
	}
	
	g_debug("SIGNAL: %s (%s) argtype: %s\n", signal_name, sender_name, g_variant_get_type_string(parameters));
}

static void mmgui_module_network_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data)
{
	mmguicore_t mmguicore;
	moduledata_t moduledata;
	GVariant *propname, *propvalue, *value;
	const gchar *parameter;
	gsize strsize;
	gint oldlocation;
	
	mmguicore = (mmguicore_t)data;
	if (mmguicore == NULL) return;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	if (moduledata == NULL) return;
	
	if (g_str_equal(signal_name, "PropertyChanged")) {
		/*Property name and value*/
		propname = g_variant_get_child_value(parameters, 0);
		propvalue = g_variant_get_child_value(parameters, 1);
		if ((propname != NULL) && (propvalue != NULL)) {
			/*Unboxed parameter and value*/
			strsize = 256;
			parameter = g_variant_get_string(propname, &strsize);
			value = g_variant_get_variant(propvalue);
			if ((parameter != NULL) && (parameter[0] != '\0') && (value != NULL)) {
				g_debug("SIGNAL: %s: %s\n", parameter, g_variant_print(value, TRUE));
				if (g_str_equal(parameter, "Strength")) {
					/*Signal level*/
					if (mmguicore->device != NULL) {
						mmguicore->device->siglevel = g_variant_get_byte(value);
						if (mmguicore->eventcb != NULL) {
							(mmguicore->eventcb)(MMGUI_EVENT_SIGNAL_LEVEL_CHANGE, mmguicore, mmguicore->device);
						}
					}
				} else if (g_str_equal(parameter, "Status")) {
					/*Registration state*/
					if (mmguicore->device != NULL) {
						strsize = 256;
						parameter = g_variant_get_string(value, &strsize);
						if ((parameter != NULL) && (parameter[0] != '\0')) {
							mmguicore->device->regstatus = mmgui_module_registration_status_translate(parameter);
							if (mmguicore->eventcb != NULL) {
								(mmguicore->eventcb)(MMGUI_EVENT_NETWORK_REGISTRATION_CHANGE, mmguicore, mmguicore->device);
							}
						}
					}
				} else if (g_str_equal(parameter, "MobileCountryCode")) {
					/*Registration state*/
					if (mmguicore->device != NULL) {
						strsize = 256;
						parameter = g_variant_get_string(value, &strsize);
						if ((parameter != NULL) && (parameter[0] != '\0')) {
							/*Operator code*/
							mmguicore->device->operatorcode |= (atoi(parameter) & 0x0000ffff) << 16;
							/*Location*/
							mmguicore->device->loc3gppdata[0] = atoi(parameter);
							oldlocation = moduledata->location;
							moduledata->location |= MODULE_INT_MODEM_LOCATION_MCC;
							if (moduledata->location == MODULE_INT_MODEM_LOCATION_ALL) {
								mmguicore->device->locationcaps |= MMGUI_LOCATION_CAPS_3GPP;
								/*Location capabilities updated*/
								if ((oldlocation != MODULE_INT_MODEM_LOCATION_ALL) && (mmguicore->eventcb != NULL)) {
									(mmguicore->eventcb)(MMGUI_EVENT_EXTEND_CAPABILITIES, mmguicore, GINT_TO_POINTER(MMGUI_CAPS_LOCATION));
								}
								/*Update location*/
								if (mmguicore->eventcb != NULL) {
									(mmguicore->eventcb)(MMGUI_EVENT_LOCATION_CHANGE, mmguicore, mmguicore->device);
								}
							
							}
							if ((moduledata->location & MODULE_INT_MODEM_LOCATION_MCC) && (moduledata->location & MODULE_INT_MODEM_LOCATION_MNC)) {
								/*Update operator code*/
								if (mmguicore->eventcb != NULL) {
									(mmguicore->eventcb)(MMGUI_EVENT_NETWORK_REGISTRATION_CHANGE, mmguicore, mmguicore->device);
								}
							}
						}
					}
				}  else if (g_str_equal(parameter, "MobileNetworkCode")) {
					/*Registration state*/
					if (mmguicore->device != NULL) {
						strsize = 256;
						parameter = g_variant_get_string(value, &strsize);
						if ((parameter != NULL) && (parameter[0] != '\0')) {
							/*Operator code*/
							mmguicore->device->operatorcode |= atoi(parameter) & 0x0000ffff;
							/*Location*/
							mmguicore->device->loc3gppdata[1] = atoi(parameter);
							oldlocation = moduledata->location;
							moduledata->location |= MODULE_INT_MODEM_LOCATION_MNC;
							if (moduledata->location == MODULE_INT_MODEM_LOCATION_ALL) {
								mmguicore->device->locationcaps |= MMGUI_LOCATION_CAPS_3GPP;
								/*Location capabilities updated*/
								if ((oldlocation != MODULE_INT_MODEM_LOCATION_ALL) && (mmguicore->eventcb != NULL)) {
									(mmguicore->eventcb)(MMGUI_EVENT_EXTEND_CAPABILITIES, mmguicore, GINT_TO_POINTER(MMGUI_CAPS_LOCATION));
								}
								/*Update location*/
								if (mmguicore->eventcb != NULL) {
									(mmguicore->eventcb)(MMGUI_EVENT_LOCATION_CHANGE, mmguicore, mmguicore->device);
								}
							}
							if ((moduledata->location & MODULE_INT_MODEM_LOCATION_MCC) && (moduledata->location & MODULE_INT_MODEM_LOCATION_MNC)) {
								/*Update operator code*/
								if (mmguicore->eventcb != NULL) {
									(mmguicore->eventcb)(MMGUI_EVENT_NETWORK_REGISTRATION_CHANGE, mmguicore, mmguicore->device);
								}
							}
						}
					}
				} else if (g_str_equal(parameter, "LocationAreaCode")) {
					/*Location*/
					if (mmguicore->device != NULL) {
						mmguicore->device->loc3gppdata[2] = g_variant_get_uint16(value);
						oldlocation = moduledata->location;
						moduledata->location |= MODULE_INT_MODEM_LOCATION_LAC;
						if (moduledata->location == MODULE_INT_MODEM_LOCATION_ALL) {
							mmguicore->device->locationcaps |= MMGUI_LOCATION_CAPS_3GPP;
							/*Location capabilities updated*/
							if ((oldlocation != MODULE_INT_MODEM_LOCATION_ALL) && (mmguicore->eventcb != NULL)) {
								(mmguicore->eventcb)(MMGUI_EVENT_EXTEND_CAPABILITIES, mmguicore, GINT_TO_POINTER(MMGUI_CAPS_LOCATION));
							}
							/*Update location*/
							if (mmguicore->eventcb != NULL) {
								(mmguicore->eventcb)(MMGUI_EVENT_LOCATION_CHANGE, mmguicore, mmguicore->device);
							}
						}
					}
				} else if (g_str_equal(parameter, "CellId")) {
					/*Location*/
					if (mmguicore->device != NULL) {
						mmguicore->device->loc3gppdata[3] = g_variant_get_uint32(value);
						oldlocation = moduledata->location;
						moduledata->location |= MODULE_INT_MODEM_LOCATION_CID;
						if (moduledata->location == MODULE_INT_MODEM_LOCATION_ALL) {
							mmguicore->device->locationcaps |= MMGUI_LOCATION_CAPS_3GPP;
							/*Location capabilities updated*/
							if ((oldlocation != MODULE_INT_MODEM_LOCATION_ALL) && (mmguicore->eventcb != NULL)) {
								(mmguicore->eventcb)(MMGUI_EVENT_EXTEND_CAPABILITIES, mmguicore, GINT_TO_POINTER(MMGUI_CAPS_LOCATION));
							}
							/*Update location*/
							if (mmguicore->eventcb != NULL) {
								(mmguicore->eventcb)(MMGUI_EVENT_LOCATION_CHANGE, mmguicore, mmguicore->device);
							}
						}
					}
				} else if (g_str_equal(parameter, "Name")) {
					/*Registration state*/
					if (mmguicore->device != NULL) {
						if (mmguicore->device->operatorname != NULL) {
							g_free(mmguicore->device->operatorname);
						}
						strsize = 256;
						parameter = g_variant_get_string(value, &strsize);
						if ((parameter != NULL) && (parameter[0] != '\0')) {
							mmguicore->device->operatorname = g_strdup(parameter);
							if (mmguicore->eventcb != NULL) {
								(mmguicore->eventcb)(MMGUI_EVENT_NETWORK_REGISTRATION_CHANGE, mmguicore, mmguicore->device);
							}
						}
					}
				} else if (g_str_equal(parameter, "Technology")) {
					/*Network mode*/
					if (mmguicore->device != NULL) {
						strsize = 256;
						parameter = g_variant_get_string(value, &strsize);
						if ((parameter != NULL) && (parameter[0] != '\0')) {
							mmguicore->device->mode = mmgui_module_access_mode_translate(parameter);
							if (mmguicore->eventcb != NULL) {
								(mmguicore->eventcb)(MMGUI_EVENT_NETWORK_MODE_CHANGE, mmguicore, mmguicore->device);
							}
						}
					}
				}
				g_variant_unref(value);
			}
		}
	}
}

static void mmgui_module_modem_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data)
{
	mmguicore_t mmguicore;
	moduledata_t moduledata;
	GVariant *propname, *propvalue, *value;
	const gchar *parameter;
	gsize strsize;
	GVariantIter iterl1;
	GVariant *nodel1;
	const gchar *interface;
		
	mmguicore = (mmguicore_t)data;
	if (mmguicore == NULL) return;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	if (moduledata == NULL) return;
	
	if (g_str_equal(signal_name, "PropertyChanged")) {
		/*Property name and value*/
		propname = g_variant_get_child_value(parameters, 0);
		propvalue = g_variant_get_child_value(parameters, 1);
		if ((propname != NULL) && (propvalue != NULL)) {
			/*Unboxed parameter and value*/
			strsize = 256;
			parameter = g_variant_get_string(propname, &strsize);
			value = g_variant_get_variant(propvalue);
			if ((parameter != NULL) && (parameter[0] != '\0') && (value != NULL)) {
				if (g_str_equal(parameter, "Interfaces")) {
					if (mmguicore->device != NULL) {
						g_variant_iter_init(&iterl1, value);
						while ((nodel1 = g_variant_iter_next_value(&iterl1)) != NULL) {
							strsize = 256;
							interface = g_variant_get_string(nodel1, &strsize);
							if ((interface != NULL) && (interface[0] != '\0')) {
								if ((moduledata->netproxy == NULL) && (g_str_equal(interface, "org.ofono.NetworkRegistration"))) {
									if (mmgui_module_open_network_registration_interface(mmguicore, mmguicore->device)) {
										/*Scan capabilities updated*/
										if (mmguicore->eventcb != NULL) {
											(mmguicore->eventcb)(MMGUI_EVENT_EXTEND_CAPABILITIES, mmguicore, GINT_TO_POINTER(MMGUI_CAPS_SCAN));
										}
										mmgui_module_devices_information(mmguicore);
									}
								} else if ((moduledata->netproxy == NULL) && (g_str_equal(interface, "org.ofono.cdma.NetworkRegistration"))) {
									if (mmgui_module_open_cdma_network_registration_interface(mmguicore, mmguicore->device)) {
										mmgui_module_devices_information(mmguicore);
									}	
								} else if ((moduledata->cardproxy == NULL) && (g_str_equal(interface, "org.ofono.SimManager"))) {
									if (mmgui_module_open_sim_manager_interface(mmguicore, mmguicore->device)) {
										mmgui_module_devices_information(mmguicore);
									}
								} else if ((moduledata->smsproxy == NULL) && (g_str_equal(interface, "org.ofono.MessageManager"))) {
									if (mmgui_module_open_message_manager_interface(mmguicore, mmguicore->device)) {
										/*SMS messaging capabilities updated*/
										if (mmguicore->eventcb != NULL) {
											(mmguicore->eventcb)(MMGUI_EVENT_EXTEND_CAPABILITIES, mmguicore, GINT_TO_POINTER(MMGUI_CAPS_SMS));
										}
									}
								} else  if ((moduledata->smsproxy == NULL) && (g_str_equal(interface, "org.ofono.cdma.MessageManager"))) {
									if (mmgui_module_open_cdma_message_manager_interface(mmguicore, mmguicore->device)) {
										/*SMS messaging capabilities updated*/
										if (mmguicore->eventcb != NULL) {
											(mmguicore->eventcb)(MMGUI_EVENT_EXTEND_CAPABILITIES, mmguicore, GINT_TO_POINTER(MMGUI_CAPS_SMS));
										}
									}
								} else if ((moduledata->ussdproxy == NULL) && (g_str_equal(interface, "org.ofono.SupplementaryServices"))) {
									if (mmgui_module_open_supplementary_services_interface(mmguicore, mmguicore->device)) {
										/*Supplimentary services capabilities updated*/
										if (mmguicore->eventcb != NULL) {
											(mmguicore->eventcb)(MMGUI_EVENT_EXTEND_CAPABILITIES, mmguicore, GINT_TO_POINTER(MMGUI_CAPS_USSD));
										}
									}
								} else if ((moduledata->contactsproxy == NULL) && (g_str_equal(interface, "org.ofono.Phonebook"))) {
									if (mmgui_module_open_phonebook_interface(mmguicore, mmguicore->device)) {
										/*Contacts capabilities updated*/
										if (mmguicore->eventcb != NULL) {
											(mmguicore->eventcb)(MMGUI_EVENT_EXTEND_CAPABILITIES, mmguicore, GINT_TO_POINTER(MMGUI_CAPS_CONTACTS));
										}
									}
								} else if ((moduledata->connectionproxy == NULL) && (g_str_equal(interface, "org.ofono.ConnectionManager"))) {
									if (mmgui_module_open_connection_manager_interface(mmguicore, mmguicore->device)) {
										mmgui_module_devices_information(mmguicore);
									}
								} else if ((moduledata->connectionproxy == NULL) && (g_str_equal(interface, "org.ofono.cdma.ConnectionManager"))) {
									if (mmgui_module_open_cdma_connection_manager_interface(mmguicore, mmguicore->device)) {
										mmgui_module_devices_information(mmguicore);
									}
								}
							}
							g_variant_unref(nodel1);
						}
					}
				} else if (g_str_equal(parameter, "Online")) {
					if (mmguicore->device != NULL) {
						mmguicore->device->enabled = g_variant_get_boolean(value);
						if (mmguicore->eventcb != NULL) {
							if (mmguicore->device->operation != MMGUI_DEVICE_OPERATION_ENABLE) {
								(mmguicore->eventcb)(MMGUI_EVENT_DEVICE_ENABLED_STATUS, mmguicore, GUINT_TO_POINTER(mmguicore->device->enabled));
							} else {
								if (mmguicore->device != NULL) {
									mmguicore->device->operation = MMGUI_DEVICE_OPERATION_IDLE;
								}
								if (mmguicore->eventcb != NULL) {
									(mmguicore->eventcb)(MMGUI_EVENT_MODEM_ENABLE_RESULT, mmguicore, GUINT_TO_POINTER(TRUE));
								}				
							}
						}
					}
				}
				g_variant_unref(value);
			}
		}
	}
}

static void mmgui_module_card_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data)
{
	mmguicore_t mmguicore;
	moduledata_t moduledata;
	GVariant *propname, *propvalue, *value;
	const gchar *parameter;
	gsize strsize;
		
	mmguicore = (mmguicore_t)data;
	if (mmguicore == NULL) return;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	if (moduledata == NULL) return;
	
	if (g_str_equal(signal_name, "PropertyChanged")) {
		/*Property name and value*/
		propname = g_variant_get_child_value(parameters, 0);
		propvalue = g_variant_get_child_value(parameters, 1);
		if ((propname != NULL) && (propvalue != NULL)) {
			/*Unboxed parameter and value*/
			strsize = 256;
			parameter = g_variant_get_string(propname, &strsize);
			value = g_variant_get_variant(propvalue);
			if ((parameter != NULL) && (parameter[0] != '\0') && (value != NULL)) {
				if (g_str_equal(parameter, "PinRequired")) {
					/*Locked state*/
					if (mmguicore->device != NULL) {
						strsize = 256;
						parameter = g_variant_get_string(value, &strsize);
						if ((parameter != NULL) && (parameter[0] != '\0')) {
							mmguicore->device->blocked = mmgui_module_device_get_locked_from_unlock_string(parameter);
							mmguicore->device->locktype = mmgui_module_device_get_lock_type_from_unlock_string(parameter);
							if (mmguicore->eventcb != NULL) {
								if (mmguicore->device->operation != MMGUI_DEVICE_OPERATION_UNLOCK) {
									(mmguicore->eventcb)(MMGUI_EVENT_DEVICE_BLOCKED_STATUS, mmguicore, GUINT_TO_POINTER(mmguicore->device->blocked));
								} else {
									if (mmguicore->device != NULL) {
										mmguicore->device->operation = MMGUI_DEVICE_OPERATION_IDLE;
									}
									if (mmguicore->eventcb != NULL) {
										(mmguicore->eventcb)(MMGUI_EVENT_MODEM_UNLOCK_WITH_PIN_RESULT, mmguicore, GUINT_TO_POINTER(TRUE));
									}
								}
							}
						}
					}
				}
				g_variant_unref(value);
			}
		}
	}
}

static void mmgui_module_sms_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data)
{
	mmguicore_t mmguicore;
	mmgui_sms_message_t message;
	moduledata_t moduledata;
	guint messageid;
				
	mmguicore = (mmguicore_t)data;
	if (mmguicore == NULL) return;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	if (moduledata == NULL) return;
	
	if (mmguicore->eventcb != NULL) {
		if ((g_str_equal(signal_name, "IncomingMessage")) || (g_str_equal(signal_name, "ImmediateMessage"))) {
			/*Receive message*/
			message = mmgui_module_sms_retrieve(mmguicore, parameters);
			if (message != NULL) {
				/*Store message in list*/
				messageid = g_list_length(moduledata->msgqueue);
				moduledata->msgqueue = g_list_append(moduledata->msgqueue, message);
				/*Send signal*/
				if (mmguicore->eventcb != NULL) {
					(mmguicore->eventcb)(MMGUI_EVENT_SMS_COMPLETED, mmguicore, GUINT_TO_POINTER(messageid));
				}
			}
		}
	}
}

static void mmgui_module_connection_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data)
{
	mmguicore_t mmguicore;
	moduledata_t moduledata;
	GVariant *propname, *propvalue, *value;
	const gchar *parameter;
	gsize strsize;
					
	mmguicore = (mmguicore_t)data;
	if (mmguicore == NULL) return;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	if (moduledata == NULL) return;
	
	if (g_str_equal(signal_name, "PropertyChanged")) {
		/*Property name and value*/
		propname = g_variant_get_child_value(parameters, 0);
		propvalue = g_variant_get_child_value(parameters, 1);
		if ((propname != NULL) && (propvalue != NULL)) {
			/*Unboxed parameter and value*/
			strsize = 256;
			parameter = g_variant_get_string(propname, &strsize);
			value = g_variant_get_variant(propvalue);
			if ((parameter != NULL) && (parameter[0] != '\0') && (value != NULL)) {
				if (g_str_equal(parameter, "Bearer")) {
					/*Network mode*/
					if (mmguicore->device != NULL) {
						strsize = 256;
						parameter = g_variant_get_string(value, &strsize);
						if ((parameter != NULL) && (parameter[0] != '\0')) {
							mmguicore->device->mode = mmgui_module_access_mode_translate(parameter);
							if (mmguicore->eventcb != NULL) {
								(mmguicore->eventcb)(MMGUI_EVENT_NETWORK_MODE_CHANGE, mmguicore, mmguicore->device);
							}
						}
					}
				}
				g_variant_unref(value);
			}
		}
	}
}

static enum _mmgui_device_modes mmgui_module_access_mode_translate(const gchar *mode)
{
	enum _mmgui_device_modes tmode;
	
	if (mode == NULL) return MMGUI_DEVICE_MODE_UNKNOWN;
	
	if (g_str_equal(mode, "gsm")) {
		tmode = MMGUI_DEVICE_MODE_GSM;
	} else if (g_str_equal(mode, "gprs")) {
		tmode = MMGUI_DEVICE_MODE_GSM;
	} else if (g_str_equal(mode, "edge")) {
		tmode = MMGUI_DEVICE_MODE_EDGE;
	} else if (g_str_equal(mode, "umts")) {
		tmode = MMGUI_DEVICE_MODE_UMTS;
	} else if (g_str_equal(mode, "hsdpa")) {
		tmode = MMGUI_DEVICE_MODE_HSDPA;
	} else if (g_str_equal(mode, "hsupa")) {
		tmode = MMGUI_DEVICE_MODE_HSUPA;
	} else if (g_str_equal(mode, "hspa")) {
		tmode = MMGUI_DEVICE_MODE_HSPA;
	} else if (g_str_equal(mode, "lte")) {
		tmode = MMGUI_DEVICE_MODE_LTE;
	} else {
		tmode = MMGUI_DEVICE_MODE_UNKNOWN;
	}
	
	return tmode;
}

static enum _mmgui_reg_status mmgui_module_registration_status_translate(const gchar *status)
{
	enum _mmgui_reg_status tstatus;
	
	if (status == NULL) return MMGUI_REG_STATUS_UNKNOWN;
	
	if (g_str_equal(status, "unregistered")) {
		tstatus = MMGUI_REG_STATUS_IDLE;
	} else if (g_str_equal(status, "registered")) {
		tstatus = MMGUI_REG_STATUS_HOME;
	} else if (g_str_equal(status, "searching")) {
		tstatus = MMGUI_REG_STATUS_SEARCHING;
	} else if (g_str_equal(status, "denied")) {
		tstatus = MMGUI_REG_STATUS_DENIED;
	} else if (g_str_equal(status, "unknown")) {
		tstatus = MMGUI_REG_STATUS_UNKNOWN;
	} else if (g_str_equal(status, "roaming")) {
		tstatus = MMGUI_REG_STATUS_ROAMING;
	} else {
		tstatus = MMGUI_REG_STATUS_UNKNOWN;
	}
	
	return tstatus;
}

static enum _mmgui_network_availability mmgui_module_network_availability_status_translate(const gchar* status)
{
	guint tstatus;
	
	if (status == NULL) return MMGUI_REG_STATUS_UNKNOWN;
	
	if (g_str_equal(status, "unknown")) {
		tstatus = MMGUI_NA_UNKNOWN;
	} else if (g_str_equal(status, "available")) {
		tstatus = MMGUI_NA_AVAILABLE;
	} else if (g_str_equal(status, "current")) {
		tstatus = MMGUI_NA_CURRENT;
	} else if (g_str_equal(status, "forbidden")) {
		tstatus = MMGUI_NA_FORBIDDEN;
	} else {
		tstatus = MMGUI_NA_UNKNOWN;
	}
	
	return tstatus;	
}

static enum _mmgui_access_tech mmgui_module_access_technology_translate(const gchar *technology)
{
	enum _mmgui_access_tech ttechnology;
	
	if (technology == NULL) return MMGUI_ACCESS_TECH_UNKNOWN;
	
	if (g_str_equal(technology, "gsm")) {
		ttechnology = MMGUI_ACCESS_TECH_GSM;
	} else if (g_str_equal(technology, "edge")) {
		ttechnology = MMGUI_ACCESS_TECH_EDGE;
	} else if (g_str_equal(technology, "umts")) {
		ttechnology = MMGUI_ACCESS_TECH_UMTS;
	} else if (g_str_equal(technology, "hspa")) {
		ttechnology = MMGUI_ACCESS_TECH_HSPA;
	} else if (g_str_equal(technology, "lte")) {
		ttechnology = MMGUI_ACCESS_TECH_LTE;
	} else {
		ttechnology = MMGUI_ACCESS_TECH_UNKNOWN;
	}
	
	return ttechnology;
}

static GVariant *mmgui_module_proxy_get_property(GDBusProxy *proxy, const gchar *name, const GVariantType *type)
{
	GError *error;
	GVariant *data, *dict, *property;
		
	if ((proxy == NULL) || (name == NULL) || (type == NULL)) return NULL;
	
	error = NULL;
	
	data = g_dbus_proxy_call_sync(proxy,
								"GetProperties",
								NULL,
								0,
								-1,
								NULL,
								&error);
	
	if ((data == NULL) && (error != NULL)) {
		g_error_free(error);
		return NULL;
	}
	
	dict = g_variant_get_child_value(data, 0);
	if (dict == NULL) {
		g_variant_unref(data);
		return NULL;
	}
	
	property = g_variant_lookup_value(dict, name, type);
	if (property == NULL) {
		g_variant_unref(dict);
		g_variant_unref(data);
		return NULL;
	}
	
	g_variant_unref(dict);
	g_variant_unref(data);
	
	return property;
}

static gboolean mmgui_module_device_get_enabled(mmguicore_t mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *deviceinfo, *propdict, *propenabled;
	gboolean enabled;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (moduledata->modemproxy == NULL) return FALSE;
	
	error = NULL;
	
	deviceinfo = g_dbus_proxy_call_sync(moduledata->modemproxy,
										"GetProperties",
										NULL,
										0,
										-1,
										NULL,
										&error);
	
	if ((deviceinfo == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
		return FALSE;
	}
	
	enabled = FALSE;
	
	/*Properties dictionary (not dbus properties)*/
	propdict = g_variant_get_child_value(deviceinfo, 0);
	if (propdict != NULL) {
		propenabled = g_variant_lookup_value(propdict, "Online", G_VARIANT_TYPE_BOOLEAN);
		if (propenabled != NULL) {
			enabled = g_variant_get_boolean(propenabled);
			g_variant_unref(propenabled);
		}
		g_variant_unref(propdict);
	}
	g_variant_unref(deviceinfo);
	
	return enabled;
}

static gchar *mmgui_module_device_get_unlock_string(mmguicore_t mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *deviceinfo, *propdict, *proplocked;
	const gchar *propstring;
	gsize propstrsize;
	gchar *res;
	
	if (mmguicore == NULL) return NULL;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return NULL;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return NULL;
	if (moduledata->cardproxy == NULL) return NULL;
	
	error = NULL;
	
	deviceinfo = g_dbus_proxy_call_sync(moduledata->cardproxy,
										"GetProperties",
										NULL,
										0,
										-1,
										NULL,
										&error);
	
	if ((deviceinfo == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
		return NULL;
	}
	
	res = NULL;
	
	/*Properties dictionary (not dbus properties)*/
	propdict = g_variant_get_child_value(deviceinfo, 0);
	if (propdict != NULL) {
		proplocked = g_variant_lookup_value(propdict, "PinRequired", G_VARIANT_TYPE_STRING);
		if (proplocked != NULL) {
			propstrsize = 256;
			propstring = g_variant_get_string(proplocked, &propstrsize);
			if ((propstring != NULL) && (propstring[0] != '\0')) {
				res = g_strdup(propstring);
			}
			g_variant_unref(proplocked);
		}
		g_variant_unref(propdict);
	}
	g_variant_unref(deviceinfo);
	
	return res;
}

static gboolean mmgui_module_device_get_locked_from_unlock_string(const gchar *ustring)
{
	gboolean locked;
	
	if (ustring == NULL) return FALSE;
	
	if (g_strcmp0(ustring, "none") == 0) {
		locked = FALSE;
	} else {
		locked = TRUE;
	}
	
	return locked;
}

static gint mmgui_module_device_get_lock_type_from_unlock_string(const gchar *ustring)
{
	gint locktype;
	
	locktype = MMGUI_LOCK_TYPE_NONE;
	
	if (ustring == NULL) return locktype;
	
	if (g_strcmp0(ustring, "none") == 0) {
		locktype = MMGUI_LOCK_TYPE_NONE;
	} else if (g_strcmp0(ustring, "pin") == 0) {
		locktype = MMGUI_LOCK_TYPE_PIN;
	} else if (g_strcmp0(ustring, "puk") == 0) {
		locktype = MMGUI_LOCK_TYPE_PUK;
	} else {
		locktype = MMGUI_LOCK_TYPE_OTHER;
	}
	
	return locktype;
}

static gboolean mmgui_module_device_get_registered(mmguicore_t mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *deviceinfo, *propdict, *propreg;
	const gchar *regstr;
	gsize regstrsize;
	gboolean registered;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (moduledata->netproxy == NULL) return FALSE;
	
	error = NULL;
	
	deviceinfo = g_dbus_proxy_call_sync(moduledata->netproxy,
										"GetProperties",
										NULL,
										0,
										-1,
										NULL,
										&error);
	
	if ((deviceinfo == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
		return FALSE;
	}
	
	registered = FALSE;
	
	/*Properties dictionary (not dbus properties)*/
	propdict = g_variant_get_child_value(deviceinfo, 0);
	if (propdict != NULL) {
		propreg = g_variant_lookup_value(propdict, "Status", G_VARIANT_TYPE_STRING);
		if (propreg != NULL) {
			regstrsize = 256;
			regstr = g_variant_get_string(propreg, &regstrsize);
			if ((regstr != NULL) && (regstr[0] != '\0')) {
				if (g_str_equal(regstr, "registered") || g_str_equal(regstr, "roaming")) {
					registered = TRUE;
				} else {
					registered = FALSE;
				}
			}
			g_variant_unref(propreg);
		}
		g_variant_unref(propdict);
	}
	g_variant_unref(deviceinfo);
	
	return registered;
}

static gboolean mmgui_module_device_get_connected(mmguicore_t mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *contexts, *properties, *propdict;
	GVariantIter coniterl1, coniterl2;
	GVariant *connodel1, *connodel2;
	GVariant *conparams, *typev, *statev;
	gsize strlength;
	const gchar *type;
	gboolean connected;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (!mmguicorelc->device->enabled) return FALSE;
	
	if (moduledata->connectionproxy == NULL) return FALSE;
	
	connected = FALSE;
	
	error = NULL;
	
	if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_GSM) {
		contexts = g_dbus_proxy_call_sync(moduledata->connectionproxy,
											"GetContexts",
											NULL,
											0,
											-1,
											NULL,
											&error);
		
		if ((contexts == NULL) && (error != NULL)) {
			mmgui_module_handle_error_message(mmguicore, error);
			g_error_free(error);
			return FALSE;
		}
		
		g_variant_iter_init(&coniterl1, contexts);
		while (((connodel1 = g_variant_iter_next_value(&coniterl1)) != NULL) && (!connected)) {
			g_variant_iter_init(&coniterl2, connodel1);
			while (((connodel2 = g_variant_iter_next_value(&coniterl2)) != NULL) && (!connected)) {
				/*Parameters*/
				conparams = g_variant_get_child_value(connodel2, 1);
				if (conparams != NULL) {
					/*Type*/
					typev = g_variant_lookup_value(conparams, "Type", G_VARIANT_TYPE_STRING);
					if (typev != NULL) {
						strlength = 256;
						type = g_variant_get_string(typev, &strlength);
						if ((type != NULL) && (type[0] != '\0')) {
							if (g_str_equal(type, "internet")) {
								/*State*/
								statev = g_variant_lookup_value(conparams, "Active", G_VARIANT_TYPE_BOOLEAN);
								if (statev != NULL) {
									connected = g_variant_get_boolean(statev);
									g_variant_unref(statev);
								}
							}
						}
						g_variant_unref(typev);
					}
					g_variant_unref(conparams);
				}
				g_variant_unref(connodel2);
			}
			g_variant_unref(connodel1);
		}
		g_variant_unref(contexts);
	} else if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_CDMA) {
		properties = g_dbus_proxy_call_sync(moduledata->connectionproxy,
											"GetProperties",
											NULL,
											0,
											-1,
											NULL,
											&error);
		
		if ((properties == NULL) && (error != NULL)) {
			mmgui_module_handle_error_message(mmguicore, error);
			g_error_free(error);
			return FALSE;
		}
		
		propdict = g_variant_get_child_value(properties, 0);
		
		if (propdict == NULL) {
			g_variant_unref(properties);
			return FALSE;
		}
		
		statev = g_variant_lookup_value(propdict, "Powered", G_VARIANT_TYPE_BOOLEAN);
		if (statev != NULL) {
			connected = g_variant_get_boolean(statev);
			g_variant_unref(statev);
		}
	}
	
	return connected;
}

static GVariant *mmgui_module_device_queue_get_properties(mmguicore_t mmguicore, const gchar *devpath)
{
	moduledata_t moduledata;
	GDBusProxy *deviceproxy;
	GError *error;
	GVariant *deviceinfo, *propdict, *devproppow, *devpropman, *devpropmod, *devproprev;
	gboolean powered;
			
	if ((mmguicore == NULL) || (devpath == NULL)) return NULL;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	
	if (moduledata == NULL) return NULL;
	if (moduledata->connection == NULL) return NULL;
	
	error = NULL;
	
	deviceproxy = g_dbus_proxy_new_sync(moduledata->connection,
										G_DBUS_PROXY_FLAGS_NONE,
										NULL,
										"org.ofono",
										devpath,
										"org.ofono.Modem",
										NULL,
										&error);
	
	if ((deviceproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
		g_object_unref(deviceproxy);
		return NULL;
	}
	
	error = NULL;
	
	deviceinfo = g_dbus_proxy_call_sync(deviceproxy,
										"GetProperties",
										NULL,
										0,
										-1,
										NULL,
										&error);
	
	if ((deviceinfo == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
		g_object_unref(deviceproxy);
		return NULL;
	}
	
	/*Properties dictionary (not dbus properties)*/
	propdict = g_variant_get_child_value(deviceinfo, 0);
	
	if (propdict == NULL) {
		g_variant_unref(deviceinfo);
		g_object_unref(deviceproxy);
		return NULL;
	}
	
	/*If device is powered*/
	devproppow = g_variant_lookup_value(propdict, "Powered", G_VARIANT_TYPE_BOOLEAN);
	
	if (devproppow != NULL) {
		/*Test if modem powered*/
		powered = g_variant_get_boolean(devproppow);
		g_variant_unref(devproppow);
		if (!powered) {
			/*If modem is not powered try to power on*/
			error = NULL;
			g_dbus_proxy_call_sync(deviceproxy,
									"SetProperty",
									g_variant_new("(sv)", "Powered", g_variant_new_boolean(TRUE)),
									0,
									-1,
									NULL,
									&error);
			/*If powered on, return properties*/
			if (error != NULL) {
				/*Proxy is not needed anymore*/
				g_object_unref(deviceproxy);
				mmgui_module_handle_error_message(mmguicore, error);
				g_error_free(error);
				return NULL;
			}
		}
	}
	
	/*These values are strictly required, so test existence*/
	devpropman = g_variant_lookup_value(propdict, "Manufacturer", G_VARIANT_TYPE_STRING);
	devpropmod = g_variant_lookup_value(propdict, "Model", G_VARIANT_TYPE_STRING);
	devproprev = g_variant_lookup_value(propdict, "Revision", G_VARIANT_TYPE_STRING);
	
	if ((devpropman != NULL) && (devpropmod != NULL) && (devproprev != NULL)) {
		g_variant_unref(devpropman);
		g_variant_unref(devpropmod);
		g_variant_unref(devproprev);
		g_object_unref(deviceproxy);
		return propdict;
	}
	
	if (devpropman != NULL) {
		g_variant_unref(devpropman);
	}
	
	if (devpropmod != NULL) {
		g_variant_unref(devpropmod);
	}
	
	if (devproprev != NULL) {
		g_variant_unref(devproprev);
	}
	
	g_variant_unref(propdict);
	g_variant_unref(deviceinfo);
	g_object_unref(deviceproxy);
		
	return NULL;
}

static mmguidevice_t mmgui_module_device_new(mmguicore_t mmguicore, const gchar *devpath, GVariant *devprops)
{
	mmguidevice_t device;
	moduledata_t moduledata;
	GVariant *devprop, *interfaces;
	GVariantIter iterl1;
	GVariant *nodel1;
	gsize strsize;
	const gchar *parameter;
	
	if ((mmguicore == NULL) || (devpath == NULL) || (devprops == NULL)) return NULL;
	
	moduledata = (moduledata_t)mmguicore->moduledata;
	
	if (moduledata == NULL) return NULL;
	if (moduledata->connection == NULL) return NULL;
	
	device = g_new0(struct _mmguidevice, 1);
	
	//Save device identifier and object path
	device->id = mmgui_module_device_id(devpath);
	device->objectpath = g_strdup(devpath);
	
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
	
	
	devprop = g_variant_lookup_value(devprops, "Online", G_VARIANT_TYPE_BOOLEAN);
	if (devprop != NULL) {
		device->enabled = g_variant_get_boolean(devprop);
		g_variant_unref(devprop);
	} else {
		device->enabled = FALSE;
	}
	
	//Blocked must be retrived from SIM interface
	device->blocked = FALSE;
	
	devprop = g_variant_lookup_value(devprops, "Manufacturer", G_VARIANT_TYPE_STRING);
	if (devprop != NULL) {
		strsize = 256;
		parameter = g_variant_get_string(devprop, &strsize);
		if ((parameter != NULL) && (parameter[0] != '\0')) {
			device->manufacturer = g_strdup(parameter);
		} else {
			device->manufacturer = g_strdup(_("Unknown"));
		}
		g_variant_unref(devprop);
	} else {
		device->manufacturer = g_strdup(_("Unknown"));
	}
	
	devprop = g_variant_lookup_value(devprops, "Model", G_VARIANT_TYPE_STRING);
	if (devprop != NULL) {
		strsize = 256;
		parameter = g_variant_get_string(devprop, &strsize);
		if ((parameter != NULL) && (parameter[0] != '\0')) {
			device->model = g_strdup(parameter);
		} else {
			device->model = g_strdup(_("Unknown"));
		}
		g_variant_unref(devprop);
	} else {
		device->model = g_strdup(_("Unknown"));
	}
	
	devprop = g_variant_lookup_value(devprops, "Revision", G_VARIANT_TYPE_STRING);
	if (devprop != NULL) {
		strsize = 256;
		parameter = g_variant_get_string(devprop, &strsize);
		if ((parameter != NULL) && (parameter[0] != '\0')) {
			device->version = g_strdup(parameter);
		} else {
			device->version = g_strdup(_("Unknown"));
		}
		g_variant_unref(devprop);
	} else {
		device->version = g_strdup(_("Unknown"));
	}
	
	/*No port information can be obtained from oFono*/
	device->port = g_strdup(_("Unknown"));
	
	/*No sysfs path can be obtained from oFono*/
	device->sysfspath = NULL;
	
	/*Internal identifier is MM-only thing*/
	device->internalid = NULL;
	
	/*Device type*/
	device->type = MMGUI_DEVICE_TYPE_GSM;
	
	interfaces = g_variant_lookup_value(devprops, "Interfaces", G_VARIANT_TYPE_STRING);
	if (interfaces != NULL) {
		g_variant_iter_init(&iterl1, interfaces);
		while ((nodel1 = g_variant_iter_next_value(&iterl1)) != NULL) {
			strsize = 256;
			parameter = g_variant_get_string(nodel1, &strsize);
			if ((parameter != NULL) && (parameter[0] != '\0')) {
				if (strstr(parameter, "org.ofono.cdma") != NULL) {
					device->type = MMGUI_DEVICE_TYPE_CDMA;
					break;
				}
			}					
			g_variant_unref(nodel1);
		}
	}
	
	/*Persistent device identifier*/
	parameter = g_strdup_printf("%s_%s_%s", device->manufacturer, device->model, device->version);
	device->persistentid = g_compute_checksum_for_string(G_CHECKSUM_MD5, parameter, -1);
	g_free((gchar *)parameter);
	
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
	g_snprintf(module->servicename, sizeof(module->servicename), MMGUI_MODULE_SERVICE_NAME);
	g_snprintf(module->systemdname, sizeof(module->systemdname), MMGUI_MODULE_SYSTEMD_NAME);
	g_snprintf(module->description, sizeof(module->description), MMGUI_MODULE_DESCRIPTION);
	
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
	
	/*Initialize queues*/
	(*moduledata)->devqueue = NULL;
	(*moduledata)->msgqueue = NULL;
	
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
													"org.ofono",
													"/",
													"org.ofono.Manager",
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
	
	/*History storage*/
	(*moduledata)->historyshm = mmgui_history_client_new();
	
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
		
		if (moduledata->historyshm != NULL) {
			mmgui_history_client_close(moduledata->historyshm);
			moduledata->historyshm = NULL;
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
	GVariant *devpathv, *devprops;
	const gchar *devpath;
	gsize devpathsize;
	GVariant *devtypev;
	const gchar *typestr;
	gsize typestrsize;
	gboolean devreal;
	GVariant *devpoweredv;
	gboolean devpowered;
	
	if ((mmguicore == NULL) || (devicelist == NULL)) return 0;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return 0;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	error = NULL;
	
	devices = g_dbus_proxy_call_sync(moduledata->managerproxy,
									"GetModems",
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
			devpathv = g_variant_get_child_value(dnodel2, 0);
			devprops = g_variant_get_child_value(dnodel2, 1);
			if ((devpathv != NULL) && (devprops != NULL)) {
				devpathsize = 256;
				devpath = g_variant_get_string(devpathv, &devpathsize);
				if ((devpath != NULL) && (devpath[0] != '\0')) {
					/*Determine if modem is not emulated*/
					devreal = FALSE;
					devtypev = g_variant_lookup_value(devprops, "Type", G_VARIANT_TYPE_STRING);
					if (devtypev != NULL) {
						typestrsize = 256;
						typestr = g_variant_get_string(devtypev, &typestrsize);
						if ((typestr != NULL) && (typestr[0] != '\0')) {
							if (g_str_equal(typestr, "hardware")) {
								devreal = TRUE;
							}
						}
						g_variant_unref(devtypev);
					}
					/*If modem is not emulated, work with it*/
					if (devreal) {
						/*Determine if modem is powered*/
						devpoweredv = g_variant_lookup_value(devprops, "Powered", G_VARIANT_TYPE_BOOLEAN);
						if (devpoweredv != NULL) {
							devpowered = g_variant_get_boolean(devpoweredv);
							g_variant_unref(devpoweredv);
						} else {
							devpowered = FALSE;
						}
						/*Add device to apporitate list*/
						if (!devpowered) {
							/*Add device to waiting queue if not powered*/
							moduledata->devqueue = g_list_prepend(moduledata->devqueue, g_strdup(devpath));
						} else {
							/*Add device to list if already powered*/
							*devicelist = g_slist_prepend(*devicelist, mmgui_module_device_new(mmguicore, devpath, devprops));
							devnum++;
						}
					}
					g_variant_unref(devpathv);
					g_variant_unref(devprops);
				}
			}
			g_variant_unref(dnodel2);
		}
		g_variant_unref(dnodel1);
    }
	
	g_variant_unref(devices);
	
	return devnum;
}

G_MODULE_EXPORT gboolean mmgui_module_devices_state(gpointer mmguicore, enum _mmgui_device_state_request request)
{
	mmguicore_t mmguicorelc;
	/*moduledata_t moduledata;*/
	mmguidevice_t device;
	gchar *ustring;
	gboolean res;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	/*moduledata = (moduledata_t)mmguicorelc->moduledata;*/
	
	if (mmguicorelc->device == NULL) return FALSE;
	device = mmguicorelc->device;
	
	switch (request) {
		case MMGUI_DEVICE_STATE_REQUEST_ENABLED:
			/*Is device enabled*/
			res = mmgui_module_device_get_enabled(mmguicorelc);
			if (device->operation != MMGUI_DEVICE_OPERATION_ENABLE) {
				device->enabled = res;
			}
			break;
		case MMGUI_DEVICE_STATE_REQUEST_LOCKED:
			/*Is device blocked*/
			ustring = mmgui_module_device_get_unlock_string(mmguicorelc);
			res = mmgui_module_device_get_locked_from_unlock_string(ustring);
			device->locktype = mmgui_module_device_get_lock_type_from_unlock_string(ustring);
			g_free(ustring);
			device->blocked = res;
			break;
		case MMGUI_DEVICE_STATE_REQUEST_REGISTERED:
			/*Is device registered in network*/
			res = mmgui_module_device_get_registered(mmguicorelc);
			device->registered = res;
			break;
		case MMGUI_DEVICE_STATE_REQUEST_CONNECTED:
			/*Is device connected*/
			res = mmgui_module_device_get_connected(mmguicorelc);
			device->connected = res;
			break;
		case MMGUI_DEVICE_STATE_REQUEST_PREPARED:
			/*Is device ready for opearation - maybe add additional code in future*/
			res = TRUE;
			device->prepared = TRUE;
			break;
		default:
			res = FALSE;
			break;
	}
	
	return res;
}

static gboolean mmgui_module_devices_queue_remove(mmguicore_t mmguicore, const gchar *devpath)
{
	moduledata_t moduledata;
	GList *dqlnode;
	GList *dqlnext;
	gchar *dqlpath;
	gboolean res;
	
	if ((mmguicore == NULL) || (devpath == NULL)) return FALSE;
	
	if (mmguicore->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicore->moduledata;
	
	res = FALSE;
	
	//Search for specified device and remove if any
	if (moduledata->devqueue != NULL) {
		dqlnode = moduledata->devqueue;
		while (dqlnode != NULL) {
			dqlpath = (gchar *)dqlnode->data;
			dqlnext = g_list_next(dqlnode);
			if (g_str_equal(devpath, dqlpath)) {
				//Free resources
				g_free(dqlpath);
				//Remove list node
				moduledata->devqueue = g_list_delete_link(moduledata->devqueue, dqlnode);
				//Set flag and break
				res = TRUE;
				break;
			}
			dqlnode = dqlnext;
		}
	}
	
	return res;
}

G_MODULE_EXPORT gboolean mmgui_module_devices_update_state(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	mmguidevice_t device;
	GList *dqlnode;
	GList *dqlnext;
	gchar *dqlpath;
	GVariant *devprops;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	/*Search for ready devices*/
	if ((moduledata->devqueue != NULL) && (mmguicorelc->eventcb != NULL)) {
		dqlnode = moduledata->devqueue;
		while (dqlnode != NULL) {
			dqlpath = (gchar *)dqlnode->data;
			dqlnext = g_list_next(dqlnode);
			/*If device is not ready, NULL is returned*/
			devprops = mmgui_module_device_queue_get_properties(mmguicore, dqlpath);
			if (devprops != NULL) {
				device = mmgui_module_device_new(mmguicore, dqlpath, devprops);
				if (device != NULL) {
					/*Free resources*/
					g_free(dqlpath);
					g_variant_unref(devprops);
					/*Remove list node*/
					moduledata->devqueue = g_list_delete_link(moduledata->devqueue, dqlnode);
					/*Send notification*/
					(mmguicorelc->eventcb)(MMGUI_EVENT_DEVICE_ADDED, mmguicore, device);
				}
			}
			dqlnode = dqlnext;
		}
	}
	
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_devices_information(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	mmguidevice_t device;
	GVariant *data;
	/*GError *error;*/
	//gchar opcode[6];
	//guchar locvalues;
	gsize strsize = 256;
	const gchar *parameter;
	gchar *ustring;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	device = mmguicorelc->device;
	
	if (moduledata->modemproxy != NULL) {
		/*Is device enabled and blocked*/
		device->enabled = mmgui_module_device_get_enabled(mmguicorelc);
		ustring = mmgui_module_device_get_unlock_string(mmguicorelc);
		device->blocked = mmgui_module_device_get_locked_from_unlock_string(ustring);
		device->locktype = mmgui_module_device_get_lock_type_from_unlock_string(ustring);
		g_free(ustring);
		device->registered = mmgui_module_device_get_registered(mmguicorelc);
				
		if (device->enabled) {
			/*Device identifier (IMEI)*/
			if (device->imei != NULL) {
				g_free(device->imei);
				device->imei = NULL;
			}
			
			data = mmgui_module_proxy_get_property(moduledata->modemproxy, "Serial", G_VARIANT_TYPE_STRING);
			if (data != NULL) {
				strsize = 256;
				parameter = g_variant_get_string(data, &strsize);
				if ((parameter != NULL) && (parameter[0] != '\0')) {
					device->imei = g_strdup(parameter);
				} else {
					device->imei = NULL;
				}
				g_variant_unref(data);
			} else {
				device->imei = NULL;
			}
		}
	}
	
	if (moduledata->netproxy != NULL) {
		/*Operator information*/
		device->operatorcode = 0;
		
		if (device->operatorname != NULL) {
			g_free(device->operatorname);
			device->operatorname = NULL;
		}
		/*Signal level*/
		data = mmgui_module_proxy_get_property(moduledata->netproxy, "Strength", G_VARIANT_TYPE_BYTE);
		if (data != NULL) {
			device->siglevel = g_variant_get_byte(data);
			g_variant_unref(data);
		} else {
			device->siglevel = 0;
		}
		/*Used access technology*/
		data = mmgui_module_proxy_get_property(moduledata->netproxy, "Technology", G_VARIANT_TYPE_STRING);
		if (data != NULL) {
			strsize = 256;
			parameter = g_variant_get_string(data, &strsize);
			if ((parameter != NULL) && (parameter[0] != '\0')) {
				device->mode = mmgui_module_access_mode_translate(parameter);
			} else {
				device->mode = MMGUI_DEVICE_MODE_UNKNOWN;
			}
			g_variant_unref(data);
		} else {
			device->mode = MMGUI_DEVICE_MODE_UNKNOWN;
		}
		/*Registration state*/
		data = mmgui_module_proxy_get_property(moduledata->netproxy, "Status", G_VARIANT_TYPE_STRING);
		if (data != NULL) {
			strsize = 256;
			parameter = g_variant_get_string(data, &strsize);
			if ((parameter != NULL) && (parameter[0] != '\0')) {
				device->regstatus = mmgui_module_registration_status_translate(parameter);
			} else {
				device->regstatus = MMGUI_REG_STATUS_UNKNOWN;
			}
			g_variant_unref(data);
		} else {
			device->regstatus = MMGUI_REG_STATUS_UNKNOWN;
		}
		/*Operator name*/
		data = mmgui_module_proxy_get_property(moduledata->netproxy, "Name", G_VARIANT_TYPE_STRING);
		if (data != NULL) {
			strsize = 256;
			parameter = g_variant_get_string(data, &strsize);
			if ((parameter != NULL) && (parameter[0] != '\0')) {
				device->operatorname = g_strdup(parameter);
			} else {
				device->operatorname = NULL;
			}
			g_variant_unref(data);
		} else {
			device->operatorname = NULL;
		}
		/*3gpp location, Operator code*/
		data = mmgui_module_proxy_get_property(moduledata->netproxy, "MobileCountryCode", G_VARIANT_TYPE_STRING);
		if (data != NULL) {
			strsize = 256;
			parameter = g_variant_get_string(data, &strsize);
			if ((parameter != NULL) && (parameter[0] != '\0')) {
				device->loc3gppdata[0] = atoi(parameter);
				device->operatorcode |= (device->loc3gppdata[0] & 0x0000ffff) << 16;
				moduledata->location |= MODULE_INT_MODEM_LOCATION_MCC;
				if (moduledata->location == MODULE_INT_MODEM_LOCATION_ALL) {
					device->locationcaps |= MMGUI_LOCATION_CAPS_3GPP;
				}
			}
			g_variant_unref(data);
		}
		data = mmgui_module_proxy_get_property(moduledata->netproxy, "MobileNetworkCode", G_VARIANT_TYPE_STRING);
		if (data != NULL) {
			strsize = 256;
			parameter = g_variant_get_string(data, &strsize);
			if ((parameter != NULL) && (parameter[0] != '\0')) {
				device->loc3gppdata[1] = atoi(parameter);
				device->operatorcode |= device->loc3gppdata[1] & 0x0000ffff;
				moduledata->location |= MODULE_INT_MODEM_LOCATION_MNC;
				if (moduledata->location == MODULE_INT_MODEM_LOCATION_ALL) {
					device->locationcaps |= MMGUI_LOCATION_CAPS_3GPP;
				}
			}
			g_variant_unref(data);
		}
		data = mmgui_module_proxy_get_property(moduledata->netproxy, "LocationAreaCode", G_VARIANT_TYPE_UINT16);
		if (data != NULL) {
			device->loc3gppdata[2] = g_variant_get_uint16(data);
			moduledata->location |= MODULE_INT_MODEM_LOCATION_LAC;
			if (moduledata->location == MODULE_INT_MODEM_LOCATION_ALL) {
				device->locationcaps |= MMGUI_LOCATION_CAPS_3GPP;
			}
			g_variant_unref(data);
		}
		data = mmgui_module_proxy_get_property(moduledata->netproxy, "CellId", G_VARIANT_TYPE_UINT32);
		if (data != NULL) {
			device->loc3gppdata[3] = g_variant_get_uint32(data);
			moduledata->location |= MODULE_INT_MODEM_LOCATION_CID;
			if (moduledata->location == MODULE_INT_MODEM_LOCATION_ALL) {
				device->locationcaps |= MMGUI_LOCATION_CAPS_3GPP;
			}
			g_variant_unref(data);
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
				data = mmgui_module_proxy_get_property(moduledata->cardproxy, "SubscriberIdentity", G_VARIANT_TYPE_STRING);
				if (data != NULL) {
					strsize = 256;
					parameter = g_variant_get_string(data, &strsize);
					if ((parameter != NULL) && (parameter[0] != '\0')) {
						device->imsi = g_strdup(parameter);
					} else {
						device->imsi = NULL;
					}
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
	
	if (moduledata->connectionproxy != NULL) {
		if (device->type == MMGUI_DEVICE_TYPE_GSM) {
			/*Used access technology*/
			data = mmgui_module_proxy_get_property(moduledata->connectionproxy, "Bearer", G_VARIANT_TYPE_STRING);
			if (data != NULL) {
				strsize = 256;
				parameter = g_variant_get_string(data, &strsize);
				if ((parameter != NULL) && (parameter[0] != '\0')) {
					device->mode = mmgui_module_access_mode_translate(parameter);
				} else {
					device->mode = MMGUI_DEVICE_MODE_UNKNOWN;
				}
				g_variant_unref(data);
			} else {
				device->mode = MMGUI_DEVICE_MODE_UNKNOWN;
			}
		}
	}
			
	return TRUE;
}

static gboolean mmgui_module_open_network_registration_interface(gpointer mmguicore, mmguidevice_t device)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (device->objectpath == NULL) return FALSE;
	
	error = NULL;
	
	moduledata->location = MODULE_INT_MODEM_LOCATION_NULL;
	
	moduledata->netproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.ofono",
												device->objectpath,
												"org.ofono.NetworkRegistration",
												NULL,
												&error);
	
	if ((moduledata->netproxy == NULL) && (error != NULL)) {
		device->scancaps = MMGUI_SCAN_CAPS_NONE;
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	} else {
		device->scancaps = MMGUI_SCAN_CAPS_OBSERVE;
		moduledata->netsignal = g_signal_connect(G_OBJECT(moduledata->netproxy), "g-signal", G_CALLBACK(mmgui_module_network_signal_handler), mmguicore);
		return TRUE;
	}
}

static gboolean mmgui_module_open_cdma_network_registration_interface(gpointer mmguicore, mmguidevice_t device)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (device->objectpath == NULL) return FALSE;
	
	error = NULL;
	
	moduledata->location = MODULE_INT_MODEM_LOCATION_NULL;
	
	/*Update device type*/
	device->type = MMGUI_DEVICE_TYPE_CDMA;
	device->scancaps = MMGUI_SCAN_CAPS_NONE;
		
	moduledata->netproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.ofono",
												device->objectpath,
												"org.ofono.cdma.NetworkRegistration",
												NULL,
												&error);
	
	if ((moduledata->netproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	} else {
		moduledata->netsignal = g_signal_connect(G_OBJECT(moduledata->netproxy), "g-signal", G_CALLBACK(mmgui_module_network_signal_handler), mmguicore);
		return TRUE;
	}
}

static gboolean mmgui_module_open_sim_manager_interface(gpointer mmguicore, mmguidevice_t device)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (device->objectpath == NULL) return FALSE;
	
	error = NULL;
	
	moduledata->cardproxy = g_dbus_proxy_new_sync(moduledata->connection,
													G_DBUS_PROXY_FLAGS_NONE,
													NULL,
													"org.ofono",
													device->objectpath,
													"org.ofono.SimManager",
													NULL,
													&error);
	
	if ((moduledata->cardproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	} else {
		moduledata->cardsignal = g_signal_connect(G_OBJECT(moduledata->cardproxy), "g-signal", G_CALLBACK(mmgui_module_card_signal_handler), mmguicore);
		return TRUE;
	}
}

static gboolean mmgui_module_open_message_manager_interface(gpointer mmguicore, mmguidevice_t device)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (device->objectpath == NULL) return FALSE;
	
	error = NULL;
	
	moduledata->smsproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.ofono",
												device->objectpath,
												"org.ofono.MessageManager",
												NULL,
												&error);
	
	if ((moduledata->smsproxy == NULL) && (error != NULL)) {
		device->smscaps = MMGUI_SMS_CAPS_NONE;
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	} else {
		device->smscaps = MMGUI_SMS_CAPS_RECEIVE | MMGUI_SMS_CAPS_SEND;
		moduledata->smssignal = g_signal_connect(moduledata->smsproxy, "g-signal", G_CALLBACK(mmgui_module_sms_signal_handler), mmguicore);
		return TRUE;
	}
}

static gboolean mmgui_module_open_cdma_message_manager_interface(gpointer mmguicore, mmguidevice_t device)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (device->objectpath == NULL) return FALSE;
	
	/*Update device type*/
	device->type = MMGUI_DEVICE_TYPE_CDMA;
	
	error = NULL;
	
	moduledata->smsproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.ofono",
												device->objectpath,
												"org.ofono.cdma.MessageManager",
												NULL,
												&error);
	
	if ((moduledata->smsproxy == NULL) && (error != NULL)) {
		device->smscaps = MMGUI_SMS_CAPS_NONE;
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	} else {
		device->smscaps = MMGUI_SMS_CAPS_RECEIVE | MMGUI_SMS_CAPS_SEND;
		moduledata->smssignal = g_signal_connect(moduledata->smsproxy, "g-signal", G_CALLBACK(mmgui_module_sms_signal_handler), mmguicore);
		return TRUE;
	}
}

static gboolean mmgui_module_open_supplementary_services_interface(gpointer mmguicore, mmguidevice_t device)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (device->objectpath == NULL) return FALSE;
	
	error = NULL;
	
	moduledata->ussdproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.ofono",
												device->objectpath,
												"org.ofono.SupplementaryServices",
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

static gboolean mmgui_module_open_phonebook_interface(gpointer mmguicore, mmguidevice_t device)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (device->objectpath == NULL) return FALSE;
	
	error = NULL;
	
	moduledata->contactsproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.ofono",
												device->objectpath,
												"org.ofono.Phonebook",
												NULL,
												&error);
	
	if ((moduledata->contactsproxy == NULL) && (error != NULL)) {
		device->contactscaps = MMGUI_CONTACTS_CAPS_NONE;
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	} else {
		device->contactscaps = MMGUI_CONTACTS_CAPS_EXPORT | MMGUI_CONTACTS_CAPS_EXTENDED;
		return TRUE;
	}
}

static gboolean mmgui_module_open_connection_manager_interface(gpointer mmguicore, mmguidevice_t device)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (device->objectpath == NULL) return FALSE;
	
	error = NULL;
	
	moduledata->connectionproxy = g_dbus_proxy_new_sync(moduledata->connection,
														G_DBUS_PROXY_FLAGS_NONE,
														NULL,
														"org.ofono",
														device->objectpath,
														"org.ofono.ConnectionManager",
														NULL,
														&error);
	
	if ((moduledata->connectionproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	} else {
		moduledata->connectionsignal = g_signal_connect(moduledata->connectionproxy, "g-signal", G_CALLBACK(mmgui_module_connection_signal_handler), mmguicore);
		return TRUE;
	}
}

static gboolean mmgui_module_open_cdma_connection_manager_interface(gpointer mmguicore, mmguidevice_t device)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (device->objectpath == NULL) return FALSE;
	
	/*Update device type*/
	device->type = MMGUI_DEVICE_TYPE_CDMA;
	
	error = NULL;
	
	moduledata->connectionproxy = g_dbus_proxy_new_sync(moduledata->connection,
														G_DBUS_PROXY_FLAGS_NONE,
														NULL,
														"org.ofono",
														device->objectpath,
														"org.ofono.cdma.ConnectionManager",
														NULL,
														&error);
	
	if ((moduledata->connectionproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	} else {
		moduledata->connectionsignal = g_signal_connect(moduledata->connectionproxy, "g-signal", G_CALLBACK(mmgui_module_connection_signal_handler), mmguicore);
		return TRUE;
	}
}

G_MODULE_EXPORT gboolean mmgui_module_devices_open(gpointer mmguicore, mmguidevice_t device)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *interfaces;
	GVariantIter iterl1;
	GVariant *nodel1;
	const gchar *interface;
	gsize strsize;
	
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (device == NULL) return FALSE;
	if (device->objectpath == NULL) return FALSE;
	
	/*Interfaces can appear suddenly*/
	moduledata->netproxy = NULL;
	moduledata->cardproxy = NULL;
	moduledata->smsproxy = NULL;
	moduledata->ussdproxy = NULL;
	moduledata->contactsproxy = NULL;
	moduledata->connectionproxy = NULL;
	
	error = NULL;
	
	moduledata->modemproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.ofono",
												device->objectpath,
												"org.ofono.Modem",
												NULL,
												&error);
	
	if ((moduledata->modemproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
	} else {
		moduledata->modemsignal = g_signal_connect(G_OBJECT(moduledata->modemproxy), "g-signal", G_CALLBACK(mmgui_module_modem_signal_handler), mmguicore);
		/*Get available interfaces*/
		interfaces = mmgui_module_proxy_get_property(moduledata->modemproxy, "Interfaces", G_VARIANT_TYPE_ARRAY);
		if (interfaces != NULL) {
			g_variant_iter_init(&iterl1, interfaces);
			while ((nodel1 = g_variant_iter_next_value(&iterl1)) != NULL) {
				interface = g_variant_get_string(nodel1, &strsize);
				if ((interface != NULL) && (interface[0] != '\0')) {
					if (g_str_equal(interface, "org.ofono.NetworkRegistration")) {
						mmgui_module_open_network_registration_interface(mmguicore, device);
					} else if (g_str_equal(interface, "org.ofono.cdma.NetworkRegistration")) {
						mmgui_module_open_cdma_network_registration_interface(mmguicore, device);
					} else if (g_str_equal(interface, "org.ofono.SimManager")) {
						mmgui_module_open_sim_manager_interface(mmguicore, device);
					} else if (g_str_equal(interface, "org.ofono.MessageManager")) {
						mmgui_module_open_message_manager_interface(mmguicore, device);
					} else if (g_str_equal(interface, "org.ofono.cdma.MessageManager")) {
						mmgui_module_open_cdma_message_manager_interface(mmguicore, device);
					} else if (g_str_equal(interface, "org.ofono.SupplementaryServices")) {
						mmgui_module_open_supplementary_services_interface(mmguicore, device);
					} else if (g_str_equal(interface, "org.ofono.Phonebook")) {
						mmgui_module_open_phonebook_interface(mmguicore, device);
					} else if (g_str_equal(interface, "org.ofono.ConnectionManager")) {
						mmgui_module_open_connection_manager_interface(mmguicore, device);
					} else if (g_str_equal(interface, "org.ofono.cdma.ConnectionManager")) {
						mmgui_module_open_cdma_connection_manager_interface(mmguicore, device);
					}
				}
				g_variant_unref(nodel1);
			}
			g_variant_unref(interfaces);
		}
	}
	
	/*Update device information using created proxy objects*/
	mmgui_module_devices_information(mmguicore);
	
	/*Open device history storage*/
	if (moduledata->historyshm != NULL) {
		mmgui_history_client_open_device(moduledata->historyshm, device->objectpath);
	}
	
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
	
	/*Close proxy objects*/
	if (moduledata->cardproxy != NULL) {
		if (g_signal_handler_is_connected(moduledata->cardproxy, moduledata->cardsignal)) {
			g_signal_handler_disconnect(moduledata->cardproxy, moduledata->cardsignal);
		}
		g_object_unref(moduledata->cardproxy);
		moduledata->cardproxy = NULL;
	}
	if (moduledata->netproxy != NULL) {
		if (g_signal_handler_is_connected(moduledata->netproxy, moduledata->netsignal)) {
			g_signal_handler_disconnect(moduledata->netproxy, moduledata->netsignal);
		}
		g_object_unref(moduledata->netproxy);
		moduledata->netproxy = NULL;
	}
	if (moduledata->modemproxy != NULL) {
		if (g_signal_handler_is_connected(moduledata->modemproxy, moduledata->modemsignal)) {
			g_signal_handler_disconnect(moduledata->modemproxy, moduledata->modemsignal);
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
	if (moduledata->contactsproxy != NULL) {
		g_object_unref(moduledata->contactsproxy);
		moduledata->contactsproxy = NULL;
	}
	if (moduledata->connectionproxy != NULL) {
		if (g_signal_handler_is_connected(moduledata->connectionproxy, moduledata->connectionsignal)) {
			g_signal_handler_disconnect(moduledata->connectionproxy, moduledata->connectionsignal);
		}
		g_object_unref(moduledata->connectionproxy);
		moduledata->connectionproxy = NULL;
	}
	/*Close device history storage*/
	if (moduledata->historyshm != NULL) {
		mmgui_history_client_close_device(moduledata->historyshm);
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
												"org.ofono",
												device->objectpath,
												"org.ofono.SupplementaryServices",
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
	GVariant *value;
			
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
	
	value = g_variant_new_boolean(enabled);
	
	g_dbus_proxy_call(moduledata->modemproxy,
						"SetProperty",
						g_variant_new("(sv)", "Online", value),
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
						"EnterPin",
						g_variant_new("(ss)", "pin", pin),
						G_DBUS_CALL_FLAGS_NONE,
						moduledata->timeouts[MMGUI_DEVICE_OPERATION_UNLOCK],
						moduledata->cancellable,
						(GAsyncReadyCallback)mmgui_module_devices_unlock_with_pin_handler,
						mmguicore);
	
	return TRUE;
}

/*Time format: 2013-10-18T22:22:22+0400*/
static time_t mmgui_module_str_to_time(const gchar *str)
{
	guint i, len, field, fieldlen;
	gint prevptr, curptr; 
	gchar strbuf[5];
	struct tm btime;
	time_t timestamp;
	gint *fields[] = {&btime.tm_year, &btime.tm_mon, &btime.tm_mday,
						&btime.tm_hour,	&btime.tm_min, &btime.tm_sec};
	
	timestamp = time(NULL);
	
	if (str == NULL) return timestamp; 
	
	len = strlen(str);
	
	if (len == 0) return timestamp; 
	
	prevptr = 0;
	curptr = -1;
	field = 0;
	fieldlen = 0;
	
	for (i=0; i<len; i++) {
		if ((str[i] == '-') || (str[i] == 'T') || (str[i] == ':') || (str[i] == '+')) {
			prevptr = curptr+1; /*skip delimiter*/
			curptr = i;
			fieldlen = curptr-prevptr;
			if (fieldlen <= sizeof(strbuf)) {
				strncpy(strbuf, str+prevptr, fieldlen);
				strbuf[fieldlen] = '\0';
				*fields[field] = atoi(strbuf);
			} else {
				*fields[field] = 0;
			}
			/*Go to next field*/
			if (field < (sizeof(fields)/sizeof(gint *))) {
				field++;
			} else {
				break;
			}
		}
	}
	
	if (btime.tm_year > 1900) {
		btime.tm_year -= 1900;
	} 
	btime.tm_mon -= 1;
	timestamp = mktime(&btime);
	
	return timestamp;
}

static mmgui_sms_message_t mmgui_module_sms_retrieve(mmguicore_t mmguicore, GVariant *smsdata)
{
	mmgui_sms_message_t message;
	GVariant *smsmessagev, *smsparamsv, *value;
	gsize strlength;
	const gchar *valuestr;
	gboolean gottext;
	
	if ((mmguicore == NULL) || (smsdata == NULL)) return NULL;
	
	/*Message text and parameters*/
	smsmessagev = g_variant_get_child_value(smsdata, 0);
	smsparamsv = g_variant_get_child_value(smsdata, 1);
	
	if ((smsmessagev == NULL) || (smsparamsv == NULL)) return NULL;
		
	message = mmgui_smsdb_message_create();
	
	/*Message text*/
	gottext = FALSE;
	strlength = 160*256;
	valuestr = g_variant_get_string(smsmessagev, &strlength);
	if ((valuestr != NULL) && (valuestr[0] != '\0')) {
		mmgui_smsdb_message_set_text(message, valuestr, FALSE);
		gottext = TRUE;
	}
	g_variant_unref(smsmessagev);
		
	/*Message has no text - skip it*/
	if (!gottext) {
		mmgui_smsdb_message_free(message);
		return NULL;
	}
		
	/*Message sender*/
	value = g_variant_lookup_value(smsparamsv, "Sender", G_VARIANT_TYPE_STRING);
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
	
	/*Message local time*/
	value = g_variant_lookup_value(smsparamsv, "LocalSentTime", G_VARIANT_TYPE_STRING);
	if (value != NULL) {
		strlength = 256;
		valuestr = g_variant_get_string(value, &strlength);
		if ((valuestr != NULL) && (valuestr[0] != '\0')) {
			mmgui_smsdb_message_set_timestamp(message, mmgui_module_str_to_time(valuestr));
			g_debug("SMS timestamp: %s", ctime((const time_t *)&message->timestamp));
		}
		g_variant_unref(value);
	}
	
	/*Message index (0 since oFono automatically removes messages from modem memory)*/
	mmgui_smsdb_message_set_identifier(message, 0, FALSE);
	
	return message;
}

G_MODULE_EXPORT guint mmgui_module_sms_enum(gpointer mmguicore, GSList **smslist)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	guint msgnum;
	GSList *messages;
		
	if ((mmguicore == NULL) || (smslist == NULL)) return 0;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return 0;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (mmguicorelc->device == NULL) return 0;
	if (moduledata->historyshm == NULL) return 0;
	
	messages = NULL;
	msgnum = 0;
	
	/*Get messages from history storage*/
	messages = mmgui_history_client_enum_messages(moduledata->historyshm);
	if (messages != NULL) {
		msgnum = g_slist_length(messages);
		*smslist = messages;
	}
	
	return msgnum;
}

G_MODULE_EXPORT mmgui_sms_message_t mmgui_module_sms_get(gpointer mmguicore, guint index)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	mmgui_sms_message_t message;
	
	if (mmguicore == NULL) return NULL;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return NULL;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->smsproxy == NULL) return NULL;
	if (mmguicorelc->device == NULL) return NULL;
	if (!mmguicorelc->device->enabled) return NULL;
	if (!(mmguicorelc->device->smscaps & MMGUI_SMS_CAPS_RECEIVE)) return NULL;
	
	/*Message queue is not initialized*/
	if (moduledata->msgqueue == NULL) return NULL;
	/*Message queue is shorter than assumed*/
	if (g_list_length(moduledata->msgqueue) <= index) return NULL;
	
	/*Retrieve message*/
	message = g_list_nth_data(moduledata->msgqueue, index);
	/*And remove it from queue*/
	moduledata->msgqueue = g_list_remove(moduledata->msgqueue, message);
	
	return message;
}

G_MODULE_EXPORT gboolean mmgui_module_sms_delete(gpointer mmguicore, guint index)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
				
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->smsproxy == NULL) return FALSE;
	if (mmguicorelc->device == NULL) return FALSE;
	if (!mmguicorelc->device->enabled) return FALSE;
	if (!(mmguicorelc->device->smscaps & MMGUI_SMS_CAPS_RECEIVE)) return FALSE;
	
	/*Ofono automatically removes messages, so doing nothing there*/
	
	return TRUE;
}

static void mmgui_module_sms_send_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *data;
	gboolean sent;
	const gchar *smspath;
	GDBusProxy *smsproxy;
	GVariant *smsinfo, *smsdict, *smsstatev;
	gsize smsstatesize;
	const gchar *smsstate;
	
	if (user_data == NULL) return;
	mmguicorelc = (mmguicore_t)user_data;
	
	if (mmguicorelc->moduledata == NULL) return;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	error = NULL;
	
	data = g_dbus_proxy_call_finish(proxy, res, &error);
	
	sent = FALSE;
	
	/*Operation result*/
	if ((error != NULL) && (data == NULL)) {
		if ((moduledata->cancellable == NULL) || ((moduledata->cancellable != NULL) && (!g_cancellable_is_cancelled(moduledata->cancellable)))) {
			mmgui_module_handle_error_message(mmguicorelc, error);
		}
		g_error_free(error);
	} else if (data != NULL) {
		/*Get message object path*/
		g_variant_get(data, "(o)", &smspath);
		if (smspath != NULL) {
			/*Message proxy*/
			error = NULL;
			smsproxy = g_dbus_proxy_new_sync(moduledata->connection,
											G_DBUS_PROXY_FLAGS_NONE,
											NULL,
											"org.ofono",
											smspath,
											"org.ofono.Message",
											NULL,
											&error);
			if ((smsproxy == NULL) && (error != NULL)) {
				mmgui_module_handle_error_message(mmguicorelc, error);
				g_error_free(error);
			} else {
				/*Message properties*/
				error = NULL;
				smsinfo = g_dbus_proxy_call_sync(smsproxy,
												"GetProperties",
												NULL,
												0,
												-1,
												NULL,
												&error);
				if ((smsinfo == NULL) && (error != NULL)) {
					mmgui_module_handle_error_message(mmguicorelc, error);
					g_error_free(error);
				} else {
					smsdict = g_variant_get_child_value(smsinfo, 0);
					if (smsdict != NULL) {
						/*Message state*/
						smsstatev = g_variant_lookup_value(smsdict, "State", G_VARIANT_TYPE_STRING);
						if (smsstatev != NULL) {
							smsstatesize = 256;
							smsstate = g_variant_get_string(smsstatev, &smsstatesize);
							if ((smsstate != NULL) && (smsstate[0] != '\0')) {
								/*Message must be already sent or pending*/
								if ((g_str_equal(smsstate, "pending")) || (g_str_equal(smsstate, "sent"))) {
									sent = TRUE;					
								}
							}
							g_variant_unref(smsstatev);
						}
						g_variant_unref(smsdict);
					}
					g_variant_unref(smsinfo);
				}
				g_object_unref(smsproxy);
			}
		}
		g_variant_unref(data);
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
	GError *error;
	GVariant *message;
	GVariantBuilder *messagebuilder;
	
	if ((number == NULL) || (text == NULL)) return FALSE;
			
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->smsproxy == NULL) return FALSE;
	if (mmguicorelc->device == NULL) return FALSE;
	if (!mmguicorelc->device->enabled) return FALSE;
	if (!(mmguicorelc->device->smscaps & MMGUI_SMS_CAPS_SEND)) return FALSE;
	
	mmguicorelc->device->operation = MMGUI_DEVICE_OPERATION_SEND_SMS;
	
	if (moduledata->cancellable != NULL) {
		g_cancellable_reset(moduledata->cancellable);
	}
	
	error = NULL;
	
	/*Set delivery reports state*/
	if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_GSM) {
		g_dbus_proxy_call_sync(moduledata->smsproxy,
								"SetProperty",
								g_variant_new("(sv)", "UseDeliveryReports", g_variant_new_boolean(report)),
								0,
								-1,
								NULL,
								&error);
	} else if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_CDMA) {
		g_dbus_proxy_call_sync(moduledata->smsproxy,
								"SetProperty",
								g_variant_new("(sv)", "UseDeliveryAcknowledgement", g_variant_new_boolean(report)),
								0,
								-1,
								NULL,
								&error);
	}
	/*Save error message if something going wrong*/
	if (error != NULL) {
		mmgui_module_handle_error_message(mmguicore, error);
		g_error_free(error);
	}
	
	/*Form message and send it*/
	if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_GSM) {
		/*In GSM message is simple two string tuple*/
		message = g_variant_new("(ss)", number, text);
		g_dbus_proxy_call(moduledata->smsproxy,
						"SendMessage",
						message,
						G_DBUS_CALL_FLAGS_NONE,
						moduledata->timeouts[MMGUI_DEVICE_OPERATION_SEND_SMS],
						moduledata->cancellable,
						(GAsyncReadyCallback)mmgui_module_sms_send_handler,
						mmguicore);
	} else if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_CDMA) {
		/*In CDMA it is dictionary with some string flags not used here*/
		messagebuilder = g_variant_builder_new(G_VARIANT_TYPE_DICTIONARY);
		g_variant_builder_add(messagebuilder, "{ss}", "To", number);
		g_variant_builder_add(messagebuilder, "{ss}", "Text", text);
		message = g_variant_builder_end(messagebuilder);
		g_dbus_proxy_call(moduledata->smsproxy,
						"SendMessage",
						message,
						G_DBUS_CALL_FLAGS_NONE,
						moduledata->timeouts[MMGUI_DEVICE_OPERATION_SEND_SMS],
						moduledata->cancellable,
						(GAsyncReadyCallback)mmgui_module_sms_send_handler,
						mmguicore);
	}
	
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
	
	session = mmgui_module_proxy_get_property(moduledata->ussdproxy, "State", G_VARIANT_TYPE_STRING);
	
	if (session == NULL) return stateid;
	
	strsize = 256;
	state = (gchar *)g_variant_get_string(session, &strsize);
	
	if ((state != NULL) && (state[0] != '\0')) {
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
	const gchar *restype;
	const gchar *parameter;
	gchar *reqtype, *answer;
	gsize strsize;
	GVariant *data;
		
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
		restype = g_variant_get_type_string(result);
		if (g_str_equal(restype, "(sv)")) {
			/*Initiate*/
			g_variant_get(result, "(sv)", &reqtype, &data);
			if (g_str_equal(reqtype, "USSD")) {
				strsize = 4096;
				parameter = g_variant_get_string(data, &strsize);
				if ((parameter != NULL) && (parameter[0] != '\0')) {
					answer = g_strdup(parameter);
				} else {
					answer = NULL;
				}
			}
			g_variant_unref(data);
		} else if (g_str_equal(restype, "(s)")) {
			/*Response*/
			g_variant_get(result, "(s)", &answer);
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
	
	command = "Initiate";
	
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
	
	/*No problems with USSD in oFono, so just ignore the 'reencode' argument*/
	
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


static guint mmgui_module_network_retrieve(GVariant *networkv, GSList **networks)
{
	mmgui_scanned_network_t network;
	gsize technologies;
	GVariant *dict, *techs, *value;
	gsize strlength;
	guint i, mcc, mnc, mul, num;
	const gchar *parameter;
		
	if ((networkv == NULL) || (networks == NULL)) return 0;
	
	dict = g_variant_get_child_value(networkv, 1);
	
	techs = g_variant_lookup_value(dict, "Technologies", G_VARIANT_TYPE_ARRAY);
	if (techs != NULL) {
		technologies = g_variant_n_children(techs);
	} else {
		technologies = 0;
	}
	
	num = 0;
	
	if (technologies > 0) {
		for (i=0; i<technologies; i++) {
			network = g_new0(struct _mmgui_scanned_network, 1);
			/*Mobile operator code (MCCMNC)*/
			network->operator_num = 0;
			/*MCC*/
			mcc = 0;
			value = g_variant_lookup_value(dict, "MobileCountryCode", G_VARIANT_TYPE_STRING);
			if (value != NULL) {
				strlength = 256;
				parameter = g_variant_get_string(value, &strlength);
				if ((parameter != NULL) && (parameter[0] != '\0')) {
					mcc = atoi(parameter);
				}
				g_variant_unref(value);
			}
			/*MNC*/
			mnc = 0;
			value = g_variant_lookup_value(dict, "MobileNetworkCode", G_VARIANT_TYPE_STRING);
			if (value != NULL) {
				strlength = 256;
				parameter = g_variant_get_string(value, &strlength);
				if ((parameter != NULL) && (parameter[0] != '\0')) {
					mnc = atoi(parameter);
				}
				g_variant_unref(value);
			}
			mul = 1;
			while (mul <= mnc) {
				mul *= 10;
			}
			if (mnc < 10) {
				network->operator_num = mcc * mul * 10 + mnc;
			} else {
				network->operator_num = mcc * mul + mnc;
			}
			/*Network access technology*/
			value = g_variant_get_child_value(techs, i);
			if (value != NULL) {
				strlength = 256;
				parameter = g_variant_get_string(value, &strlength);
				if ((parameter != NULL) && (parameter[0] != '\0')) {
					network->access_tech = mmgui_module_access_technology_translate(parameter);
				} else {
					network->access_tech = MMGUI_ACCESS_TECH_GSM;
				}
				g_variant_unref(value);
			} else {
				network->access_tech = MMGUI_ACCESS_TECH_GSM;
			}
			/*Name of operator*/
			value = g_variant_lookup_value(dict, "Name", G_VARIANT_TYPE_STRING);
			if (value != NULL) {
				strlength = 256;
				parameter = g_variant_get_string(value, &strlength);
				if ((parameter != NULL) && (parameter[0] != '\0')) {
					network->operator_long = g_strdup(parameter);
					network->operator_short = g_strdup(parameter);
				} else {
					network->operator_long = g_strdup(_("Unknown"));
					network->operator_short = g_strdup(_("Unknown"));
				}
				g_variant_unref(value);
			} else {
				network->operator_long = g_strdup(_("Unknown"));
				network->operator_short = g_strdup(_("Unknown"));
			}
			/*Network availability status*/
			value = g_variant_lookup_value(dict, "Status", G_VARIANT_TYPE_STRING);
			if (value != NULL) {
				strlength = 256;
				parameter = g_variant_get_string(value, &strlength);
				if ((parameter != NULL) && (parameter[0] != '\0')) {
					network->status = mmgui_module_network_availability_status_translate(parameter);
				} else {
					network->status = MMGUI_NA_UNKNOWN;
				}
				g_variant_unref(value);
				/*Add network to list*/
				*networks = g_slist_prepend(*networks, network);
				num++;
			} else {
				if (network->operator_long != NULL) g_free(network->operator_long);
				if (network->operator_short != NULL) g_free(network->operator_short);
				g_free(network);
			}
		}
		g_variant_unref(techs);
	}
	
	return num;
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
				mmgui_module_network_retrieve(nnodel2, &networks);
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

G_MODULE_EXPORT guint mmgui_module_contacts_enum(gpointer mmguicore, GSList **contactslist)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *contacts, *contpayload;
	guint contactsnum;
	gsize vcardstrlen;
	const gchar *vcardstr;
				
	if ((mmguicore == NULL) || (contactslist == NULL)) return 0;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return 0;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->contactsproxy == NULL) return 0;
	if (mmguicorelc->device == NULL) return 0;
	//if (!mmguicorelc->device->enabled) return 0;
	if (!(mmguicorelc->device->contactscaps & MMGUI_CONTACTS_CAPS_EXPORT)) return 0;
	
	error = NULL;
	
	contacts = g_dbus_proxy_call_sync(moduledata->contactsproxy,
									"Import",
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
	
	/*Get vcard string*/
	contpayload = g_variant_get_child_value(contacts, 0);
	if (contpayload != NULL) {
		/*Convert to string*/
		vcardstrlen = 16384;
		vcardstr = g_variant_get_string(contpayload, &vcardstrlen);
		if ((vcardstr != NULL) && (vcardstr[0] != '\0')) {
			/*Enumerate contacts*/
			contactsnum = vcard_parse_string(vcardstr, contactslist, "SIM");
		}
		/*Free resources*/
		g_variant_unref(contpayload);
	}
		
	g_variant_unref(contacts);
	
	return contactsnum;
}

G_MODULE_EXPORT gboolean mmgui_module_contacts_delete(gpointer mmguicore, guint index)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
		
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if (moduledata->contactsproxy == NULL) return FALSE;
	if (mmguicorelc->device == NULL) return FALSE;
	if (!mmguicorelc->device->enabled) return FALSE;
	if (!(mmguicorelc->device->contactscaps & MMGUI_CONTACTS_CAPS_EDIT)) return FALSE;
	
	/*oFono can only export contacts, so do nothing here*/
	
	return TRUE;
}

G_MODULE_EXPORT gint mmgui_module_contacts_add(gpointer mmguicore, mmgui_contact_t contact)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	
	if ((mmguicore == NULL) || (contact == NULL)) return -1;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return -1;
	moduledata = (moduledata_t)mmguicorelc->moduledata;
	
	if ((contact->name == NULL) || (contact->number == NULL)) return -1;
	if (moduledata->contactsproxy == NULL) return -1;
	if (mmguicorelc->device == NULL) return -1;
	if (!mmguicorelc->device->enabled) return -1;
	if (!(mmguicorelc->device->contactscaps & MMGUI_CONTACTS_CAPS_EDIT)) return -1;
	
	/*oFono can only export contacts, so do nothing here*/
	
	return 0;
}
