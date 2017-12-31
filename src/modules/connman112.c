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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../mmguicore.h"

#define MMGUI_MODULE_SERVICE_NAME  "net.connman"
#define MMGUI_MODULE_SYSTEMD_NAME  "connman.service"
#define MMGUI_MODULE_IDENTIFIER    112
#define MMGUI_MODULE_DESCRIPTION   "Connman >= 1.12"

/*Internal definitions*/
#define MODULE_INT_CONNMAN_ERROR_CODE_UNKNOWN_METHOD   19
#define MODULE_INT_CONNMAN_UUID_GENERATION_TEMPLATE    "00000000-0000-4000-1000-0000%08x"
#define MODULE_INT_CONNMAN_CONTEXT_PATH_TEMPLATE       "%s/context%u"
#define MODULE_INT_CONNMAN_SERVICE_PATH_TEMPLATE_GSM   "/net/connman/service/cellular_%s_context%u"
#define MODULE_INT_CONNMAN_SERVICE_PATH_TEMPLATE_CDMA  "/net/connman/service/cellular_%s"
#define MODULE_INT_CONNMAN_OPERATION_TIMEOUT           10000

/*Internal enumerations*/

/*Private module variables*/
struct _mmguimoduledata {
	/*DBus connection*/
	GDBusConnection *connection;
	/*DBus proxy objects*/
	GDBusProxy *connmanproxy;
	GDBusProxy *connectionproxy;
	GDBusProxy *ofonoproxy;
	/*Table of context proxies to monitor changes*/
	GHashTable *contexttable;
	/*DBus object paths*/
	gchar *actcontpath;
	/*Signals*/
	gulong connectionsignal;
	/*Internal state flags*/
	gboolean opinitiated;
	gboolean opstate;
	gboolean contextlistformed;
	/*Last error message*/
	gchar *errormessage;
};

typedef struct _mmguimoduledata *moduledata_t;


static void mmgui_module_handle_error_message(mmguicore_t mmguicore, GError *error);
static gchar *mmgui_module_context_path_to_uuid(mmguicore_t mmguicore, const gchar *path);
static gchar *mmgui_module_uuid_to_context_path(mmguicore_t mmguicore, const gchar *uuid);
static gchar *mmgui_module_context_path_to_service_path(mmguicore_t mmguicore, const gchar *path);
static gboolean mmgui_module_device_connection_initialize_contexts(mmguicore_t mmguicore, gboolean createproxies);
static void mmgui_module_device_connection_manager_context_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data);
static void mmgui_module_device_context_property_changed_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data);
static void mmgui_module_device_cdma_connection_manager_context_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data);
static void mmgui_module_device_connection_connect_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data);
static void mmgui_module_device_connection_disconnect_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data);


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

static gchar *mmgui_module_context_path_to_uuid(mmguicore_t mmguicore, const gchar *path)
{
	gchar *constr, *resuuid;
	
	if ((mmguicore == NULL) || (path == NULL)) return NULL;
	if (mmguicore->device == NULL) return NULL;
	
	resuuid = NULL;
	
	if (mmguicore->device->type == MMGUI_DEVICE_TYPE_GSM) {
		constr = g_strrstr(path, "/context");
		if (constr != NULL) {
			resuuid = g_strdup_printf(MODULE_INT_CONNMAN_UUID_GENERATION_TEMPLATE, (guint)atoi(constr + 8));
		}
	} else if (mmguicore->device->type == MMGUI_DEVICE_TYPE_CDMA) {
		resuuid = g_strdup_printf(MODULE_INT_CONNMAN_UUID_GENERATION_TEMPLATE, 0);
	}
	
	return resuuid;
}

static gchar *mmgui_module_uuid_to_context_path(mmguicore_t mmguicore, const gchar *uuid)
{
	gchar *respath;
	guint id;
	
	if ((mmguicore == NULL) || (uuid == NULL)) return NULL;
	if (mmguicore->device == NULL) return NULL;	
	if (mmguicore->device->objectpath == NULL) return NULL;
	
	respath = NULL;
	
	if (mmguicore->device->type == MMGUI_DEVICE_TYPE_GSM) {
		sscanf(uuid, MODULE_INT_CONNMAN_UUID_GENERATION_TEMPLATE, &id);
		respath = g_strdup_printf(MODULE_INT_CONNMAN_CONTEXT_PATH_TEMPLATE, mmguicore->device->objectpath, id);
	}
	
	return respath;
}

