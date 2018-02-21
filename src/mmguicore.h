/*
 *      mmguicore.h
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

#ifndef __MMGUICORE_H__
#define __MMGUICORE_H__

#include <glib.h>
#include <gmodule.h>
#include <net/if.h>

#include "netlink.h"
#include "polkit.h"
#include "svcmanager.h"
#include "smsdb.h"

#define MMGUI_SPEED_VALUES_NUMBER    20

#define MMGUI_THREAD_SLEEP_PERIOD    1 /*seconds*/

enum _mmgui_event {
	/*Device events*/
	MMGUI_EVENT_DEVICE_ADDED = 0,
	MMGUI_EVENT_DEVICE_REMOVED,
	MMGUI_EVENT_DEVICE_OPENED,
	MMGUI_EVENT_DEVICE_CLOSING,
	MMGUI_EVENT_DEVICE_ENABLED_STATUS,
	MMGUI_EVENT_DEVICE_BLOCKED_STATUS,
	MMGUI_EVENT_DEVICE_PREPARED_STATUS,
	MMGUI_EVENT_DEVICE_CONNECTION_STATUS,
	/*Messaging events*/
	MMGUI_EVENT_SMS_LIST_READY,
	MMGUI_EVENT_SMS_COMPLETED,
	MMGUI_EVENT_SMS_SENT,
	MMGUI_EVENT_SMS_REMOVED,
	MMGUI_EVENT_SMS_NEW_DAY,
	/*Network events*/
	MMGUI_EVENT_SIGNAL_LEVEL_CHANGE,
	MMGUI_EVENT_NETWORK_MODE_CHANGE,
	MMGUI_EVENT_NETWORK_REGISTRATION_CHANGE,
	MMGUI_EVENT_LOCATION_CHANGE,
	/*Modem events*/
	MMGUI_EVENT_MODEM_ENABLE_RESULT,
	MMGUI_EVENT_MODEM_UNLOCK_WITH_PIN_RESULT,
	MMGUI_EVENT_MODEM_CONNECTION_RESULT,
	MMGUI_EVENT_SCAN_RESULT,
	MMGUI_EVENT_USSD_RESULT,
	MMGUI_EVENT_NET_STATUS,
	MMGUI_EVENT_TRAFFIC_LIMIT,
	MMGUI_EVENT_TIME_LIMIT,
	MMGUI_EVENT_UPDATE_CONNECTIONS_LIST,
	/*Special-purpose events*/
	MMGUI_EVENT_EXTEND_CAPABILITIES,
	MMGUI_EVENT_SERVICE_ACTIVATION_STARTED,
	MMGUI_EVENT_SERVICE_ACTIVATION_SERVICE_CHANGED,
	MMGUI_EVENT_SERVICE_ACTIVATION_SERVICE_ACTIVATED,
	MMGUI_EVENT_SERVICE_ACTIVATION_SERVICE_ERROR,
	MMGUI_EVENT_SERVICE_ACTIVATION_FINISHED,
	MMGUI_EVENT_SERVICE_ACTIVATION_AUTH_ERROR,
	MMGUI_EVENT_SERVICE_ACTIVATION_STARTUP_ERROR,
	MMGUI_EVENT_SERVICE_ACTIVATION_OTHER_ERROR,
	/*System addressbooks export*/
	MMGUI_EVENT_ADDRESS_BOOKS_EXPORT_FINISHED
};

/*External event callback*/
typedef void (*mmgui_event_ext_callback)(enum _mmgui_event event, gpointer mmguicore, gpointer data, gpointer userdata);
/*Internal event callback*/
typedef void (*mmgui_event_int_callback)(enum _mmgui_event event, gpointer mmguicore, gpointer data);

enum _mmgui_module_priority {
	/*module priority levels*/
	MMGUI_MODULE_PRIORITY_LOW                = 0,
	MMGUI_MODULE_PRIORITY_NORMAL             = 1,
	MMGUI_MODULE_PRIORITY_RECOMMENDED        = 2
};

