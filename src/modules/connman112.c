/*
 *      connman112.c
 *      
 *      Copyright 2014-2017 Alex <alex@linuxonly.ru>
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

#define MMGUI_MODULE_SERVICE_NAME  "net.connman"
#define MMGUI_MODULE_SYSTEMD_NAME  "connman.service"
#define MMGUI_MODULE_IDENTIFIER    112
#define MMGUI_MODULE_DESCRIPTION   "Connman >= 1.12"

//Internal definitions
#define MODULE_INT_PPPD_LOCK_FILE_PATH          "/var/run/%s.pid"

//Internal enumerations

//Private module variables
struct _mmguimoduledata {
	//DBus connection
	GDBusConnection *connection;
	//DBus proxy objects
	GDBusProxy *connmanproxy;
	GDBusProxy *ofonoproxy;
	//Dbus object paths
	gchar *cnsvcpath;
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

static gboolean mmgui_module_get_network_interface(mmguicore_t mmguicorelc, gchar *intname, gsize intnamesize)
{
	moduledata_t moduledata;
	GError *error;
	GDBusProxy *ofonoconnproxy;
	GVariant *ofonoconns;
	GVariantIter ofonoconniterl1, ofonoconniterl2;
	GVariant *ofonoconnnodel1, *ofonoconnnodel2;
	GVariant *ofonoconnparams;
	gsize strlength;
	const gchar *valuestr;
	GVariant *parameters, *parameter;
	gboolean connstate, conninttype;
	
	if ((mmguicorelc == NULL) || (intname == NULL) || (intnamesize == 0)) return FALSE;
		
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (mmguicorelc->device->objectpath == NULL) return FALSE; 
	
	/*Initial state*/	
	conninttype = FALSE;
	connstate = FALSE;
		
	/*oFono connection proxy*/
	error = NULL;
	ofonoconnproxy = g_dbus_proxy_new_sync(moduledata->connection,
										G_DBUS_PROXY_FLAGS_NONE,
										NULL,
										"org.ofono",
										mmguicorelc->device->objectpath,
										"org.ofono.ConnectionManager",
										NULL,
										&error);
	
	if ((ofonoconnproxy != NULL) && (error == NULL)) {
		/*Connection contexts*/
		error = NULL;
		ofonoconns = g_dbus_proxy_call_sync(ofonoconnproxy,
										"GetContexts",
										NULL,
										0,
										-1,
										NULL,
										&error);
		
		if ((ofonoconns != NULL) && (error == NULL)) {
			g_variant_iter_init(&ofonoconniterl1, ofonoconns);
			while ((ofonoconnnodel1 = g_variant_iter_next_value(&ofonoconniterl1)) != NULL) {
				g_variant_iter_init(&ofonoconniterl2, ofonoconnnodel1);
				while ((ofonoconnnodel2 = g_variant_iter_next_value(&ofonoconniterl2)) != NULL) {
					/*Parameters*/
					ofonoconnparams = g_variant_get_child_value(ofonoconnnodel2, 1);
					if (ofonoconnparams != NULL) {
						/*Type*/
						parameter = g_variant_lookup_value(ofonoconnparams, "Type", G_VARIANT_TYPE_STRING);
						if (parameter != NULL) {
							strlength = 256;
							valuestr = g_variant_get_string(parameter, &strlength);
							conninttype = g_str_equal(valuestr, "internet");
							g_variant_unref(parameter);
						}
						/*State*/
						parameter = g_variant_lookup_value(ofonoconnparams, "Active", G_VARIANT_TYPE_BOOLEAN);
						if (parameter != NULL) {
							connstate = g_variant_get_boolean(parameter);
							g_variant_unref(parameter);
						}
						/*Set network interface*/
						if ((conninttype) && (connstate)) {
							/*Parameters*/
							parameters = g_variant_lookup_value(ofonoconnparams, "Settings", G_VARIANT_TYPE_ARRAY);
							if (parameters != NULL) {
								/*Interface*/
								parameter = g_variant_lookup_value(parameters, "Interface", G_VARIANT_TYPE_STRING);
								if (parameter != NULL) {
									strlength = 256;
									valuestr = g_variant_get_string(parameter, &strlength);
									/*Save interface name if connected*/
									memset(intname, 0, intnamesize);
									strncpy(intname, valuestr, intnamesize);
									g_variant_unref(parameter);
								}
								g_variant_unref(parameters);
							}
						} else {
							/*Zero interface name if disconnected*/
							memset(intname, 0, intnamesize);
						}
						g_variant_unref(ofonoconnparams);
					}
					g_variant_unref(ofonoconnnodel2);
				}
				g_variant_unref(ofonoconnnodel1);
			}
			g_variant_unref(ofonoconns);
		} else if ((ofonoconns == NULL) && (error != NULL)) {
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
		}
		g_object_unref(ofonoconnproxy);
	} else if ((ofonoconnproxy != NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
	}
	
	if ((conninttype) && (connstate)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

static gchar *mmgui_module_get_service_name(mmguicore_t mmguicorelc)
{
	moduledata_t moduledata;
	GError *error;
	GVariant *cnsvcs;
	GVariantIter cnsvciterl1, cnsvciterl2;
	GVariant *cnsvcnodel1, *cnsvcnodel2;
	GVariant *cnsvcid, *cnsvcparams;
	gsize strlength;
	const gchar *valuestr;
	GVariant *parameter, *parameters;
	gchar *svcname;
	gboolean isonline, iscelluar;
	
	if (mmguicorelc == NULL) return NULL;
	
	if (mmguicorelc->cmoduledata == NULL) return NULL;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return NULL;
	if (mmguicorelc->device->objectpath == NULL) return NULL; 
	
	svcname = NULL;
	
	error = NULL;	
		
	//Connman interface
	if (moduledata->connmanproxy != NULL) {
		cnsvcs = g_dbus_proxy_call_sync(moduledata->connmanproxy,
										"GetServices",
										NULL,
										0,
										-1,
										NULL,
										&error);
		
		if ((cnsvcs != NULL) && (error == NULL)) {
			g_variant_iter_init(&cnsvciterl1, cnsvcs);
			while ((cnsvcnodel1 = g_variant_iter_next_value(&cnsvciterl1)) != NULL) {
				g_variant_iter_init(&cnsvciterl2, cnsvcnodel1);
				while ((cnsvcnodel2 = g_variant_iter_next_value(&cnsvciterl2)) != NULL) {
					isonline = FALSE;
					iscelluar = FALSE;
					/*Parameters*/
					cnsvcparams = g_variant_get_child_value(cnsvcnodel2, 1);
					if (cnsvcparams != NULL) {
						/*State*/
						parameter = g_variant_lookup_value(cnsvcparams, "State", G_VARIANT_TYPE_STRING);
						if (parameter != NULL) {
							strlength = 256;
							valuestr = g_variant_get_string(parameter, &strlength);
							isonline = g_str_equal(valuestr, "online");
							g_variant_unref(parameter);
						}
						/*Type*/
						parameter = g_variant_lookup_value(cnsvcparams, "Type", G_VARIANT_TYPE_STRING);
						if (parameter != NULL) {
							strlength = 256;
							valuestr = g_variant_get_string(parameter, &strlength);
							iscelluar = g_str_equal(valuestr, "cellular");
							g_variant_unref(parameter);
						}
						
						if ((isonline) && (iscelluar)) {
							/*Parameters*/
							parameters = g_variant_lookup_value(cnsvcparams, "Ethernet", G_VARIANT_TYPE_ARRAY);
							if (parameters != NULL) {
								/*Interface*/
								parameter = g_variant_lookup_value(parameters, "Interface", G_VARIANT_TYPE_STRING);
								if (parameter != NULL) {
									strlength = 256;
									valuestr = g_variant_get_string(parameter, &strlength);
									if (g_str_equal(valuestr, mmguicorelc->device->interface)) {
										/*Save service identifier*/
										cnsvcid = g_variant_get_child_value(cnsvcnodel2, 0);
										if (cnsvcid != NULL) {
											/*Identifier*/
											strlength = 256;
											valuestr = g_variant_get_string(cnsvcid, &strlength);
											svcname = g_strdup(valuestr);
											g_variant_unref(cnsvcid);
										}
									}
									g_variant_unref(parameter);
								}
								g_variant_unref(parameters);
							}
						}
						g_variant_unref(cnsvcparams);
					}
				}
				g_variant_unref(cnsvcnodel1);
			}
			g_variant_unref(cnsvcs);
		} else if ((cnsvcs == NULL) && (error != NULL)) {
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
			return NULL;
		}
	}
	
	return svcname;
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
	
	mmguicorelc->cmcaps = MMGUI_CONNECTION_MANAGER_CAPS_BASIC;
	
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
	
	(*moduledata)->connmanproxy = g_dbus_proxy_new_sync((*moduledata)->connection,
													G_DBUS_PROXY_FLAGS_NONE,
													NULL,
													"net.connman",
													"/",
													"net.connman.Manager",
													NULL,
													&error);
		
	if (((*moduledata)->connmanproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_object_unref((*moduledata)->connection);
		g_free(mmguicorelc->cmoduledata);
		return FALSE;
	}
	
	(*moduledata)->cnsvcpath = NULL;
		
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
		
		if (moduledata->connmanproxy != NULL) {
			g_object_unref(moduledata->connmanproxy);
			moduledata->connmanproxy = NULL;
		}
		
		if (moduledata->connection != NULL) {
			g_object_unref(moduledata->connection);
			moduledata->connection = NULL;
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
	
	if (device == NULL) return FALSE;	
	if (device->objectpath == NULL) return FALSE;
	
	moduledata->cnsvcpath = NULL;
	
	/*Nothing to precache there*/
	
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
	
	if (moduledata->cnsvcpath != NULL) {
		g_free(moduledata->cnsvcpath);
		moduledata->cnsvcpath = NULL;
	}
		
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_status(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (mmguicorelc->device->objectpath == NULL) return FALSE; 
	
	/*Get interface information*/
	mmguicorelc->device->connected = mmgui_module_get_network_interface(mmguicorelc, (gchar *)&mmguicorelc->device->interface, IFNAMSIZ);
	
	/*Precache service path for future disconnect*/
	if (mmguicorelc->device->connected) {
		if (moduledata->cnsvcpath != NULL) {
			g_free(moduledata->cnsvcpath);
		}
		moduledata->cnsvcpath = mmgui_module_get_service_name(mmguicorelc);
	}
	
	return TRUE;
}

G_MODULE_EXPORT guint64 mmgui_module_device_connection_timestamp(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	gchar intname[IFNAMSIZ];
	gchar lockfilepath[128];
	guint64 timestamp;
	struct stat statbuf;
	
	if (mmguicore == NULL) return 0;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->device == NULL) return 0;
	if (mmguicorelc->device->objectpath == NULL) return 0;
	
	/*Get current timestamp*/
	timestamp = (guint64)time(NULL);
	
	if (mmgui_module_get_network_interface(mmguicorelc, (gchar *)&intname, IFNAMSIZ)) {
		/*Form lock file path*/
		memset(lockfilepath, 0, sizeof(lockfilepath));
		g_snprintf(lockfilepath, sizeof(lockfilepath), MODULE_INT_PPPD_LOCK_FILE_PATH, intname);
		/*Get lock file modification timestamp*/
		if (stat(lockfilepath, &statbuf) == 0) {
			timestamp = (guint64)statbuf.st_mtime;
		}
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
	GDBusProxy *cnsvcproxy;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (moduledata->cnsvcpath == NULL) return FALSE;
	
	/*If device already disconnected, return TRUE*/
	if (!mmguicorelc->device->connected) return TRUE;
	
	/*Service proxy*/
	error = NULL;
	cnsvcproxy = g_dbus_proxy_new_sync(moduledata->connection,
										G_DBUS_PROXY_FLAGS_NONE,
										NULL,
										"net.connman",
										moduledata->cnsvcpath,
										"net.connman.Service",
										NULL,
										&error);
	
	if ((cnsvcproxy == NULL) && (error != NULL)) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		return FALSE;
	}
	
	//Call disconnect method
	g_dbus_proxy_call_sync(cnsvcproxy, "Disconnect", NULL, 0, -1, NULL, &error);
	
	if (error != NULL) {
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
		g_object_unref(cnsvcproxy);
		return FALSE;
	}
	
	g_object_unref(cnsvcproxy);
	
	//Update device state
	mmguicorelc->device->connected = FALSE;
	
	return TRUE;	
}