static gchar *mmgui_module_context_path_to_service_path(mmguicore_t mmguicore, const gchar *path)
{
	gchar *constr, *deviceid, *respath;
		
	if (mmguicore == NULL) return NULL;
	if (mmguicore->device == NULL) return NULL;
	if ((mmguicore->device->imsi == NULL) && (mmguicore->device->version)) return NULL;
	
	respath = NULL;
	
	if (mmguicore->device->imsi != NULL) {
		deviceid = mmguicore->device->imsi;
	} else {
		deviceid = mmguicore->device->version;
	}
	
	if (mmguicore->device->type == MMGUI_DEVICE_TYPE_GSM) {
		if (path != NULL) {
			constr = g_strrstr(path, "/context");
			if (constr != NULL) {
				respath = g_strdup_printf(MODULE_INT_CONNMAN_SERVICE_PATH_TEMPLATE_GSM, deviceid, (guint)atoi(constr + 8));
			}
		}
	} else if (mmguicore->device->type == MMGUI_DEVICE_TYPE_CDMA) {
		respath = g_strdup_printf(MODULE_INT_CONNMAN_SERVICE_PATH_TEMPLATE_CDMA, deviceid);
	}
	
	return respath;
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
	
	mmguicorelc->cmcaps = MMGUI_CONNECTION_MANAGER_CAPS_BASIC | MMGUI_CONNECTION_MANAGER_CAPS_MANAGEMENT | MMGUI_CONNECTION_MANAGER_CAPS_MONITORING;
	
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
	
	(*moduledata)->actcontpath = NULL;
		
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
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GDBusProxy *serviceproxy;
	const gchar *contextpath;
	gchar *servicepath;
	GVariant *contexts;
	GVariantIter coniterl1, coniterl2;
	GVariant *connodel1, *connodel2;
	GVariant *conparams, *pathv, *parameter, *paramdict;
	GVariant *properties, *propdictv, *dnsarrayv, *dnsvaluev;
	gsize strlength;
	const gchar *valuestr;
	gboolean internet;
	mmguiconn_t connection;
	guint connnum, dnsnum;
		
	if ((mmguicore == NULL) || (connlist == NULL)) return 0;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (!(mmguicorelc->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MANAGEMENT)) return 0;
	
	if (mmguicorelc->cmoduledata == NULL) return 0;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return 0;
	if (moduledata->connectionproxy == NULL) return 0;
	
	connnum = 0;
	
	error = NULL;
	
	if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_GSM) {
		contexts = g_dbus_proxy_call_sync(moduledata->connectionproxy,
											"GetContexts",
											NULL,
											0,
											-1,
											NULL,
											&error);
		
		if (contexts != NULL) {
			g_variant_iter_init(&coniterl1, contexts);
			while ((connodel1 = g_variant_iter_next_value(&coniterl1)) != NULL) {
				g_variant_iter_init(&coniterl2, connodel1);
				while ((connodel2 = g_variant_iter_next_value(&coniterl2)) != NULL) {
					/*Parameters*/
					conparams = g_variant_get_child_value(connodel2, 1);
					if (conparams != NULL) {
						/*Type*/
						internet = FALSE;
						parameter = g_variant_lookup_value(conparams, "Type", G_VARIANT_TYPE_STRING);
						if (parameter != NULL) {
							strlength = 256;
							valuestr = g_variant_get_string(parameter, &strlength);
							if ((valuestr != NULL) && (valuestr[0] != '\0')) {
								internet = g_str_equal(valuestr, "internet");
							}
							g_variant_unref(parameter);
						}
						if (internet) {
							/*Object path*/
							pathv = g_variant_get_child_value(connodel2, 0);
							strlength = 256;
							contextpath = g_variant_get_string(pathv, &strlength);
							if ((contextpath != NULL) && (contextpath[0] != '\0')) {
								/*New connection*/
								connection = g_new0(struct _mmguiconn, 1);
								/*UUID*/
								connection->uuid = mmgui_module_context_path_to_uuid(mmguicore, contextpath);
								/*Name*/
								parameter = g_variant_lookup_value(conparams, "Name", G_VARIANT_TYPE_STRING);
								if (parameter != NULL) {
									strlength = 256;
									valuestr = g_variant_get_string(parameter, &strlength);
									if ((valuestr != NULL) && (valuestr[0] != '\0')) {
										connection->name = g_strdup(valuestr);
									} else {
										connection->name = g_strdup(_("Unknown"));	
									}
									g_variant_unref(parameter);
								} else {
									connection->name = g_strdup(_("Unknown"));		
								}
								/*APN*/
								parameter = g_variant_lookup_value(conparams, "AccessPointName", G_VARIANT_TYPE_STRING);
								if (parameter != NULL) {
									strlength = 256;
									valuestr = g_variant_get_string(parameter, &strlength);
									if ((valuestr != NULL) && (valuestr[0] != '\0')) {
										connection->apn = g_strdup(valuestr);
									} else {
										connection->apn = g_strdup("internet");	
									}
									g_variant_unref(parameter);
								} else {
									connection->apn = g_strdup("internet");
								}
								/*Number*/
								connection->number = g_strdup("*99#");
								/*Username*/
								parameter = g_variant_lookup_value(conparams, "Username", G_VARIANT_TYPE_STRING);
								if (parameter != NULL) {
									strlength = 256;
									valuestr = g_variant_get_string(parameter, &strlength);
									if ((valuestr != NULL) && (valuestr[0] != '\0')) {
										connection->username = g_strdup(valuestr);
									}
									g_variant_unref(parameter);
								}
								/*Password*/
								parameter = g_variant_lookup_value(conparams, "Password", G_VARIANT_TYPE_STRING);
								if (parameter != NULL) {
									strlength = 256;
									valuestr = g_variant_get_string(parameter, &strlength);
									if ((valuestr != NULL) && (valuestr[0] != '\0')) {
										connection->password = g_strdup(valuestr);
									}
									g_variant_unref(parameter);
								}
								/*DNS*/
								servicepath = mmgui_module_context_path_to_service_path(mmguicore, contextpath);
								if (servicepath != NULL) {
									error = NULL;
									serviceproxy = g_dbus_proxy_new_sync(moduledata->connection,
																		G_DBUS_PROXY_FLAGS_NONE,
																		NULL,
																		"net.connman",
																		servicepath,
																		"net.connman.Service",
																		NULL,
																		&error);
							
									if (serviceproxy != NULL) {
										error = NULL;
										properties = g_dbus_proxy_call_sync(serviceproxy,
																			"GetProperties",
																			NULL,
																			0,
																			-1,
																			NULL,
																			&error);
										if (properties != NULL) {
											propdictv = g_variant_get_child_value(properties, 0);
											if (propdictv != NULL) {
												dnsarrayv = g_variant_lookup_value(propdictv, "Nameservers.Configuration", G_VARIANT_TYPE_ARRAY);
												if (dnsarrayv != NULL) {
													dnsnum = g_variant_n_children(dnsarrayv);
													if (dnsnum >= 1) {
														dnsvaluev = g_variant_get_child_value(dnsarrayv, 0);
														if (dnsvaluev != NULL) {
															strlength = 256;
															valuestr = g_variant_get_string(dnsvaluev, &strlength);
															if ((valuestr != NULL) && (valuestr[0] != '\0')) {
																connection->dns1 = g_strdup(valuestr);
															}
															g_variant_unref(dnsvaluev);
														}
													}
													if (dnsnum >= 2) {
														dnsvaluev = g_variant_get_child_value(dnsarrayv, 1);
														if (dnsvaluev != NULL) {
															strlength = 256;
															valuestr = g_variant_get_string(dnsvaluev, &strlength);
															if ((valuestr != NULL) && (valuestr[0] != '\0')) {
																connection->dns2 = g_strdup(valuestr);
															}
															g_variant_unref(dnsvaluev);
														}
													}
													g_variant_unref(dnsarrayv);
												}
												g_variant_unref(propdictv);
											}
											g_variant_unref(properties);
										} /*else if (error != NULL) {
											mmgui_module_handle_error_message(mmguicore, error);
											g_error_free(error);
										}*/
										g_object_unref(serviceproxy);
									} else if (error != NULL) {
										mmgui_module_handle_error_message(mmguicore, error);
										g_error_free(error);
									}
									g_free(servicepath);
								}
								/*Network ID*/
								connection->networkid = 0;
								/*Home only*/
								connection->homeonly = FALSE;
								/*Type*/
								connection->type = MMGUI_DEVICE_TYPE_GSM;
								*connlist = g_slist_prepend(*connlist, connection);
								connnum++;
								/*Free resources*/
								g_variant_unref(pathv);
							}
						}
						g_variant_unref(conparams);
					}
					g_variant_unref(connodel2);
				}
				g_variant_unref(connodel1);
			}
			g_variant_unref(contexts);
		} else if (error != NULL) {
			mmgui_module_handle_error_message(mmguicore, error);
			g_error_free(error);
			return 0;
		}
	} else if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_CDMA) {
		conparams = g_dbus_proxy_call_sync(moduledata->connectionproxy,
											"GetProperties",
											NULL,
											0,
											-1,
											NULL,
											&error);
		if (conparams != NULL) {
			paramdict = g_variant_get_child_value(conparams, 0);
			if (paramdict != NULL) {
				connection = g_new0(struct _mmguiconn, 1);
				/*UUID*/
				connection->uuid = mmgui_module_context_path_to_uuid(mmguicore, NULL);
				/*Name*/
				connection->name = g_strdup(_("CDMA Connection"));
				/*APN*/
				connection->apn = NULL;
				/*Number*/
				connection->number = g_strdup("*777#");
				/*Username*/
				parameter = g_variant_lookup_value(paramdict, "Username", G_VARIANT_TYPE_STRING);
				if (parameter != NULL) {
					strlength = 256;
					valuestr = g_variant_get_string(parameter, &strlength);
					if ((valuestr != NULL) && (valuestr[0] != '\0')) {
						connection->username = g_strdup(valuestr);
					}
					g_variant_unref(parameter);
				}
				/*Password*/
				parameter = g_variant_lookup_value(paramdict, "Password", G_VARIANT_TYPE_STRING);
				if (parameter != NULL) {
					strlength = 256;
					valuestr = g_variant_get_string(parameter, &strlength);
					if ((valuestr != NULL) && (valuestr[0] != '\0')) {
						connection->password = g_strdup(valuestr);
					}
					g_variant_unref(parameter);
				}
				/*DNS*/
				servicepath = mmgui_module_context_path_to_service_path(mmguicore, NULL);
				if (servicepath != NULL) {
					error = NULL;
					serviceproxy = g_dbus_proxy_new_sync(moduledata->connection,
														G_DBUS_PROXY_FLAGS_NONE,
														NULL,
														"net.connman",
														servicepath,
														"net.connman.Service",
														NULL,
														&error);
					
					if (serviceproxy != NULL) {
						error = NULL;
						properties = g_dbus_proxy_call_sync(serviceproxy,
															"GetProperties",
															NULL,
															0,
															-1,
															NULL,
															&error);
						if (properties != NULL) {
							propdictv = g_variant_get_child_value(properties, 0);
							if (propdictv != NULL) {
								dnsarrayv = g_variant_lookup_value(propdictv, "Nameservers.Configuration", G_VARIANT_TYPE_ARRAY);
								if (dnsarrayv != NULL) {
									dnsnum = g_variant_n_children(dnsarrayv);
									if (dnsnum >= 1) {
										dnsvaluev = g_variant_get_child_value(dnsarrayv, 0);
										if (dnsvaluev != NULL) {
											strlength = 256;
											valuestr = g_variant_get_string(dnsvaluev, &strlength);
											if ((valuestr != NULL) && (valuestr[0] != '\0')) {
												connection->dns1 = g_strdup(valuestr);
											}
											g_variant_unref(dnsvaluev);
										}
									}
									if (dnsnum >= 2) {
										dnsvaluev = g_variant_get_child_value(dnsarrayv, 1);
										if (dnsvaluev != NULL) {
											strlength = 256;
											valuestr = g_variant_get_string(dnsvaluev, &strlength);
											if ((valuestr != NULL) && (valuestr[0] != '\0')) {
												connection->dns2 = g_strdup(valuestr);
											}
											g_variant_unref(dnsvaluev);
										}
									}
									g_variant_unref(dnsarrayv);
								}
								g_variant_unref(propdictv);
							}
							g_variant_unref(properties);
						} else if (error != NULL) {
							mmgui_module_handle_error_message(mmguicore, error);
							g_error_free(error);
						}
						g_object_unref(serviceproxy);
					} else if (error != NULL) {
						mmgui_module_handle_error_message(mmguicore, error);
						g_error_free(error);
					}
					g_free(servicepath);
				}
				/*Network ID*/
				connection->networkid = 0;
				/*Home only*/
				connection->homeonly = FALSE;
				/*Type*/
				connection->type = MMGUI_DEVICE_TYPE_CDMA;
				*connlist = g_slist_prepend(*connlist, connection);
				connnum = 1;
				g_variant_unref(paramdict);
			}
			g_variant_unref(conparams);
		} else if (error != NULL) {
			mmgui_module_handle_error_message(mmguicore, error);
			g_error_free(error);
			return 0;
		}
	}
	
	return connnum;
}