enum _mmgui_module_type {
	/*module types*/
	MMGUI_MODULE_TYPE_MODEM_MANAGER          = 0,
	MMGUI_MODULE_TYPE_CONNECTION_MANGER      = 1
};

enum _mmgui_module_functions {
	/*module functions bitmask*/
	MMGUI_MODULE_FUNCTION_BASIC              =      0,
	MMGUI_MODULE_FUNCTION_POLKIT_PROTECTION  = 1 << 0
};

enum _mmgui_module_requirement {
	/*module requirements*/
	MMGUI_MODULE_REQUIREMENT_SERVICE = 0x00,
	MMGUI_MODULE_REQUIREMENT_FILE,
	MMGUI_MODULE_REQUIREMENT_NONE
};

struct _mmguimodule {
	/*Module description*/
	guint identifier;
	gboolean applicable;
	gboolean recommended;
	gint type;
	gint requirement;
	gint priority;
	gint functions;
	gint activationtech;
	gchar servicename[256];
	gchar systemdname[256];
	gchar description[256];
	gchar compatibility[256];
	gchar filename[256];
	gchar shortname[256];
};

typedef struct _mmguimodule *mmguimodule_t;

enum _mmgui_device_types {
	MMGUI_DEVICE_TYPE_GSM = 1,
	MMGUI_DEVICE_TYPE_CDMA
};

enum _mmgui_device_modes {
	MMGUI_DEVICE_MODE_UNKNOWN = 0,
	MMGUI_DEVICE_MODE_GSM,
	MMGUI_DEVICE_MODE_GSM_COMPACT,
	MMGUI_DEVICE_MODE_GPRS,
	MMGUI_DEVICE_MODE_EDGE,
	MMGUI_DEVICE_MODE_UMTS,
	MMGUI_DEVICE_MODE_HSDPA,
	MMGUI_DEVICE_MODE_HSUPA,
	MMGUI_DEVICE_MODE_HSPA,
	MMGUI_DEVICE_MODE_HSPA_PLUS,
	MMGUI_DEVICE_MODE_1XRTT,
	MMGUI_DEVICE_MODE_EVDO0,
	MMGUI_DEVICE_MODE_EVDOA,
	MMGUI_DEVICE_MODE_EVDOB,
	MMGUI_DEVICE_MODE_LTE
};

enum _mmgui_network_availability {
	MMGUI_NA_UNKNOWN = 0,
	MMGUI_NA_AVAILABLE,
	MMGUI_NA_CURRENT,
	MMGUI_NA_FORBIDDEN
};

enum _mmgui_access_tech {
	MMGUI_ACCESS_TECH_GSM = 0,
	MMGUI_ACCESS_TECH_GSM_COMPACT,
	MMGUI_ACCESS_TECH_UMTS,
	MMGUI_ACCESS_TECH_EDGE,
	MMGUI_ACCESS_TECH_HSDPA,
	MMGUI_ACCESS_TECH_HSUPA,
	MMGUI_ACCESS_TECH_HSPA,
	MMGUI_ACCESS_TECH_HSPA_PLUS,
    MMGUI_ACCESS_TECH_1XRTT,
    MMGUI_ACCESS_TECH_EVDO0,
    MMGUI_ACCESS_TECH_EVDOA,
    MMGUI_ACCESS_TECH_EVDOB,
    MMGUI_ACCESS_TECH_LTE,
    MMGUI_ACCESS_TECH_UNKNOWN
};

enum _mmgui_ussd_encoding {
	MMGUI_USSD_ENCODING_GSM7 = 0,
	MMGUI_USSD_ENCODING_UCS2
};

enum _mmgui_reg_status {
	MMGUI_REG_STATUS_IDLE = 0,
	MMGUI_REG_STATUS_HOME,
	MMGUI_REG_STATUS_SEARCHING,
	MMGUI_REG_STATUS_DENIED,
	MMGUI_REG_STATUS_UNKNOWN,
	MMGUI_REG_STATUS_ROAMING
};

