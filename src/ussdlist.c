/*
 *      ussdlist.c
 *      
 *      Copyright 2012 Alex <alex@linuxonly.ru>
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

#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ussdlist.h"
#include "encoding.h"

#define USSDLIST_XML_HEADER "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n\n<ussdlist reencode=\"%u\">"
#define USSDLIST_XML_ENTRY  "\n\t<ussd command=\"%s\" description=\"%s\" />"
#define USSDLIST_XML_FOOTER "\n</ussdlist>" 

struct _ussdlist_user_data {
	ussdlist_read_callback callback;
	gpointer data;
};

typedef struct _ussdlist_user_data *ussdlist_user_data_t;

static GString *xmlstring = NULL;

static const gchar *ussdlist_form_file_path(const gchar *persistentid, const gchar *internalid);
static void ussdlist_xml_get_element(GMarkupParseContext *context, const gchar *element, const gchar **attr_names, const gchar **attr_values, gpointer data, GError **error);


static const gchar *ussdlist_form_file_path(const gchar *persistentid, const gchar *internalid)
{
	const gchar *newfilepath;
	const gchar *newfilename;
	gchar filename[64];
	const gchar *oldfilename;
	
	if (persistentid == NULL) return NULL;
	
	//Form path using XDG standard
	newfilepath = g_build_path(G_DIR_SEPARATOR_S, g_get_user_data_dir(), "modem-manager-gui", "devices", persistentid, NULL);
	
	if (newfilepath == NULL) return NULL;
	
	//If directory structure not exists, create it
	if (!g_file_test(newfilepath, G_FILE_TEST_IS_DIR)) {
		if (g_mkdir_with_parents(newfilepath, S_IRWXU|S_IXGRP|S_IXOTH) == -1) {
			g_warning("Failed to make XDG data directory: %s", newfilepath);
		}
	}
	
	//Form file name
	newfilename = g_build_filename(newfilepath, "ussdlist.xml", NULL);
	
	g_free((gchar *)newfilepath);
	
	if (newfilename == NULL) return NULL;
	
	//If file already exists, just work with it
	if ((g_file_test(newfilename, G_FILE_TEST_EXISTS)) || (internalid == NULL)) {
		return newfilename;
	}
	
	//Form old-style file path
	memset(filename, 0, sizeof(filename));
	g_snprintf(filename, sizeof(filename), "ussd-%s.xml", internalid);
	
	oldfilename = g_build_filename(g_get_home_dir(), ".config", "modem-manager-gui", filename, NULL);	
	
	if (oldfilename == NULL) return newfilename;
	
	//If file exists in old location, move it
	if (g_file_test(oldfilename, G_FILE_TEST_EXISTS)) {
		if (g_rename(oldfilename, newfilename) == -1) {
			g_warning("Failed to move file into XDG data directory: %s -> %s", oldfilename, newfilename);
		}
	}
	
	g_free((gchar *)oldfilename);
	
	return newfilename;
}

gboolean ussdlist_read_commands(ussdlist_read_callback callback, const gchar *persistentid, const gchar *internalid, gpointer data)
{
	const gchar *filepath;
	gchar *contents;
	gsize length;
	GError *error;
	GMarkupParser mp;
	GMarkupParseContext *mpc;
	struct _ussdlist_user_data userdata;
	
	if ((callback == NULL) || (persistentid == NULL)) return FALSE;
	
	filepath = ussdlist_form_file_path(persistentid, internalid);
	
	if (filepath == NULL) return FALSE;
	
	error = NULL;
	
	if (!g_file_get_contents((const gchar *)filepath, &contents, &length, &error)) {
		g_free((gchar *)filepath);
		g_error_free(error);
		return FALSE;
	}
	
	g_free((gchar *)filepath);
	
	mp.start_element = ussdlist_xml_get_element;
	mp.end_element = NULL;
	mp.text = NULL;
	mp.passthrough = NULL;
	mp.error = NULL;
	
	userdata.callback = callback;
	userdata.data = data;
	
	mpc = g_markup_parse_context_new(&mp, 0, (gpointer)&userdata, NULL);
	g_markup_parse_context_parse(mpc, contents, length, &error);
	if (error != NULL) {
		//g_warning(error->message);
		g_error_free(error);
		g_markup_parse_context_free(mpc);
		return FALSE;
	}
	g_markup_parse_context_free(mpc);
	
	return TRUE;
}

static void ussdlist_xml_get_element(GMarkupParseContext *context, const gchar *element, const gchar **attr_names, const gchar **attr_values, gpointer data, GError **error)
{
	gint i;
	gchar *command, *description;
	gboolean reencode;
	ussdlist_user_data_t userdata;
	
	userdata = (ussdlist_user_data_t)data;
	
	if (g_str_equal(element, "ussdlist")) {
		if (g_str_equal(attr_names[0], "reencode")) {
			reencode = (gboolean)atoi(attr_values[0]);
			
			(userdata->callback)(NULL, NULL, reencode, userdata->data);
		}
	} else if (g_str_equal(element, "ussd")) {
		i = 0; command = NULL; description = NULL;
		
		while (attr_names[i] != NULL) {
			if (g_str_equal(attr_names[i], "command")) {
				command = encoding_unescape_xml_markup((const gchar *)attr_values[i], strlen(attr_values[i]));
			} else if (g_str_equal(attr_names[i], "description")) {
				description = encoding_unescape_xml_markup((const gchar *)attr_values[i], strlen(attr_values[i]));
			}
			i++;
		}
		
		if ((command != NULL) && (description != NULL)) {
			(userdata->callback)(command, description, FALSE, userdata->data);
		}
		
		if (command != NULL) g_free(command);
		if (description != NULL) g_free(description);
	}
}

gboolean ussdlist_start_xml_export(gboolean reencode)
{
	if (xmlstring != NULL) {
		g_string_free(xmlstring, TRUE);
		xmlstring = NULL;
	}
	
	xmlstring = g_string_new(NULL);
	g_string_append_printf(xmlstring, USSDLIST_XML_HEADER, reencode);
	
	return TRUE; 
}

gboolean ussdlist_add_command_to_xml_export(gchar *command, gchar *description)
{
	gchar *esccommand, *escdescription;
	
	if (xmlstring == NULL) return FALSE;
	if ((command == NULL) || (description == NULL)) return FALSE;
	
	esccommand = g_markup_escape_text(command, -1);
	escdescription = g_markup_escape_text(description, -1);
	
	if ((esccommand != NULL) && (escdescription != NULL)) {
		g_string_append_printf(xmlstring, USSDLIST_XML_ENTRY, esccommand, escdescription);
		g_free(esccommand);
		g_free(escdescription);
		return TRUE;
	}
	
	if (esccommand != NULL) g_free(esccommand);
	if (escdescription != NULL) g_free(escdescription);
	
	return FALSE;
}

gboolean ussdlist_end_xml_export(const gchar *persistentid)
{
	const gchar *filepath;
	GError *error;
	
	if ((xmlstring == NULL) || (persistentid == NULL)) return FALSE;
	
	g_string_append(xmlstring, USSDLIST_XML_FOOTER);
	
	filepath = ussdlist_form_file_path(persistentid, NULL);
	
	if (filepath == NULL) return FALSE;
	
	error = NULL;
	
	if (!g_file_set_contents((const gchar *)filepath, xmlstring->str, xmlstring->len, &error)) {
		g_free((gchar *)filepath);
		g_error_free(error);
		return FALSE;
	}
	
	g_free((gchar *)filepath);
	
	return TRUE;	
}