G_MODULE_EXPORT mmguiconn_t mmgui_module_connection_add(gpointer mmguicore, const gchar *name, const gchar *number, const gchar *username, const gchar *password, const gchar *apn, guint networkid, guint type, gboolean homeonly, const gchar *dns1, const gchar *dns2)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	mmguiconn_t connection;
	GError *error;
	GVariant *contexttuple, *context;
	gsize strlength;
	const gchar *contextpath;
	gchar *servicepath;
	GDBusProxy *contextproxy, *serviceproxy;
	GVariantBuilder *dnsbuilder;
	
	if ((mmguicore == NULL) || (name == NULL)) return NULL;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (!(mmguicorelc->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MANAGEMENT)) return NULL;
	
	if (mmguicorelc->cmoduledata == NULL) return NULL;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	connection = NULL;
	
	error = NULL;
	
	if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_GSM) {
		contexttuple = g_dbus_proxy_call_sync(moduledata->connectionproxy,
											"AddContext",
											g_variant_new("(s)", "internet"),
											0,
											-1,
											NULL,
											&error);
		
		if (contexttuple != NULL) {
			context = g_variant_get_child_value(contexttuple, 0);
			if (context != NULL) {
				/*Context object path*/
				strlength = 256;
				contextpath = g_variant_get_string(context, &strlength);
				if ((contextpath != NULL) && (contextpath[0] != '\0')) {
					/*Context proxy*/
					contextproxy = g_dbus_proxy_new_sync(moduledata->connection,
														G_DBUS_PROXY_FLAGS_NONE,
														NULL,
														"org.ofono",
														contextpath,
														"org.ofono.ConnectionContext",
														NULL,
														&error);
					
					if (contextproxy != NULL) {
						/*New connection*/
						connection = g_new0(struct _mmguiconn, 1);
						/*Name*/
						error = NULL;
						g_dbus_proxy_call_sync(contextproxy,
												"SetProperty",
												g_variant_new("(sv)", "Name", g_variant_new_string(name)),
												0,
												-1,
												NULL,
												&error);
						if (error != NULL) {
							mmgui_module_handle_error_message(mmguicore, error);
							g_error_free(error);
						}
						connection->name = g_strdup(name);
						/*APN*/
						error = NULL;
						g_dbus_proxy_call_sync(contextproxy,
												"SetProperty",
												g_variant_new("(sv)", "AccessPointName", g_variant_new_string(apn)),
												0,
												-1,
												NULL,
												&error);
						if (error != NULL) {
							mmgui_module_handle_error_message(mmguicore, error);
							g_error_free(error);
						}
						connection->apn = g_strdup(apn);
						/*Username*/
						error = NULL;
						g_dbus_proxy_call_sync(contextproxy,
												"SetProperty",
												g_variant_new("(sv)", "Username", g_variant_new_string(username)),
												0,
												-1,
												NULL,
												&error);
						if (error != NULL) {
							mmgui_module_handle_error_message(mmguicore, error);
							g_error_free(error);
						}
						connection->username = g_strdup(username);
						/*Password*/
						error = NULL;
						g_dbus_proxy_call_sync(contextproxy,
												"SetProperty",
												g_variant_new("(sv)", "Password", g_variant_new_string(password)),
												0,
												-1,
												NULL,
												&error);
						if (error != NULL) {
							mmgui_module_handle_error_message(mmguicore, error);
							g_error_free(error);
						}
						connection->password = g_strdup(password);
						/*DNS*/
						if ((dns1 != NULL) || (dns2 != NULL)) {
							servicepath = mmgui_module_context_path_to_service_path(mmguicore, contextpath);
							if (servicepath != NULL) {
								error = NULL;
								serviceproxy = g_dbus_proxy_new_sync(moduledata->connection,
																	G_DBUS_PROXY_FLAGS_NONE,
																	NULL,
																	"net.connman",
																	servicepath,
																	"net.connman.Service",
																	NULL,
																	&error);
							
								if (serviceproxy != NULL) {
									dnsbuilder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
									if (dns1 != NULL) {
										g_variant_builder_add_value(dnsbuilder, g_variant_new_string(dns1));
									}
									if (dns2 != NULL) {
										g_variant_builder_add_value(dnsbuilder, g_variant_new_string(dns2));
									}
									error = NULL;
									g_dbus_proxy_call_sync(serviceproxy,
															"SetProperty",
															g_variant_new("(sv)", "Nameservers.Configuration", g_variant_builder_end(dnsbuilder)),
															0,
															-1,
															NULL,
															&error);
									if (error != NULL) {
										mmgui_module_handle_error_message(mmguicore, error);
										g_error_free(error);
									}
									g_object_unref(serviceproxy);
								} else if (error != NULL) {
									mmgui_module_handle_error_message(mmguicore, error);
									g_error_free(error);
								}
							}
						}
						connection->dns1 = g_strdup(dns1);
						connection->dns2 = g_strdup(dns2);
						/*Type*/
						connection->type =  MMGUI_DEVICE_TYPE_GSM;
						/*Number*/
						connection->number = g_strdup(number);
						/*Network ID*/
						connection->networkid = networkid;
						/*Home only*/
						connection->homeonly = homeonly;
						/*UUID*/
						connection->uuid = mmgui_module_context_path_to_uuid(mmguicore, contextpath);
						/*Free resources*/
						g_object_unref(contextproxy);
					} else if (error != NULL) {
						mmgui_module_handle_error_message(mmguicore, error);
						g_error_free(error);
					}
				}
			}
			/*Free resources*/
			g_variant_unref(context);
		} else if (error != NULL) {
			mmgui_module_handle_error_message(mmguicore, error);
			g_error_free(error);
		}
	}
	
	return connection;
}