enum _mmgui_device_state {
	MMGUI_DEVICE_STATE_UNKNOWN       = 0,
    MMGUI_DEVICE_STATE_DISABLED      = 10,
    MMGUI_DEVICE_STATE_DISABLING     = 20,
    MMGUI_DEVICE_STATE_ENABLING      = 30,
    MMGUI_DEVICE_STATE_ENABLED       = 40,
    MMGUI_DEVICE_STATE_SEARCHING     = 50,
    MMGUI_DEVICE_STATE_REGISTERED    = 60,
    MMGUI_DEVICE_STATE_DISCONNECTING = 70,
    MMGUI_DEVICE_STATE_CONNECTING    = 80,
    MMGUI_DEVICE_STATE_CONNECTED     = 90
};

enum _mmgui_lock_type {
	MMGUI_LOCK_TYPE_NONE = 0,
	MMGUI_LOCK_TYPE_PIN,
	MMGUI_LOCK_TYPE_PUK,
	MMGUI_LOCK_TYPE_OTHER
};

enum _mmgui_ussd_state {
	MMGUI_USSD_STATE_UNKNOWN = 0,
	MMGUI_USSD_STATE_IDLE,
	MMGUI_USSD_STATE_ACTIVE,
	MMGUI_USSD_STATE_USER_RESPONSE
};

enum _mmgui_ussd_validation {
	MMGUI_USSD_VALIDATION_INVALID = 0,
	MMGUI_USSD_VALIDATION_REQUEST,
	MMGUI_USSD_VALIDATION_RESPONSE
};

enum _mmgui_contacts_storage {
	MMGUI_CONTACTS_STORAGE_UNKNOWN   = 0,
    MMGUI_MODEM_CONTACTS_STORAGE_ME  = 1,
    MMGUI_MODEM_CONTACTS_STORAGE_SM  = 2,
    MMGUI_MODEM_CONTACTS_STORAGE_MT  = 3,
};

/*Capbilities specifications*/
enum _mmgui_capabilities {
	MMGUI_CAPS_NONE                  = 0,
	MMGUI_CAPS_SMS                   = 1 << 1,
	MMGUI_CAPS_USSD                  = 1 << 2,
	MMGUI_CAPS_LOCATION              = 1 << 3,
	MMGUI_CAPS_SCAN                  = 1 << 4,
	MMGUI_CAPS_CONTACTS              = 1 << 5,
	MMGUI_CAPS_CONNECTIONS           = 1 << 6
};

enum _mmgui_sms_capabilities {
	MMGUI_SMS_CAPS_NONE              = 0,
    MMGUI_SMS_CAPS_RECEIVE           = 1 << 1,
    MMGUI_SMS_CAPS_SEND              = 1 << 2,
    MMGUI_SMS_CAPS_RECEIVE_BROADCAST = 1 << 3
};

enum _mmgui_ussd_capabilities {
	MMGUI_USSD_CAPS_NONE             = 0,
    MMGUI_USSD_CAPS_SEND             = 1 << 1
};

enum _mmgui_location_capabilities {
	MMGUI_LOCATION_CAPS_NONE         = 0,
    MMGUI_LOCATION_CAPS_3GPP         = 1 << 1,
    MMGUI_LOCATION_CAPS_GPS          = 1 << 2
};

enum _mmgui_scan_capabilities {
	MMGUI_SCAN_CAPS_NONE             = 0,
    MMGUI_SCAN_CAPS_OBSERVE          = 1 << 1
};

enum _mmgui_contacts_capabilities {
	MMGUI_CONTACTS_CAPS_NONE         = 0,
    MMGUI_CONTACTS_CAPS_EXPORT       = 1 << 1,
    MMGUI_CONTACTS_CAPS_EDIT         = 1 << 2,
    MMGUI_CONTACTS_CAPS_EXTENDED     = 1 << 3
};

enum _mmgui_connection_manager_caps {
	MMGUI_CONNECTION_MANAGER_CAPS_BASIC = 0,
	MMGUI_CONNECTION_MANAGER_CAPS_MANAGEMENT = 1 << 1,
	MMGUI_CONNECTION_MANAGER_CAPS_MONITORING = 1 << 2
};

enum _mmgui_device_operation {
	MMGUI_DEVICE_OPERATION_IDLE = 0,
	MMGUI_DEVICE_OPERATION_ENABLE,
	MMGUI_DEVICE_OPERATION_UNLOCK,
	MMGUI_DEVICE_OPERATION_SEND_SMS,
	MMGUI_DEVICE_OPERATION_SEND_USSD,
	MMGUI_DEVICE_OPERATION_SCAN,
	MMGUI_DEVICE_OPERATIONS
};

enum _mmgui_device_state_request {
	MMGUI_DEVICE_STATE_REQUEST_ENABLED = 0,
	MMGUI_DEVICE_STATE_REQUEST_LOCKED,
	MMGUI_DEVICE_STATE_REQUEST_REGISTERED,
	MMGUI_DEVICE_STATE_REQUEST_CONNECTED,
	MMGUI_DEVICE_STATE_REQUEST_PREPARED
};

enum _mmgui_event_action {
	MMGUI_EVENT_ACTION_NOTHING = 0,
	MMGUI_EVENT_ACTION_DISCONNECT
};

struct _mmgui_scanned_network {
	enum _mmgui_network_availability status;
	enum _mmgui_access_tech access_tech;
	guint operator_num;
	gchar *operator_long;
	gchar *operator_short;
};

typedef struct _mmgui_scanned_network *mmgui_scanned_network_t;

struct _mmgui_contact {
	guint id;        /*Internal private number*/
	gchar *name;     /*Full name of the contact*/
	gchar *number;   /*Telephone number*/
	gchar *email;    /*Email address*/
	gchar *group;    /*Group this contact belongs to*/
	gchar *name2;    /*Additional contact name*/
	gchar *number2;  /*Additional contact telephone number*/
	gboolean hidden; /*Boolean flag to specify whether this entry is hidden or not*/
	guint storage;   /*Phonebook in which the contact is stored*/
};

typedef struct _mmgui_contact *mmgui_contact_t;

struct _mmgui_traffic_stats {
	gint64 rxbytes;
	gint64 rxpackets;
	gint64 rxerrs;
	gint64 rxdrop;
	gint64 rxfifo;
	gint64 rxframe;
	gint64 rxcompressed;
	gint64 rxmulticast;
	gint64 txbytes;
	gint64 txpackets;
	gint64 txerrs;
	gint64 txdrop;
	gint64 txfifo;
	gint64 txcolls;
	gint64 txcarrier;
	gint64 txcompressed;
};

typedef struct _mmgui_traffic_stats *mmgui_traffic_stats_t;

struct _mmgui_core_options {
	/*Preferred modules*/
	gchar *mmmodule;
	gchar *cmmodule;
	gboolean enableservices;
	/*Timeouts*/
	gint enabletimeout;
	gint sendsmstimeout;
	gint sendussdtimeout;
	gint scannetworkstimeout;
	/*Traffic limits*/
	gboolean trafficenabled;
	gboolean trafficexecuted;
	guint trafficamount;
	guint trafficunits;
	guint64 trafficfull;
	gchar *trafficmessage;
	guint trafficaction;
	gboolean timeenabled;
	gboolean timeexecuted;
	guint timeamount;
	guint timeunits;
	guint64 timefull;
	gchar *timemessage;
	guint timeaction;
};

typedef struct _mmgui_core_options *mmgui_core_options_t;