G_MODULE_EXPORT gboolean mmgui_module_connection_update(gpointer mmguicore, mmguiconn_t connection, const gchar *name, const gchar *number, const gchar *username, const gchar *password, const gchar *apn, guint networkid, gboolean homeonly, const gchar *dns1, const gchar *dns2)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	gchar *contextpath, *servicepath;
	GError *error;
	GDBusProxy *contextproxy, *serviceproxy;
	GVariantBuilder *dnsbuilder;
	
	if ((mmguicore == NULL) || (connection == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (!(mmguicorelc->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MANAGEMENT)) return FALSE;
	
	if (mmguicorelc->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
		
	if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_GSM) {
		/*Get context object path from generated UUID*/
		contextpath = mmgui_module_uuid_to_context_path(mmguicore, connection->uuid);
		servicepath = mmgui_module_context_path_to_service_path(mmguicore, contextpath);
		if ((contextpath != NULL) && (servicepath != NULL)) {
			/*Context proxy*/
			contextproxy = g_dbus_proxy_new_sync(moduledata->connection,
												G_DBUS_PROXY_FLAGS_NONE,
												NULL,
												"org.ofono",
												contextpath,
												"org.ofono.ConnectionContext",
												NULL,
												&error);
			/*Free resources*/
			g_free(contextpath);
			/*Update properties*/
			if (contextproxy != NULL) {
				/*Name*/
				error = NULL;
				g_dbus_proxy_call_sync(contextproxy,
										"SetProperty",
										g_variant_new("(sv)", "Name", g_variant_new_string(name)),
										0,
										-1,
										NULL,
										&error);
				if (error != NULL) {
					mmgui_module_handle_error_message(mmguicore, error);
					g_error_free(error);
				}
				if (connection->name != NULL) {
					g_free(connection->name);
				}
				connection->name = g_strdup(name);
				/*APN*/
				error = NULL;
				g_dbus_proxy_call_sync(contextproxy,
										"SetProperty",
										g_variant_new("(sv)", "AccessPointName", g_variant_new_string(apn)),
										0,
										-1,
										NULL,
										&error);
				if (error != NULL) {
					mmgui_module_handle_error_message(mmguicore, error);
					g_error_free(error);
				}
				if (connection->apn != NULL) {
					g_free(connection->apn);
				}
				connection->apn = g_strdup(apn);
				/*Username*/
				error = NULL;
				g_dbus_proxy_call_sync(contextproxy,
										"SetProperty",
										g_variant_new("(sv)", "Username", g_variant_new_string(username)),
										0,
										-1,
										NULL,
										&error);
				if (error != NULL) {
					mmgui_module_handle_error_message(mmguicore, error);
					g_error_free(error);
				}
				if (connection->username != NULL) {
					g_free(connection->username);
				}
				connection->username = g_strdup(username);
				/*Password*/
				error = NULL;
				g_dbus_proxy_call_sync(contextproxy,
										"SetProperty",
										g_variant_new("(sv)", "Password", g_variant_new_string(password)),
										0,
										-1,
										NULL,
										&error);
				if (error != NULL) {
					mmgui_module_handle_error_message(mmguicore, error);
					g_error_free(error);
				}
				if (connection->password != NULL) {
					g_free(connection->password);
				}
				connection->password = g_strdup(password);
				/*DNS*/
				if ((dns1 != NULL) || (dns2 != NULL)) {
					error = NULL;
					serviceproxy = g_dbus_proxy_new_sync(moduledata->connection,
														G_DBUS_PROXY_FLAGS_NONE,
														NULL,
														"net.connman",
														servicepath,
														"net.connman.Service",
														NULL,
														&error);
					if (serviceproxy != NULL) {
						dnsbuilder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
						if (dns1 != NULL) {
							g_variant_builder_add_value(dnsbuilder, g_variant_new_string(dns1));
						}
						if (dns2 != NULL) {
							g_variant_builder_add_value(dnsbuilder, g_variant_new_string(dns2));
						}
						error = NULL;
						g_dbus_proxy_call_sync(serviceproxy,
												"SetProperty",
												g_variant_new("(sv)", "Nameservers.Configuration", g_variant_builder_end(dnsbuilder)),
												0,
												-1,
												NULL,
												&error);
						if (error != NULL) {
							mmgui_module_handle_error_message(mmguicore, error);
							g_error_free(error);
						}
						g_object_unref(serviceproxy);
					} else if (error != NULL) {
						mmgui_module_handle_error_message(mmguicore, error);
						g_error_free(error);
					}
				}
				if (connection->dns1 != NULL) {
					g_free(connection->dns1);
				}
				connection->dns1 = g_strdup(dns1);
				if (connection->dns2 != NULL) {
					g_free(connection->dns2);
				}
				connection->dns2 = g_strdup(dns2);
				/*Number*/
				if (connection->number != NULL) {
					g_free(connection->number);
				}
				connection->number = g_strdup(number);
				/*Network ID*/
				connection->networkid = networkid;
				/*Home only*/
				connection->homeonly = homeonly;
				/*Free resources*/
				g_object_unref(contextproxy);
				g_free(servicepath);
				return TRUE;
			} else if (error != NULL) {
				/*Handle errors*/
				mmgui_module_handle_error_message(mmguicore, error);
				g_error_free(error);
				g_free(servicepath);
			}
		}
	} else if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_CDMA) {
		/*We have single connection in CDMA, so ignore other even if added*/
		if (g_str_equal(connection->uuid, "00000000-0000-4000-1000-000000000000")) {
			/*Name*/
			if (connection->name != NULL) {
				g_free(connection->name);
			}
			connection->name = g_strdup(name);
			/*APN*/
			if (connection->apn != NULL) {
				g_free(connection->apn);
			}
			connection->apn = g_strdup(apn);
			/*Username*/
			error = NULL;
			g_dbus_proxy_call_sync(moduledata->connectionproxy,
									"SetProperty",
									g_variant_new("(sv)", "Username", g_variant_new_string(username)),
									0,
									-1,
									NULL,
									&error);
			if (error != NULL) {
				mmgui_module_handle_error_message(mmguicore, error);
				g_error_free(error);
			}
			if (connection->username != NULL) {
				g_free(connection->username);
			}
			connection->username = g_strdup(username);
			/*Password*/
			error = NULL;
			g_dbus_proxy_call_sync(moduledata->connectionproxy,
									"SetProperty",
									g_variant_new("(sv)", "Password", g_variant_new_string(password)),
									0,
									-1,
									NULL,
									&error);
			if (error != NULL) {
				mmgui_module_handle_error_message(mmguicore, error);
				g_error_free(error);
			}
			if (connection->password != NULL) {
				g_free(connection->password);
			}
			connection->password = g_strdup(password);
			/*DNS*/
			if ((dns1 != NULL) || (dns2 != NULL)) {
				servicepath = mmgui_module_context_path_to_service_path(mmguicore, NULL);
				if (servicepath != NULL) {
					error = NULL;
					serviceproxy = g_dbus_proxy_new_sync(moduledata->connection,
														G_DBUS_PROXY_FLAGS_NONE,
														NULL,
														"net.connman",
														servicepath,
														"net.connman.Service",
														NULL,
														&error);
					if (serviceproxy != NULL) {
						dnsbuilder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
						if (dns1 != NULL) {
							g_variant_builder_add_value(dnsbuilder, g_variant_new_string(dns1));
						}
						if (dns2 != NULL) {
							g_variant_builder_add_value(dnsbuilder, g_variant_new_string(dns2));
						}
						error = NULL;
						g_dbus_proxy_call_sync(serviceproxy,
												"SetProperty",
												g_variant_new("(sv)", "Nameservers.Configuration", g_variant_builder_end(dnsbuilder)),
												0,
												-1,
												NULL,
												&error);
						if (error != NULL) {
							mmgui_module_handle_error_message(mmguicore, error);
							g_error_free(error);
						}
						g_object_unref(serviceproxy);
					} else if (error != NULL) {
						mmgui_module_handle_error_message(mmguicore, error);
						g_error_free(error);
					}
					g_free(servicepath);
				}
			}
			if (connection->dns1 != NULL) {
				g_free(connection->dns1);
			}
			connection->dns1 = g_strdup(dns1);
			if (connection->dns2 != NULL) {
				g_free(connection->dns2);
			}
			connection->dns2 = g_strdup(dns2);
			/*Number*/
			if (connection->number != NULL) {
				g_free(connection->number);
			}
			connection->number = g_strdup(number);
			/*Network ID*/
			connection->networkid = networkid;
			/*Home only*/
			connection->homeonly = homeonly;
			return TRUE;
		}
	}
			
	return FALSE;
}

G_MODULE_EXPORT gboolean mmgui_module_connection_remove(gpointer mmguicore, mmguiconn_t connection)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	gchar *contextpath;
	GError *error;
	
	if ((mmguicore == NULL) || (connection == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (!(mmguicorelc->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MANAGEMENT)) return FALSE;
	
	if (mmguicorelc->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_GSM) {
		/*Get context object path from generated UUID*/
		contextpath = mmgui_module_uuid_to_context_path(mmguicore, connection->uuid);
		if (contextpath != NULL) {
			/*Remove context with single call*/
			error = NULL;
			g_dbus_proxy_call_sync(moduledata->connectionproxy,
									"RemoveContext",
									g_variant_new("(o)", contextpath),
									0,
									-1,
									NULL,
									&error);
			/*Free resources*/
			g_free(contextpath);
			/*Handle errors*/
			if (error != NULL) {
				mmgui_module_handle_error_message(mmguicore, error);
				g_error_free(error);
			} else {
				return TRUE;
			}
		}
	}
		
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

static gboolean mmgui_module_device_connection_initialize_contexts(mmguicore_t mmguicore, gboolean createproxies)
{
	moduledata_t moduledata;
	GError *error;
	GDBusProxy *contextproxy;
	GVariant *contexts;
	GVariantIter coniterl1, coniterl2;
	GVariant *connodel1, *connodel2;
	GVariant *conparams, *paramdict, *contexttypev, *interfacev, *contextpathv, *activeflagv, *settingsarrayv;
	gsize strlength;
	const gchar *contexttype, *contextpath, *interface;
	gboolean foundactive;
			
	if (mmguicore == NULL) return FALSE;
	
	if (mmguicore->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicore->cmoduledata;
	
	if (mmguicore->device == NULL) return FALSE;	
	if (mmguicore->device->objectpath == NULL) return FALSE;
	
	foundactive = FALSE;
	
	error = NULL;
	
	if (mmguicore->device->type == MMGUI_DEVICE_TYPE_GSM) {
		contexts = g_dbus_proxy_call_sync(moduledata->connectionproxy,
											"GetContexts",
											NULL,
											0,
											-1,
											NULL,
											&error);
		if (contexts != NULL) {
			g_variant_iter_init(&coniterl1, contexts);
			while ((connodel1 = g_variant_iter_next_value(&coniterl1)) != NULL) {
				g_variant_iter_init(&coniterl2, connodel1);
				while ((connodel2 = g_variant_iter_next_value(&coniterl2)) != NULL) {
					/*Parameters*/
					conparams = g_variant_get_child_value(connodel2, 1);
					if (conparams != NULL) {
						/*Type*/
						contexttypev = g_variant_lookup_value(conparams, "Type", G_VARIANT_TYPE_STRING);
						if (contexttypev != NULL) {
							strlength = 256;
							contexttype = g_variant_get_string(contexttypev, &strlength);
							if ((contexttype != NULL) && (contexttype[0] != '\0')) {
								if (g_str_equal(contexttype, "internet")) {
									/*Object path*/
									contextpathv = g_variant_get_child_value(connodel2, 0);
									if (contextpathv != NULL) {
										strlength = 256;
										contextpath = g_variant_get_string(contextpathv, &strlength);
										if ((contextpath != NULL) && (contextpath[0] != '\0')) {
											if (createproxies) {
												/*Proxy*/
												contextproxy = g_dbus_proxy_new_sync(moduledata->connection,
																					G_DBUS_PROXY_FLAGS_NONE,
																					NULL,
																					"org.ofono",
																					contextpath,
																					"org.ofono.ConnectionContext",
																					NULL,
																					NULL);
												if (contextproxy != NULL) {
													g_signal_connect(contextproxy, "g-signal", G_CALLBACK(mmgui_module_device_context_property_changed_signal_handler), mmguicore);
													g_hash_table_insert(moduledata->contexttable, g_strdup(contextpath), contextproxy);
												}
											}
											if (!foundactive) {
												/*State*/
												activeflagv = g_variant_lookup_value(conparams, "Active", G_VARIANT_TYPE_BOOLEAN);
												if (activeflagv != NULL) {
													if (g_variant_get_boolean(activeflagv)) {
														settingsarrayv = g_variant_lookup_value(conparams, "Settings", G_VARIANT_TYPE_ARRAY);
														if (settingsarrayv != NULL) {
															/*Interface*/
															interfacev = g_variant_lookup_value(settingsarrayv, "Interface", G_VARIANT_TYPE_STRING);
															if (interfacev != NULL) {
																/*Update interfacename and state*/
																strlength = IFNAMSIZ;
																interface = g_variant_get_string(interfacev, &strlength);
																if ((interface != NULL) && (interface[0] != '\0')) {
																	memset(mmguicore->device->interface, 0, IFNAMSIZ);
																	strncpy(mmguicore->device->interface, interface, IFNAMSIZ);
																	mmguicore->device->connected = TRUE;
																}
																/*Precache active context path*/
																if (moduledata->actcontpath != NULL) {
																	g_free(moduledata->actcontpath);
																}
																moduledata->actcontpath = g_strdup(contextpath);
																/*Set flag*/
																foundactive = TRUE;
																/*Free resources*/
																g_variant_unref(interfacev);
															}
															g_variant_unref(settingsarrayv);
														}
													}
													g_variant_unref(activeflagv);
												}
											}
										}
										g_variant_unref(contextpathv);
									}
								}
							}
							g_variant_unref(contexttypev);
						}
						g_variant_unref(conparams);
					}
					g_variant_unref(connodel2);
				}
				g_variant_unref(connodel1);
			}
			g_variant_unref(contexts);
		} else if (error != NULL) {
			mmgui_module_handle_error_message(mmguicore, error);
			g_error_free(error);
			return FALSE;
		}
	} else if (mmguicore->device->type == MMGUI_DEVICE_TYPE_CDMA) {
		conparams = g_dbus_proxy_call_sync(moduledata->connectionproxy,
											"GetProperties",
											NULL,
											0,
											-1,
											NULL,
											&error);
		if (conparams != NULL) {
			paramdict = g_variant_get_child_value(conparams, 0);
			if (paramdict != NULL) {
				/*State*/
				activeflagv = g_variant_lookup_value(paramdict, "Powered", G_VARIANT_TYPE_BOOLEAN);
				if (activeflagv != NULL) {
					if (g_variant_get_boolean(activeflagv)) {
						settingsarrayv = g_variant_lookup_value(paramdict, "Settings", G_VARIANT_TYPE_ARRAY);
						if (settingsarrayv != NULL) {
							/*Interface*/
							interfacev = g_variant_lookup_value(settingsarrayv, "Interface", G_VARIANT_TYPE_STRING);
							if (interfacev != NULL) {
								/*Update interfacename and state*/
								strlength = IFNAMSIZ;
								interface = g_variant_get_string(interfacev, &strlength);
								if ((interface != NULL) && (interface[0] != '\0')) {
									memset(mmguicore->device->interface, 0, IFNAMSIZ);
									strncpy(mmguicore->device->interface, interface, IFNAMSIZ);
									mmguicore->device->connected = TRUE;
								}
								/*Do not precache active context path - we have single connection*/
								if (moduledata->actcontpath != NULL) {
									g_free(moduledata->actcontpath);
								}
								moduledata->actcontpath = NULL;
								/*Set flag*/
								foundactive = TRUE;
								/*Free resources*/
								g_variant_unref(interfacev);
							}
							g_variant_unref(settingsarrayv);
						}
					}
					g_variant_unref(activeflagv);
				}
				g_variant_unref(paramdict);
			}
			g_variant_unref(conparams);
		}
	}
		
	/*Set internal values if active context wasn't found*/
	if (!foundactive) {
		/*Zero interface name*/
		memset(mmguicore->device->interface, 0, IFNAMSIZ);
		mmguicore->device->connected = FALSE;
		/*Zero active context path*/
		if (moduledata->actcontpath != NULL) {
			g_free(moduledata->actcontpath);
		}
		moduledata->actcontpath = NULL;
	}
	
	return TRUE;
}

static void mmgui_module_device_connection_manager_context_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data)
{
	mmguicore_t mmguicore;
	moduledata_t moduledata;
	GDBusProxy *contextproxy;
	GVariant *propname, *propvalue, *value, *conpath;
	const gchar *parameter, *path;
	gsize strsize;
	
	if (data == NULL) return;
	mmguicore = (mmguicore_t)data;
	
	if (mmguicore->cmoduledata == NULL) return;
	moduledata = (moduledata_t)mmguicore->cmoduledata;
	
	//printf("CONN %s - > %s :: %s\n", sender_name, signal_name, g_variant_print(parameters, TRUE));
	
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
				if (g_str_equal(parameter, "Attached")) {
					if ((g_variant_get_boolean(value)) && (!moduledata->contextlistformed)) {
						moduledata->contextlistformed = mmgui_module_device_connection_initialize_contexts(mmguicore, TRUE);
						/*Contacts capabilities updated*/
						if (mmguicore->eventcb != NULL) {
							(mmguicore->eventcb)(MMGUI_EVENT_EXTEND_CAPABILITIES, mmguicore, GINT_TO_POINTER(MMGUI_CAPS_CONNECTIONS));
						}
					}
				}
				g_variant_unref(value);
			}
		}
	} else if (g_str_equal(signal_name, "ContextAdded")) {
		conpath = g_variant_get_child_value(parameters, 0);
		if (conpath != NULL) {
			strsize = 256;
			path = g_variant_get_string(conpath, &strsize);
			if ((path != NULL) && (path[0] != '\0')) {
				contextproxy = g_dbus_proxy_new_sync(moduledata->connection,
													G_DBUS_PROXY_FLAGS_NONE,
													NULL,
													"org.ofono",
													path,
													"org.ofono.ConnectionContext",
													NULL,
													NULL);
				
				if (contextproxy != NULL) {
					g_signal_connect(contextproxy, "g-signal", G_CALLBACK(mmgui_module_device_context_property_changed_signal_handler), mmguicore);
					g_hash_table_insert(moduledata->contexttable, g_strdup(path), contextproxy);
				}
			}
		}
	} else if (g_str_equal(signal_name, "ContextRemoved")) {
		conpath = g_variant_get_child_value(parameters, 0);
		if (conpath != NULL) {
			strsize = 256;
			path = g_variant_get_string(conpath, &strsize);
			if ((path != NULL) && (path[0] != '\0')) {
				g_hash_table_remove(moduledata->contexttable, path);
			}
		}
	}
	
}