struct _mmguidevice {
	guint id;
	/*State*/
	gboolean enabled;
	gboolean blocked;
	gboolean registered;
	gboolean prepared;
	enum _mmgui_device_operation operation;
	enum _mmgui_lock_type locktype;
	gboolean conntransition;
	/*Info*/
	gchar *manufacturer;
	gchar *model;
	gchar *version;
	gchar *port;
	gchar *internalid;
	gchar *persistentid;
	gchar *objectpath;
	gchar *sysfspath;
	enum _mmgui_device_types type;
	gchar *imei;
	gchar *imsi;
	gint operatorcode; /*mcc/mnc*/
	gchar *operatorname;
	enum _mmgui_reg_status regstatus;
	guint allmode;
	enum _mmgui_device_modes mode;
	guint siglevel;
	/*Location*/
	guint locationcaps;
	guint loc3gppdata[4]; /*mcc/mnc/lac/ci*/
	gfloat locgpsdata[4]; /*latitude/longitude/altitude/time*/
	/*SMS*/
	guint smscaps;
	gpointer smsdb;
	/*USSD*/
	guint ussdcaps;
	enum _mmgui_ussd_encoding ussdencoding;
	/*Scan*/
	guint scancaps;
	/*Traffic*/
	guint64 rxbytes;
	guint64 txbytes;
	guint64 sessiontime;
	time_t speedchecktime;
	time_t smschecktime;
	gfloat speedvalues[2][MMGUI_SPEED_VALUES_NUMBER];
	guint speedindex;
	gboolean connected;
	gchar interface[IFNAMSIZ];
	time_t sessionstarttime;
	gpointer trafficdb;
	/*Contacts*/
	guint contactscaps;
	GSList *contactslist;
};

typedef struct _mmguidevice *mmguidevice_t;

struct _mmguiconn {
	gchar *uuid;
	gchar *name;
	gchar *number;
	gchar *username;
	gchar *password;
	gchar *apn;
	guint networkid;
	guint type;
	gboolean homeonly;
	gchar *dns1;
	gchar *dns2;
};

typedef struct _mmguiconn *mmguiconn_t;

/*Modem manager module functions*/
typedef gboolean (*mmgui_module_init_func)(mmguimodule_t module);
typedef gboolean (*mmgui_module_open_func)(gpointer mmguicore);
typedef gboolean (*mmgui_module_close_func)(gpointer mmguicore);
typedef gchar *(*mmgui_module_last_error_func)(gpointer mmguicore);
typedef gboolean (*mmgui_module_interrupt_operation_func)(gpointer mmguicore);
typedef gboolean (*mmgui_module_set_timeout_func)(gpointer mmguicore, guint operation, guint timeout);
typedef guint (*mmgui_module_devices_enum_func)(gpointer mmguicore, GSList **devicelist);
typedef gboolean (*mmgui_module_devices_open_func)(gpointer mmguicore, mmguidevice_t device);
typedef gboolean (*mmgui_module_devices_close_func)(gpointer mmguicore);
typedef gboolean (*mmgui_module_devices_state_func)(gpointer mmguicore, enum _mmgui_device_state_request request);
typedef gboolean (*mmgui_module_devices_update_state_func)(gpointer mmguicore);
typedef gboolean (*mmgui_module_devices_information_func)(gpointer mmguicore);
typedef gboolean (*mmgui_module_devices_enable_func)(gpointer mmguicore, gboolean enabled);
typedef gboolean (*mmgui_module_devices_unlock_with_pin_func)(gpointer mmguicore, gchar *pin);
typedef guint (*mmgui_module_sms_enum_func)(gpointer mmguicore, GSList **smslist);
typedef mmgui_sms_message_t (*mmgui_module_sms_get_func)(gpointer mmguicore, guint index);
typedef gboolean (*mmgui_module_sms_delete_func)(gpointer mmguicore, guint index);
typedef gboolean (*mmgui_module_sms_send_func)(gpointer mmguicore, gchar* number, gchar *text, gint validity, gboolean report);
typedef gboolean (*mmgui_module_ussd_cancel_session_func)(gpointer mmguicore);
typedef enum _mmgui_ussd_state (*mmgui_module_ussd_get_state_func)(gpointer mmguicore);
typedef gboolean (*mmgui_module_ussd_send_func)(gpointer mmguicore, gchar *request, enum _mmgui_ussd_validation validationid, gboolean reencode);
typedef gboolean (*mmgui_module_networks_scan_func)(gpointer mmguicore);
typedef guint (*mmgui_module_contacts_enum_func)(gpointer mmguicore, GSList **contactslist);
typedef gboolean (*mmgui_module_contacts_delete_func)(gpointer mmguicore, guint index);
typedef gint (*mmgui_module_contacts_add_func)(gpointer mmguicore, mmgui_contact_t contact);
/*Connection manager module functions*/
typedef gboolean (*mmgui_module_connection_open_func)(gpointer mmguicore);
typedef gboolean (*mmgui_module_connection_close_func)(gpointer mmguicore);
typedef guint (*mmgui_module_connection_enum_func)(gpointer mmguicore, GSList **connlist);
typedef mmguiconn_t (*mmgui_module_connection_add_func)(gpointer mmguicore, const gchar *name, const gchar *number, const gchar *username, const gchar *password, const gchar *apn, guint networkid, guint type, gboolean homeonly, const gchar *dns1, const gchar *dns2);
typedef mmguiconn_t (*mmgui_module_connection_update_func)(gpointer mmguicore, mmguiconn_t connection, const gchar *name, const gchar *number, const gchar *username, const gchar *password, const gchar *apn, guint networkid, gboolean homeonly, const gchar *dns1, const gchar *dns2);
typedef gboolean (*mmgui_module_connection_remove_func)(gpointer mmguicore, mmguiconn_t connection);
typedef gchar *(*mmgui_module_connection_last_error_func)(gpointer mmguicore);
typedef gboolean (*mmgui_module_device_connection_open_func)(gpointer mmguicore, mmguidevice_t device);
typedef gboolean (*mmgui_module_device_connection_close_func)(gpointer mmguicore);
typedef gboolean (*mmgui_module_device_connection_status_func)(gpointer mmguicore);
typedef guint64 (*mmgui_module_device_connection_timestamp_func)(gpointer mmguicore);
typedef gchar *(*mmgui_module_device_connection_get_active_uuid_func)(gpointer mmguicore);
typedef gboolean (*mmgui_module_device_connection_connect_func)(gpointer mmguicore, mmguiconn_t connection);
typedef gboolean (*mmgui_module_device_connection_disconnect_func)(gpointer mmguicore);