static void mmgui_module_device_context_property_changed_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data)
{
	mmguicore_t mmguicore;
	moduledata_t moduledata;
	GError *error;
	GVariant *propname, *propvalue, *value, *conparams, *paramdictv, *settingsarrayv, *interfacev;
	const gchar *parameter, *interface;
	gsize strsize;
	
	if (data == NULL) return;
	mmguicore = (mmguicore_t)data;
	
	if (mmguicore->cmoduledata == NULL) return;
	moduledata = (moduledata_t)mmguicore->cmoduledata;
	
	if (!moduledata->contextlistformed) return;
	
	//printf("CTX %s - > %s :: %s\n", sender_name, signal_name, g_variant_print(parameters, TRUE));
	
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
				if (g_str_equal(parameter, "Active")) {
					if (g_variant_get_boolean(value)) {
						/*Update connection information*/
						error = NULL;
						conparams = g_dbus_proxy_call_sync(proxy,
															"GetProperties",
															NULL,
															0,
															-1,
															NULL,
															&error);
						if (conparams != NULL) {
							paramdictv = g_variant_get_child_value(conparams, 0);
							if (paramdictv != NULL) {
								/*Settings*/
								settingsarrayv = g_variant_lookup_value(paramdictv, "Settings", G_VARIANT_TYPE_ARRAY);
								if (settingsarrayv != NULL) {
									/*Interface*/
									interfacev = g_variant_lookup_value(settingsarrayv, "Interface", G_VARIANT_TYPE_STRING);
									if (interfacev != NULL) {
										strsize = IFNAMSIZ;
										interface = g_variant_get_string(interfacev, &strsize);
										if ((interface != NULL) && (interface[0] != '\0')) {
											memset(mmguicore->device->interface, 0, IFNAMSIZ);
											strncpy(mmguicore->device->interface, interface, IFNAMSIZ);
											mmguicore->device->connected = TRUE;
										}
										/*Precache active context path*/
										if (moduledata->actcontpath != NULL) {
											g_free(moduledata->actcontpath);
										}
										moduledata->actcontpath = g_strdup(g_dbus_proxy_get_object_path(proxy));
										/*Generate signals*/
										if (moduledata->opinitiated) {
											/*Connection activated by MMGUI*/
											if (mmguicore->eventcb != NULL) {
												(mmguicore->eventcb)(MMGUI_EVENT_MODEM_CONNECTION_RESULT, mmguicore, GUINT_TO_POINTER(moduledata->opstate));
											}
											moduledata->opinitiated = FALSE;
											moduledata->opstate = FALSE;
										} else {
											/*Connection activated by another mean*/
											if (mmguicore->eventcb != NULL) {
												(mmguicore->eventcb)(MMGUI_EVENT_DEVICE_CONNECTION_STATUS, mmguicore, GUINT_TO_POINTER(TRUE));
											}
										}
										/*Free resources*/
										g_variant_unref(interfacev);
									}
									g_variant_unref(settingsarrayv);
								}
								g_variant_unref(paramdictv);
							}
							g_variant_unref(conparams);
						} else if (error != NULL) {
							mmgui_module_handle_error_message(mmguicore, error);
							g_error_free(error);
						}
					} else {
						/*Zero interface name if disconnected*/
						memset(mmguicore->device->interface, 0, IFNAMSIZ);
						mmguicore->device->connected = FALSE;
						/*Zero active context path*/
						if (moduledata->actcontpath != NULL) {
							g_free(moduledata->actcontpath);
						}
						moduledata->actcontpath = NULL;
						/*Generate signals*/
						if (moduledata->opinitiated) {
							/*Connection deactivated by MMGUI*/
							if (mmguicore->eventcb != NULL) {
								(mmguicore->eventcb)(MMGUI_EVENT_MODEM_CONNECTION_RESULT, mmguicore, GUINT_TO_POINTER(moduledata->opstate));
							}
							moduledata->opinitiated = FALSE;
							moduledata->opstate = FALSE;
						} else {
							/*Connection deactivated by another mean*/
							if (mmguicore->eventcb != NULL) {
								(mmguicore->eventcb)(MMGUI_EVENT_DEVICE_CONNECTION_STATUS, mmguicore, GUINT_TO_POINTER(FALSE));
							}
						}
					}
				}
				g_variant_unref(value);
			}
		}
	}
}

static void mmgui_module_device_cdma_connection_manager_context_signal_handler(GDBusProxy *proxy, const gchar *sender_name, const gchar *signal_name, GVariant *parameters, gpointer data)
{
	mmguicore_t mmguicore;
	moduledata_t moduledata;
	GVariant *propname, *propvalue, *value, *conparams, *paramdictv, *settingsarrayv, *interfacev;
	const gchar *parameter, *interface;
	gsize strsize;
	GError *error;
	
	if (data == NULL) return;
	mmguicore = (mmguicore_t)data;
	
	if (mmguicore->cmoduledata == NULL) return;
	moduledata = (moduledata_t)mmguicore->cmoduledata;
	
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
				if (g_str_equal(parameter, "Powered")) {
					if (g_variant_get_boolean(value)) {
						/*Update connection information*/
						error = NULL;
						conparams = g_dbus_proxy_call_sync(proxy,
															"GetProperties",
															NULL,
															0,
															-1,
															NULL,
															&error);
						if (conparams != NULL) {
							paramdictv = g_variant_get_child_value(conparams, 0);
							if (paramdictv != NULL) {
								/*Settings*/
								settingsarrayv = g_variant_lookup_value(paramdictv, "Settings", G_VARIANT_TYPE_ARRAY);
								if (settingsarrayv != NULL) {
									/*Interface*/
									interfacev = g_variant_lookup_value(settingsarrayv, "Interface", G_VARIANT_TYPE_STRING);
									if (interfacev != NULL) {
										strsize = IFNAMSIZ;
										interface = g_variant_get_string(interfacev, &strsize);
										if ((interface != NULL) && (interface[0] != '\0')) {
											memset(mmguicore->device->interface, 0, IFNAMSIZ);
											strncpy(mmguicore->device->interface, interface, IFNAMSIZ);
											mmguicore->device->connected = TRUE;
										}
										/*Precache active context path*/
										if (moduledata->actcontpath != NULL) {
											g_free(moduledata->actcontpath);
										}
										moduledata->actcontpath = NULL;
										/*Generate signals*/
										if (moduledata->opinitiated) {
											/*Connection activated by MMGUI*/
											if (mmguicore->eventcb != NULL) {
												(mmguicore->eventcb)(MMGUI_EVENT_MODEM_CONNECTION_RESULT, mmguicore, GUINT_TO_POINTER(moduledata->opstate));
											}
											moduledata->opinitiated = FALSE;
											moduledata->opstate = FALSE;
										} else {
											/*Connection activated by another mean*/
											if (mmguicore->eventcb != NULL) {
												(mmguicore->eventcb)(MMGUI_EVENT_DEVICE_CONNECTION_STATUS, mmguicore, GUINT_TO_POINTER(TRUE));
											}
										}
										/*Free resources*/
										g_variant_unref(interfacev);
									}
									g_variant_unref(settingsarrayv);
								}
								g_variant_unref(paramdictv);
							}
							g_variant_unref(conparams);
						} else if (error != NULL) {
							mmgui_module_handle_error_message(mmguicore, error);
							g_error_free(error);
						}
					} else {
						/*Zero interface name if disconnected*/
						memset(mmguicore->device->interface, 0, IFNAMSIZ);
						mmguicore->device->connected = FALSE;
						/*Zero active context path*/
						if (moduledata->actcontpath != NULL) {
							g_free(moduledata->actcontpath);
						}
						moduledata->actcontpath = NULL;
						/*Generate signals*/
						if (moduledata->opinitiated) {
							/*Connection deactivated by MMGUI*/
							if (mmguicore->eventcb != NULL) {
								(mmguicore->eventcb)(MMGUI_EVENT_MODEM_CONNECTION_RESULT, mmguicore, GUINT_TO_POINTER(moduledata->opstate));
							}
							moduledata->opinitiated = FALSE;
							moduledata->opstate = FALSE;
						} else {
							/*Connection deactivated by another mean*/
							if (mmguicore->eventcb != NULL) {
								(mmguicore->eventcb)(MMGUI_EVENT_DEVICE_CONNECTION_STATUS, mmguicore, GUINT_TO_POINTER(FALSE));
							}
						}
					}
				}
				g_variant_unref(value);
			}
		}
	}
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_open(gpointer mmguicore, mmguidevice_t device)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	GVariant *conparams, *parameter, *paramdict;
	
				
	if ((mmguicore == NULL) || (device == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (device == NULL) return FALSE;	
	if (device->objectpath == NULL) return FALSE;
	
	moduledata->actcontpath = NULL;
	
	moduledata->opinitiated = FALSE;
	moduledata->opstate = FALSE;
	
	/*Enumerate contexts and add signal handlers*/
	moduledata->contextlistformed = FALSE;
	
	error = NULL;
	
	if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_GSM) {
		/*Hash table of connection context proxies*/
		moduledata->contexttable = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)g_free, (GDestroyNotify)g_object_unref);
		/*Connection manager proxy*/
		moduledata->connectionproxy = g_dbus_proxy_new_sync(moduledata->connection,
															G_DBUS_PROXY_FLAGS_NONE,
															NULL,
															"org.ofono",
															device->objectpath,
															"org.ofono.ConnectionManager",
															NULL,
															&error);
		if (moduledata->connectionproxy != NULL) {
			/*Set signal handler*/
			moduledata->connectionsignal = g_signal_connect(moduledata->connectionproxy, "g-signal", G_CALLBACK(mmgui_module_device_connection_manager_context_signal_handler), mmguicore);
			if (mmguicorelc->device->enabled) {
				/*Test if contexts are available*/
				conparams = g_dbus_proxy_call_sync(moduledata->connectionproxy,
													"GetProperties",
													NULL,
													0,
													-1,
													NULL,
													&error);
				if (conparams != NULL) {
					paramdict = g_variant_get_child_value(conparams, 0);
					if (paramdict != NULL) {
						parameter = g_variant_lookup_value(paramdict, "Attached", G_VARIANT_TYPE_BOOLEAN);
						if (parameter != NULL) {
							if ((g_variant_get_boolean(parameter)) && (!moduledata->contextlistformed)) {
								moduledata->contextlistformed = mmgui_module_device_connection_initialize_contexts(mmguicorelc, TRUE);
							}
							g_variant_unref(parameter);
						}
						g_variant_unref(paramdict);
					}
					g_variant_unref(conparams);
				} else if (error != NULL) {
					mmgui_module_handle_error_message(mmguicorelc, error);
					g_error_free(error);
				}
			}
		} else if (error != NULL) {
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
		}
	} else if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_CDMA) {
		/*Hash table of connection context proxies not needed here*/
		moduledata->contexttable = NULL;
		/*Connection manager proxy*/
		moduledata->connectionproxy = g_dbus_proxy_new_sync(moduledata->connection,
															G_DBUS_PROXY_FLAGS_NONE,
															NULL,
															"org.ofono",
															device->objectpath,
															"org.ofono.cdma.ConnectionManager",
															NULL,
															&error);
		if (moduledata->connectionproxy != NULL) {
			/*Set signal handler*/
			moduledata->connectionsignal = g_signal_connect(moduledata->connectionproxy, "g-signal", G_CALLBACK(mmgui_module_device_cdma_connection_manager_context_signal_handler), mmguicore);
			if (mmguicorelc->device->enabled) {
				/*Update connection state*/
				moduledata->contextlistformed = mmgui_module_device_connection_initialize_contexts(mmguicorelc, FALSE);
			}
		} else if (error != NULL) {
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
		}
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
	
	/*Handle device disconnection*/
	if (moduledata->opinitiated) {
		if (mmguicorelc->eventcb != NULL) {
			(mmguicorelc->eventcb)(MMGUI_EVENT_MODEM_CONNECTION_RESULT, mmguicorelc, GUINT_TO_POINTER(TRUE));
		}
		moduledata->opinitiated = FALSE;
		moduledata->opstate = FALSE;
	}
	
	if (moduledata->connectionproxy != NULL) {
		if (g_signal_handler_is_connected(moduledata->connectionproxy, moduledata->connectionsignal)) {
			g_signal_handler_disconnect(moduledata->connectionproxy, moduledata->connectionsignal);
		}
		g_object_unref(moduledata->connectionproxy);
		moduledata->connectionproxy = NULL;
	}
	
	if (moduledata->contexttable != NULL) {
		g_hash_table_destroy(moduledata->contexttable);
		moduledata->contexttable = NULL;
	}
	
	if (moduledata->actcontpath != NULL) {
		g_free(moduledata->actcontpath);
		moduledata->actcontpath = NULL;
	}
		
	return TRUE;
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_status(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	
	if (mmguicorelc->device == NULL) return FALSE;
	if (mmguicorelc->device->objectpath == NULL) return FALSE; 
	
	/*Get interface information*/
	mmgui_module_device_connection_initialize_contexts(mmguicorelc, FALSE);
		
	return TRUE;
}

G_MODULE_EXPORT guint64 mmgui_module_device_connection_timestamp(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	guint64 timestamp;
	
	if (mmguicore == NULL) return 0;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->device == NULL) return 0;
	if (mmguicorelc->device->objectpath == NULL) return 0;
	
	/*Get current timestamp*/
	timestamp = (guint64)time(NULL);
	
	return timestamp;
}