struct _mmguicore {
	/*Modules list*/
	GSList *modules;
	GSList *modulepairs;
	/*Cache*/
	gchar *cachefilename;
	GKeyFile *cachekeyfile;
	gboolean updatecache;
	time_t updcachetime;
	/*Modem manager module*/
	GModule *module;
	gpointer moduledata;
	gpointer moduleptr;
	/*Connection manager module*/
	GModule *cmodule;
	gpointer cmoduledata;
	gpointer cmoduleptr;
	/*Modem manager module functions*/
	mmgui_module_open_func open_func;
	mmgui_module_close_func close_func;
	mmgui_module_last_error_func last_error_func;
	mmgui_module_interrupt_operation_func interrupt_operation_func;
	mmgui_module_set_timeout_func set_timeout_func;
	mmgui_module_devices_enum_func devices_enum_func;
	mmgui_module_devices_open_func devices_open_func;
	mmgui_module_devices_close_func devices_close_func;
	mmgui_module_devices_state_func devices_state_func;
	mmgui_module_devices_update_state_func devices_update_state_func;
	mmgui_module_devices_information_func devices_information_func;
	mmgui_module_devices_enable_func devices_enable_func;
	mmgui_module_devices_unlock_with_pin_func devices_unlock_with_pin_func;
	mmgui_module_sms_enum_func sms_enum_func;
	mmgui_module_sms_get_func sms_get_func;
	mmgui_module_sms_delete_func sms_delete_func;
	mmgui_module_sms_send_func sms_send_func;
	mmgui_module_ussd_cancel_session_func ussd_cancel_session_func;
	mmgui_module_ussd_get_state_func ussd_get_state_func;
	mmgui_module_ussd_send_func ussd_send_func;
	mmgui_module_networks_scan_func networks_scan_func;
	mmgui_module_contacts_enum_func contacts_enum_func;
	mmgui_module_contacts_delete_func contacts_delete_func;
	mmgui_module_contacts_add_func contacts_add_func;
	/*Connection manager module functions*/
	mmgui_module_connection_open_func connection_open_func;
	mmgui_module_connection_close_func connection_close_func;
	mmgui_module_connection_enum_func connection_enum_func;
	mmgui_module_connection_add_func connection_add_func;
	mmgui_module_connection_update_func connection_update_func;
	mmgui_module_connection_remove_func connection_remove_func;
	mmgui_module_connection_last_error_func connection_last_error_func;
	mmgui_module_device_connection_open_func device_connection_open_func;
	mmgui_module_device_connection_close_func device_connection_close_func;
	mmgui_module_device_connection_status_func device_connection_status_func;
	mmgui_module_device_connection_timestamp_func device_connection_timestamp_func;
	mmgui_module_device_connection_get_active_uuid_func device_connection_get_active_uuid_func;
	mmgui_module_device_connection_connect_func device_connection_connect_func;
	mmgui_module_device_connection_disconnect_func device_connection_disconnect_func;
	/*Devices*/
	GSList *devices;
	mmguidevice_t device;
	/*Connections*/
	guint cmcaps;
	GSList *connections;
	/*Callbacks*/
	mmgui_event_int_callback eventcb;
	mmgui_event_ext_callback extcb;
	/*Core options*/
	mmgui_core_options_t options;
	/*User data pointer*/
	gpointer userdata;
	/*Netlink interface*/
	mmgui_netlink_t netlink;
	/*Polkit interface*/
	mmgui_polkit_t polkit;
	/*Service manager interface*/
	mmgui_svcmanager_t svcmanager;
	/*New day time*/
	time_t newdaytime;
	/*Work thread*/
	GThread *workthread;
	gint workthreadctl[2];
	#if GLIB_CHECK_VERSION(2,32,0)
		GMutex workthreadmutex;
		GMutex connsyncmutex;
	#else
		GMutex *workthreadmutex;
		GMutex *connsyncmutex;
	#endif
	
};