G_MODULE_EXPORT gchar *mmgui_module_device_connection_get_active_uuid(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	
	if (mmguicore == NULL) return NULL;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->cmoduledata == NULL) return NULL;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	if (mmguicorelc->device == NULL) return NULL;
	if (moduledata->actcontpath == NULL) return NULL;
	
	return mmgui_module_context_path_to_uuid(mmguicore, moduledata->actcontpath);
}

static void mmgui_module_device_connection_connect_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
			
	mmguicorelc = (mmguicore_t)user_data;
	if (mmguicorelc == NULL) return;
	
	if (mmguicorelc->cmoduledata == NULL) return;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	error = NULL;
	
	g_dbus_proxy_call_finish(proxy, res, &error);
	
	if (error != NULL) {
		/*Reset flags*/
		moduledata->opinitiated = FALSE;
		moduledata->opstate = FALSE;
		/*Emit signal*/
		if (mmguicorelc->eventcb != NULL) {
			(mmguicorelc->eventcb)(MMGUI_EVENT_MODEM_CONNECTION_RESULT, mmguicorelc, GUINT_TO_POINTER(FALSE));
		}
		/*Handle errors*/
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
	}
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_connect(gpointer mmguicore, mmguiconn_t connection)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	gchar *contextpath;
	GDBusProxy *contextproxy;
		
	if ((mmguicore == NULL) || (connection == NULL)) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (!(mmguicorelc->cmcaps & MMGUI_CONNECTION_MANAGER_CAPS_MANAGEMENT)) return FALSE;
	
	if (mmguicorelc->cmoduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
		
	if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_GSM) {
		if (moduledata->actcontpath == NULL) {
			/*Get context object path from generated UUID*/
			contextpath = mmgui_module_uuid_to_context_path(mmguicore, connection->uuid);
			if (contextpath != NULL) {
				contextproxy = g_hash_table_lookup(moduledata->contexttable, contextpath);
				if (contextproxy != NULL) {
					/*Change 'Active' property*/
					g_dbus_proxy_call(contextproxy,
									"SetProperty",
									g_variant_new("(sv)", "Active", g_variant_new_boolean(TRUE)),
									G_DBUS_CALL_FLAGS_NONE,
									MODULE_INT_CONNMAN_OPERATION_TIMEOUT,
									NULL,
									(GAsyncReadyCallback)mmgui_module_device_connection_connect_handler,
									mmguicore);
					/*Set flags*/
					moduledata->opinitiated = TRUE;
					moduledata->opstate = TRUE;
				}
				g_free(contextpath);
			}
		}
	} else if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_CDMA) {
		/*Change 'Powered' property*/
		g_dbus_proxy_call(moduledata->connectionproxy,
						"SetProperty",
						g_variant_new("(sv)", "Powered", g_variant_new_boolean(TRUE)),
						G_DBUS_CALL_FLAGS_NONE,
						MODULE_INT_CONNMAN_OPERATION_TIMEOUT,
						NULL,
						(GAsyncReadyCallback)mmgui_module_device_connection_connect_handler,
						mmguicore);
		/*Set flags*/
		moduledata->opinitiated = TRUE;
		moduledata->opstate = TRUE;
	}
		
	return TRUE;
}

static void mmgui_module_device_connection_disconnect_handler(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
			
	mmguicorelc = (mmguicore_t)user_data;
	if (mmguicorelc == NULL) return;
	
	if (mmguicorelc->cmoduledata == NULL) return;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
	
	error = NULL;
	
	g_dbus_proxy_call_finish(proxy, res, &error);
	
	if (error != NULL) {
		/*Reset flags*/
		moduledata->opinitiated = FALSE;
		moduledata->opstate = FALSE;
		/*Emit signal*/
		if (mmguicorelc->eventcb != NULL) {
			(mmguicorelc->eventcb)(MMGUI_EVENT_MODEM_CONNECTION_RESULT, mmguicorelc, GUINT_TO_POINTER(FALSE));
		}
		/*Handle errors*/
		mmgui_module_handle_error_message(mmguicorelc, error);
		g_error_free(error);
	}
}

G_MODULE_EXPORT gboolean mmgui_module_device_connection_disconnect(gpointer mmguicore)
{
	mmguicore_t mmguicorelc;
	moduledata_t moduledata;
	GError *error;
	gchar *actsvcpath;
	GDBusProxy *svcproxy;
		
	if (mmguicore == NULL) return FALSE;
	mmguicorelc = (mmguicore_t)mmguicore;
	
	if (mmguicorelc->moduledata == NULL) return FALSE;
	moduledata = (moduledata_t)mmguicorelc->cmoduledata;
		
	if (mmguicorelc->device == NULL) return FALSE;
	if (mmguicorelc->device->imsi == NULL) return FALSE;
	if (moduledata->actcontpath == NULL) return FALSE;
	
	/*If device already disconnected, return TRUE*/
	if (!mmguicorelc->device->connected) return TRUE;
	
	/*Get active service path*/
	if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_GSM) {
		actsvcpath = mmgui_module_context_path_to_service_path(mmguicorelc, moduledata->actcontpath);
	} else if (mmguicorelc->device->type == MMGUI_DEVICE_TYPE_CDMA) {
		actsvcpath = mmgui_module_context_path_to_service_path(mmguicorelc, NULL);
	} else {
		actsvcpath = NULL;
	}
	
	if (actsvcpath != NULL) {
		/*Service proxy*/
		error = NULL;
		svcproxy = g_dbus_proxy_new_sync(moduledata->connection,
										G_DBUS_PROXY_FLAGS_NONE,
										NULL,
										"net.connman",
										actsvcpath,
										"net.connman.Service",
										NULL,
										&error);
	
		if ((svcproxy == NULL) && (error != NULL)) {
			mmgui_module_handle_error_message(mmguicorelc, error);
			g_error_free(error);
			g_free(actsvcpath);
			return FALSE;
		}
	
		g_free(actsvcpath);
	
		/*Call disconnect method*/
		g_dbus_proxy_call(svcproxy,
						"Disconnect",
						NULL,
						G_DBUS_CALL_FLAGS_NONE,
						MODULE_INT_CONNMAN_OPERATION_TIMEOUT,
						NULL,
						(GAsyncReadyCallback)mmgui_module_device_connection_disconnect_handler,
						mmguicore);
	
		/*Set flags*/
		moduledata->opinitiated = TRUE;
		moduledata->opstate = TRUE;
	
		/*Free resources*/
		g_object_unref(svcproxy);
	
		return TRUE;
	}
		
	return FALSE;	
}