typedef struct _mmguicore *mmguicore_t;

/*Modules*/
GSList *mmguicore_modules_get_list(mmguicore_t mmguicore);
void mmguicore_modules_mm_set_timeouts(mmguicore_t mmguicore, gint operation1, gint timeout1, ...);
/*Connections*/
gboolean mmguicore_connections_enum(mmguicore_t mmguicore);
GSList *mmguicore_connections_get_list(mmguicore_t mmguicore);
mmguiconn_t mmguicore_connections_add(mmguicore_t mmguicore, const gchar *name, const gchar *number, const gchar *username, const gchar *password, const gchar *apn, guint networkid, guint type, gboolean homeonly, const gchar *dns1, const gchar *dns2);
gboolean mmguicore_connections_update(mmguicore_t mmguicore, const gchar *uuid, const gchar *name, const gchar *number, const gchar *username, const gchar *password, const gchar *apn, guint networkid, gboolean homeonly, const gchar *dns1, const gchar *dns2);
gboolean mmguicore_connections_remove(mmguicore_t mmguicore, const gchar *uuid);
gchar *mmguicore_connections_get_active_uuid(mmguicore_t mmguicore);
gboolean mmguicore_connections_connect(mmguicore_t mmguicore, const gchar *uuid);
gboolean mmguicore_connections_disconnect(mmguicore_t mmguicore);
guint mmguicore_connections_get_capabilities(mmguicore_t mmguicore);
gboolean mmguicore_connections_get_transition_flag(mmguicore_t mmguicore);
/*Devices*/
gboolean mmguicore_devices_enum(mmguicore_t mmguicore);
gboolean mmguicore_devices_open(mmguicore_t mmguicore, guint deviceid, gboolean openfirst);
gboolean mmguicore_devices_enable(mmguicore_t mmguicore, gboolean enabled);
gboolean mmguicore_devices_unlock_with_pin(mmguicore_t mmguicore, gchar *pin);
GSList *mmguicore_devices_get_list(mmguicore_t mmguicore);
mmguidevice_t mmguicore_devices_get_current(mmguicore_t mmguicore);
gboolean mmguicore_devices_get_enabled(mmguicore_t mmguicore);
gboolean mmguicore_devices_get_locked(mmguicore_t mmguicore);
gboolean mmguicore_devices_get_registered(mmguicore_t mmguicore);
gboolean mmguicore_devices_get_prepared(mmguicore_t mmguicore);
gboolean mmguicore_devices_get_connected(mmguicore_t mmguicore);
gint mmguicore_devices_get_lock_type(mmguicore_t mmguicore);
gboolean mmguicore_devices_update_state(mmguicore_t mmguicore);
const gchar *mmguicore_devices_get_identifier(mmguicore_t mmguicore);
const gchar *mmguicore_devices_get_internal_identifier(mmguicore_t mmguicore);
gpointer mmguicore_devices_get_sms_db(mmguicore_t mmguicore);
gpointer mmguicore_devices_get_traffic_db(mmguicore_t mmguicore);
gboolean mmguicore_devices_get_connection_status(mmguicore_t mmguicore);
guint64 mmguicore_devices_get_connection_timestamp(mmguicore_t mmguicore);
guint mmguicore_devices_get_current_operation(mmguicore_t mmguicore);
/*Location*/
guint mmguicore_location_get_capabilities(mmguicore_t mmguicore);
/*SMS*/
guint mmguicore_sms_get_capabilities(mmguicore_t mmguicore);
void mmguicore_sms_message_free(mmgui_sms_message_t message);
GSList *mmguicore_sms_enum(mmguicore_t mmguicore, gboolean concatenation);
mmgui_sms_message_t mmguicore_sms_get(mmguicore_t mmguicore, guint index);
gboolean mmguicore_sms_delete(mmguicore_t mmguicore, guint index);
gboolean mmguicore_sms_validate_number(const gchar *number);
gboolean mmguicore_sms_send(mmguicore_t mmguicore, gchar *number, gchar *text, gint validity, gboolean report);
/*USSD*/
guint mmguicore_ussd_get_capabilities(mmguicore_t mmguicore);
enum _mmgui_ussd_validation mmguicore_ussd_validate_request(gchar *request);
gboolean mmguicore_ussd_cancel_session(mmguicore_t mmguicore);
enum _mmgui_ussd_state mmguicore_ussd_get_state(mmguicore_t mmguicore);
gboolean mmguicore_ussd_send(mmguicore_t mmguicore, gchar *request);
gboolean mmguicore_ussd_set_encoding(mmguicore_t mmguicore, enum _mmgui_ussd_encoding encoding);
enum _mmgui_ussd_encoding mmguicore_ussd_get_encoding(mmguicore_t mmguicore);
/*Scan*/
guint mmguicore_newtworks_scan_get_capabilities(mmguicore_t mmguicore);
void mmguicore_networks_scan_free(GSList *networks);
gboolean mmguicore_networks_scan(mmguicore_t mmguicore);
/*Contacts*/
guint mmguicore_contacts_get_capabilities(mmguicore_t mmguicore);
void mmguicore_contacts_free_single(mmgui_contact_t contact, gboolean freestruct);
GSList *mmguicore_contacts_list(mmguicore_t mmguicore);
mmgui_contact_t mmguicore_contacts_get(mmguicore_t mmguicore, guint index);
gboolean mmguicore_contacts_delete(mmguicore_t mmguicore, guint index);
gboolean mmguicore_contacts_add(mmguicore_t mmguicore, mmgui_contact_t contact);
/*Connections*/
GSList *mmguicore_open_connections_list(mmguicore_t mmguicore);
void mmguicore_close_connections_list(mmguicore_t mmguicore);
GSList *mmguicore_get_connections_changes(mmguicore_t mmguicore);
/*MMGUI Core*/
gchar *mmguicore_get_last_error(mmguicore_t mmguicore);
gchar *mmguicore_get_last_connection_error(mmguicore_t mmguicore);
gboolean mmguicore_interrupt_operation(mmguicore_t mmguicore);
mmguicore_t mmguicore_init(mmgui_event_ext_callback callback, mmgui_core_options_t options, gpointer userdata);
gboolean mmguicore_start(mmguicore_t mmguicore);
void mmguicore_close(mmguicore_t mmguicore);

#endif /* __MMGUICORE_H__ */
