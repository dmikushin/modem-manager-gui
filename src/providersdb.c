/*
 *      providersdb.c
 *      
 *      Copyright 2014 Alex <alex@linuxonly.ru>
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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "providersdb.h"
#include "mmguicore.h"
#include "../resources.h"

enum _mmgui_providers_db_params {
	MMGUI_PROVIDERS_DB_PARAM_COUNTRY = 0,
	MMGUI_PROVIDERS_DB_PARAM_PROVIDER,
	MMGUI_PROVIDERS_DB_PARAM_NAME,
	MMGUI_PROVIDERS_DB_PARAM_GSM,
	MMGUI_PROVIDERS_DB_PARAM_CDMA,
	MMGUI_PROVIDERS_DB_PARAM_NETWORK_ID,
	MMGUI_PROVIDERS_DB_PARAM_SID,
	MMGUI_PROVIDERS_DB_PARAM_APN,
	MMGUI_PROVIDERS_DB_PARAM_USAGE,
	MMGUI_PROVIDERS_DB_PARAM_USERNAME,
	MMGUI_PROVIDERS_DB_PARAM_PASSWORD,
	MMGUI_PROVIDERS_DB_PARAM_DNS,
	MMGUI_PROVIDERS_DB_PARAM_NULL
};

/*enum _mmgui_providers_db_tech {
	MMGUI_PROVIDERS_DB_TECH_GSM = 0,
	MMGUI_PROVIDERS_DB_TECH_CDMA
};*/

const gchar *mmgui_providersdb_countries[250][2] = {
	{"Afghanistan", "af"},
	{"Åland Islands", "ax"},
	{"Albania", "al"},
	{"Algeria", "dz"},
	{"American Samoa", "as"},
	{"Andorra", "ad"},
	{"Angola", "ao"},
	{"Anguilla", "ai"},
	{"Antarctica", "aq"},
	{"Antigua and Barbuda", "ag"},
	{"Argentina", "ar"},
	{"Armenia", "am"},
	{"Aruba", "aw"},
	{"Australia", "au"},
	{"Austria", "at"},
	{"Azerbaijan", "az"},
	{"Bahamas", "bs"},
	{"Bahrain", "bh"},
	{"Bangladesh", "bd"},
	{"Barbados", "bb"},
	{"Belarus", "by"},
	{"Belgium", "be"},
	{"Belize", "bz"},
	{"Benin", "bj"},
	{"Bermuda", "bm"},
	{"Bhutan", "bt"},
	{"Bolivia, Plurinational State of", "bo"},
	{"Bonaire, Sint Eustatius and Saba", "bq"},
	{"Bosnia and Herzegovina", "ba"},
	{"Botswana", "bw"},
	{"Bouvet Island", "bv"},
	{"Brazil", "br"},
	{"British Indian Ocean Territory", "io"},
	{"Brunei Darussalam", "bn"},
	{"Bulgaria", "bg"},
	{"Burkina Faso", "bf"},
	{"Burundi", "bi"},
	{"Cambodia", "kh"},
	{"Cameroon", "cm"},
	{"Canada", "ca"},
	{"Cabo Verde", "cv"},
	{"Cayman Islands", "ky"},
	{"Central African Republic", "cf"},
	{"Chad", "td"},
	{"Chile", "cl"},
	{"China", "cn"},
	{"Christmas Island", "cx"},
	{"Cocos (Keeling) Islands", "cc"},
	{"Colombia", "co"},
	{"Comoros", "km"},
	{"Congo", "cg"},
	{"Congo, the Democratic Republic of the", "cd"},
	{"Cook Islands", "ck"},
	{"Costa Rica", "cr"},
	{"Côte d'Ivoire", "ci"},
	{"Croatia", "hr"},
	{"Cuba", "cu"},
	{"Curaçao", "cw"},
	{"Cyprus", "cy"},
	{"Czech Republic", "cz"},
	{"Denmark", "dk"},
	{"Djibouti", "dj"},
	{"Dominica", "dm"},
	{"Dominican Republic", "do"},
	{"Ecuador", "ec"},
	{"Egypt", "eg"},
	{"El Salvador", "sv"},
	{"Equatorial Guinea", "gq"},
	{"Eritrea", "er"},
	{"Estonia", "ee"},
	{"Ethiopia", "et"},
	{"Falkland Islands (Malvinas)", "fk"},
	{"Faroe Islands", "fo"},
	{"Fiji", "fj"},
	{"Finland", "fi"},
	{"France", "fr"},
	{"French Guiana", "gf"},
	{"French Polynesia", "pf"},
	{"French Southern Territories", "tf"},
	{"Gabon", "ga"},
	{"Gambia", "gm"},
	{"Georgia", "ge"},
	{"Germany", "de"},
	{"Ghana", "gh"},
	{"Gibraltar", "gi"},
	{"Greece", "gr"},
	{"Greenland", "gl"},
	{"Grenada", "gd"},
	{"Guadeloupe", "gp"},
	{"Guam", "gu"},
	{"Guatemala", "gt"},
	{"Guernsey", "gg"},
	{"Guinea", "gn"},
	{"Guinea-Bissau", "gw"},
	{"Guyana", "gy"},
	{"Haiti", "ht"},
	{"Heard Island and McDonald Islands", "hm"},
	{"Holy See (Vatican City State)", "va"},
	{"Honduras", "hn"},
	{"Hong Kong", "hk"},
	{"Hungary", "hu"},
	{"Iceland", "is"},
	{"India", "in"},
	{"Indonesia", "id"},
	{"Iran, Islamic Republic of", "ir"},
	{"Iraq", "iq"},
	{"Ireland", "ie"},
	{"Isle of Man", "im"},
	{"Israel", "il"},
	{"Italy", "it"},
	{"Jamaica", "jm"},
	{"Japan", "jp"},
	{"Jersey", "je"},
	{"Jordan", "jo"},
	{"Kazakhstan", "kz"},
	{"Kenya", "ke"},
	{"Kiribati", "ki"},
	{"Korea, Democratic People's Republic of", "kp"},
	{"Korea, Republic of", "kr"},
	{"Kuwait", "kw"},
	{"Kyrgyzstan", "kg"},
	{"Lao People's Democratic Republic", "la"},
	{"Latvia", "lv"},
	{"Lebanon", "lb"},
	{"Lesotho", "ls"},
	{"Liberia", "lr"},
	{"Libya", "ly"},
	{"Liechtenstein", "li"},
	{"Lithuania", "lt"},
	{"Luxembourg", "lu"},
	{"Macao", "mo"},
	{"Macedonia, the former Yugoslav Republic of", "mk"},
	{"Madagascar", "mg"},
	{"Malawi", "mw"},
	{"Malaysia", "my"},
	{"Maldives", "mv"},
	{"Mali", "ml"},
	{"Malta", "mt"},
	{"Marshall Islands", "mh"},
	{"Martinique", "mq"},
	{"Mauritania", "mr"},
	{"Mauritius", "mu"},
	{"Mayotte", "yt"},
	{"Mexico", "mx"},
	{"Micronesia, Federated States of", "fm"},
	{"Moldova, Republic of", "md"},
	{"Monaco", "mc"},
	{"Mongolia", "mn"},
	{"Montenegro", "me"},
	{"Montserrat", "ms"},
	{"Morocco", "ma"},
	{"Mozambique", "mz"},
	{"Myanmar", "mm"},
	{"Namibia", "na"},
	{"Nauru", "nr"},
	{"Nepal", "np"},
	{"Netherlands", "nl"},
	{"New Caledonia", "nc"},
	{"New Zealand", "nz"},
	{"Nicaragua", "ni"},
	{"Niger", "ne"},
	{"Nigeria", "ng"},
	{"Niue", "nu"},
	{"Norfolk Island", "nf"},
	{"Northern Mariana Islands", "mp"},
	{"Norway", "no"},
	{"Oman", "om"},
	{"Pakistan", "pk"},
	{"Palau", "pw"},
	{"Palestine, State of", "ps"},
	{"Panama", "pa"},
	{"Papua New Guinea", "pg"},
	{"Paraguay", "py"},
	{"Peru", "pe"},
	{"Philippines", "ph"},
	{"Pitcairn", "pn"},
	{"Poland", "pl"},
	{"Portugal", "pt"},
	{"Puerto Rico", "pr"},
	{"Qatar", "qa"},
	{"Réunion", "re"},
	{"Romania", "ro"},
	{"Russian Federation", "ru"},
	{"Rwanda", "rw"},
	{"Saint Barthélemy", "bl"},
	{"Saint Helena, Ascension and Tristan da Cunha", "sh"},
	{"Saint Kitts and Nevis", "kn"},
	{"Saint Lucia", "lc"},
	{"Saint Martin (French part)", "mf"},
	{"Saint Pierre and Miquelon", "pm"},
	{"Saint Vincent and the Grenadines", "vc"},
	{"Samoa", "ws"},
	{"San Marino", "sm"},
	{"Sao Tome and Principe", "st"},
	{"Saudi Arabia", "sa"},
	{"Senegal", "sn"},
	{"Serbia", "rs"},
	{"Seychelles", "sc"},
	{"Sierra Leone", "sl"},
	{"Singapore", "sg"},
	{"Sint Maarten (Dutch part)", "sx"},
	{"Slovakia", "sk"},
	{"Slovenia", "si"},
	{"Solomon Islands", "sb"},
	{"Somalia", "so"},
	{"South Africa", "za"},
	{"South Georgia and the South Sandwich Islands", "gs"},
	{"South Sudan", "ss"},
	{"Spain", "es"},
	{"Sri Lanka", "lk"},
	{"Sudan", "sd"},
	{"Suriname", "sr"},
	{"Svalbard and Jan Mayen", "sj"},
	{"Swaziland", "sz"},
	{"Sweden", "se"},
	{"Switzerland", "ch"},
	{"Syrian Arab Republic", "sy"},
	{"Taiwan, Province of China", "tw"},
	{"Tajikistan", "tj"},
	{"Tanzania, United Republic of", "tz"},
	{"Thailand", "th"},
	{"Timor-Leste", "tl"},
	{"Togo", "tg"},
	{"Tokelau", "tk"},
	{"Tonga", "to"},
	{"Trinidad and Tobago", "tt"},
	{"Tunisia", "tn"},
	{"Turkey", "tr"},
	{"Turkmenistan", "tm"},
	{"Turks and Caicos Islands", "tc"},
	{"Tuvalu", "tv"},
	{"Uganda", "ug"},
	{"Ukraine", "ua"},
	{"United Arab Emirates", "ae"},
	{"United Kingdom", "gb"},
	{"United States", "us"},
	{"United States Minor Outlying Islands", "um"},
	{"Uruguay", "uy"},
	{"Uzbekistan", "uz"},
	{"Vanuatu", "vu"},
	{"Venezuela, Bolivarian Republic of", "ve"},
	{"Viet Nam", "vn"},
	{"Virgin Islands, British", "vg"},
	{"Virgin Islands, U.S.", "vi"},
	{"Wallis and Futuna", "wf"},
	{"Western Sahara", "eh"},
	{"Yemen", "ye"},
	{"Zambia", "zm"},
	{"Zimbabwe", "zw"},
	{NULL, NULL}
};

static gboolean mmgui_providers_db_xml_parse(mmgui_providers_db_t db);
static void mmgui_providers_db_xml_get_element(GMarkupParseContext *context, const gchar *element, const gchar **attr_names, const gchar **attr_values, gpointer data, GError **error);
static void mmgui_providers_db_xml_get_value(GMarkupParseContext *context, const gchar *text, gsize size, gpointer data, GError **error);
static void mmgui_providers_db_xml_end_element(GMarkupParseContext *context, const gchar *element, gpointer data, GError **error);

/*static void mmgui_providers_db_foreach(gpointer data, gpointer user_data)
{
	mmgui_providers_db_entry_t entry;
	guint id, i;
	
	entry = (mmgui_providers_db_entry_t)data;
	
	if (entry->tech == MMGUI_PROVIDERS_DB_TECH_GSM) {
		g_printf("GSM: %s, %s, %s\n", entry->country, entry->name, entry->apn);
		if (entry->id != NULL) {
			g_printf("MCC, MNC: ");
			for (i=0; i<entry->id->len; i++) {
				id = g_array_index(entry->id, guint, i);
				g_printf("%u, %u; ", (id & 0xffff0000) >> 16, id & 0x0000ffff);
			}
			g_printf("\n");
		}
		if (entry->username != NULL) {
			g_printf("USER NAME: %s\n", entry->username);
		}
		if (entry->password != NULL) {
			g_printf("PASSWORD: %s\n", entry->password);
		}
		if (entry->dns1 != NULL) {
			g_printf("DNS1: %s\n", entry->dns1);
		}
		if (entry->dns2 != NULL) {
			g_printf("DNS2: %s\n", entry->dns2);
		}
		g_printf("\n");
		
	} else if (entry->tech == MMGUI_PROVIDERS_DB_TECH_CDMA) {
		g_printf("CDMA: %s, %s\n", entry->country, entry->name);
		if (entry->id != NULL) {
			g_printf("SIDs: ");
			for (i=0; i<entry->id->len; i++) {
				id = g_array_index(entry->id, guint, i);
				g_printf("%u; ", id);
			}
			g_printf("\n");
		}
		if (entry->username != NULL) {
			g_printf("USER NAME: %s\n", entry->username);
		}
		if (entry->password != NULL) {
			g_printf("PASSWORD: %s\n", entry->password);
		}
		if (entry->dns1 != NULL) {
			g_printf("DNS1: %s\n", entry->dns1);
		}
		if (entry->dns2 != NULL) {
			g_printf("DNS2: %s\n", entry->dns2);
		}
		g_printf("\n");
		
	}
}*/

mmgui_providers_db_t mmgui_providers_db_create(void)
{
	mmgui_providers_db_t db;
	GError *error;
	
	db = g_new(struct _mmgui_providers_db, 1);
	
	error = NULL;
	
	db->file = g_mapped_file_new(RESOURCE_PROVIDERS_DB, FALSE, &error);
	
	if ((db->file != NULL) && (error == NULL)) {
		db->curparam = MMGUI_PROVIDERS_DB_PARAM_NULL;
		db->curname = NULL;
		db->gotname = FALSE;
		db->curid = NULL;
		db->curentry = NULL;
		db->providers = NULL;
		mmgui_providers_db_xml_parse(db);
		return db;
	} else {
		g_debug("File not opened: %s\n", error->message);
		g_free(db);
		return NULL;
	}
}

void mmgui_providers_db_close(mmgui_providers_db_t db)
{
	if (db == NULL) return;
	
	//free parsed data
	
	g_mapped_file_unref(db->file);
	
	g_free(db);
}

GSList *mmgui_providers_get_list(mmgui_providers_db_t db)
{
	if (db == NULL) return NULL;
	
	return db->providers;
}

const gchar *mmgui_providers_provider_get_country_name(mmgui_providers_db_entry_t entry)
{
	gint i;
	
	if (entry == NULL) return "";
	
	for (i = 0; mmgui_providersdb_countries[i][0] != NULL; i++) {
		if (g_strcmp0(mmgui_providersdb_countries[i][1], entry->country) == 0) {
			return mmgui_providersdb_countries[i][0];
		}
	}
	
	return entry->country;
}

guint mmgui_providers_provider_get_network_id(mmgui_providers_db_entry_t entry)
{
	guint value, mcc, mnc, mul;
	
	if (entry == NULL) return 0;
	
	if (entry->id == NULL) return 0;
	
	value = g_array_index(entry->id, guint, 0);
	
	if (entry->tech == MMGUI_DEVICE_TYPE_GSM) {
		/*GSM uses combined value of MCC and MNC*/
		mcc = (value & 0xffff0000) >> 16;
		mnc = value & 0x0000ffff;
		mul = 1;
		while (mul <= mnc) {
			mul *= 10;
		}
		return mcc * mul + mnc;
	} else {
		/*CDMA uses one value - SID*/
		return value;
	}
}

static gboolean mmgui_providers_db_xml_parse(mmgui_providers_db_t db)
{
	GMarkupParser mp;
	GMarkupParseContext *mpc;
	GError *error = NULL;
	
	if (db == NULL) return FALSE;
	if (db->file == NULL) return FALSE;
	
	/*Prepare callbacks*/
	mp.start_element = mmgui_providers_db_xml_get_element;
	mp.end_element = mmgui_providers_db_xml_end_element;
	mp.text = mmgui_providers_db_xml_get_value;
	mp.passthrough = NULL;
	mp.error = NULL;
	
	/*Parse XML*/
	mpc = g_markup_parse_context_new(&mp, 0, db, NULL);
	g_markup_parse_context_parse(mpc, g_mapped_file_get_contents(db->file), g_mapped_file_get_length(db->file), &error);
	if (error != NULL) {
		g_error_free(error);
		g_markup_parse_context_free(mpc);
		return FALSE;
	}
	g_markup_parse_context_free(mpc);
	
	/*Sort providers list*/
	db->providers = g_slist_sort(db->providers, (GCompareFunc)g_strcmp0);
	
	return TRUE;
}

static void mmgui_providers_db_xml_get_element(GMarkupParseContext *context, const gchar *element, const gchar **attr_names, const gchar **attr_values, gpointer data, GError **error)
{
	mmgui_providers_db_t db;
	mmgui_providers_db_entry_t entry;
	guint netid, i;
		
	db = (mmgui_providers_db_t)data;
	
	if (db == NULL) return;
	
	if (g_str_equal(element, "country")) {
		if ((attr_names[0] != NULL) && (attr_values[0] != NULL)) {
			if (g_str_equal(attr_names[0], "code")) {
				memset(db->curcountry, 0, sizeof(db->curcountry));
				memcpy(db->curcountry, attr_values[0], 2);
			}
		}
		db->curparam = MMGUI_PROVIDERS_DB_PARAM_COUNTRY;
	} else if (g_str_equal(element, "provider")) {
		db->curparam = MMGUI_PROVIDERS_DB_PARAM_PROVIDER;
	} else if (g_str_equal(element, "name")) {
		db->curparam = MMGUI_PROVIDERS_DB_PARAM_NAME;
	} else if (g_str_equal(element, "gsm")) {
		db->curparam = MMGUI_PROVIDERS_DB_PARAM_GSM;
	} else if (g_str_equal(element, "cdma")) {
		entry = g_new0(struct _mmgui_providers_db_entry, 1);
		/*Usage*/
		//entry->usage = MMGUI_PROVIDERS_DB_ENTRY_USAGE_INTERNET;
		/*Technology*/
		entry->tech = MMGUI_DEVICE_TYPE_CDMA/*MMGUI_PROVIDERS_DB_TECH_CDMA*/;
		/*Country*/
		memcpy(entry->country, db->curcountry, sizeof(entry->country));
		/*Operator name*/
		entry->name = NULL;
		/*Access point*/
		entry->apn = NULL;
		/*Copy identifiers*/
		if (db->curid != NULL) {
			entry->id = g_array_new(FALSE, TRUE, sizeof(guint));
			for (i=0; i<db->curid->len; i++) {
				netid = g_array_index(db->curid, guint, i);
				g_array_append_val(entry->id, netid);
			}
		}
		/*Store pointer for easy access*/
		db->curentry = entry;
		/*Add pointer to list*/
		db->providers = g_slist_prepend(db->providers, entry);
		db->curparam = MMGUI_PROVIDERS_DB_PARAM_CDMA;
	} else if (g_str_equal(element, "network-id")) {
		netid = 0;
		if ((attr_names[0] != NULL) && (attr_values[0] != NULL)) {
			if (g_str_equal(attr_names[0], "mcc")) {
				netid |= (atoi(attr_values[0]) << 16) & 0xffff0000;
			}
		}
		if ((attr_names[1] != NULL) && (attr_values[1] != NULL)) {
			if (g_str_equal(attr_names[1], "mnc")) {
				netid |= atoi(attr_values[1]) & 0x0000ffff;
			}
		}
		if (netid != 0) {
			if (db->curentry == NULL) {
				if (db->curid == NULL) {
					db->curid = g_array_new(FALSE, TRUE, sizeof(guint));
				}
				g_array_append_val(db->curid, netid);
			} else {
				if (db->curentry->id == NULL) {
					db->curentry->id = g_array_new(FALSE, TRUE, sizeof(guint));
				}
				g_array_append_val(db->curentry->id, netid);
			}
		}
		db->curparam = MMGUI_PROVIDERS_DB_PARAM_NETWORK_ID;
	} else if (g_str_equal(element, "sid")) {
		netid = 0;
		if ((attr_names[0] != NULL) && (attr_values[0] != NULL)) {
			if (g_str_equal(attr_names[0], "value")) {
				netid = (atoi(attr_values[0]) & 0xffffffff);
			}
		}
		if (netid != 0) {
			if (db->curentry == NULL) {
				if (db->curid == NULL) {
					db->curid = g_array_new(FALSE, TRUE, sizeof(guint));
				}
				g_array_append_val(db->curid, netid);
			} else {
				if (db->curentry->id == NULL) {
					db->curentry->id = g_array_new(FALSE, TRUE, sizeof(guint));
				}
				g_array_append_val(db->curentry->id, netid);
			}
		}
		db->curparam = MMGUI_PROVIDERS_DB_PARAM_SID;
	} else if (g_str_equal(element, "apn")) {
		if ((attr_names[0] != NULL) && (attr_values[0] != NULL)) {
			if (g_str_equal(attr_names[0], "value")) {
				entry = g_new0(struct _mmgui_providers_db_entry, 1);
				/*Usage*/
				//entry->usage = MMGUI_PROVIDERS_DB_ENTRY_USAGE_INTERNET;
				/*Technology*/
				entry->tech = MMGUI_DEVICE_TYPE_GSM/*MMGUI_PROVIDERS_DB_TECH_GSM*/;
				/*Country*/
				memcpy(entry->country, db->curcountry, sizeof(entry->country));
				/*Operator name*/
				entry->name = NULL;
				/*Access point*/
				entry->apn = g_strdup(attr_values[0]);
				/*Copy identifiers*/
				if (db->curid != NULL) {
					entry->id = g_array_new(FALSE, TRUE, sizeof(guint));
					for (i=0; i<db->curid->len; i++) {
						netid = g_array_index(db->curid, guint, i);
						g_array_append_val(entry->id, netid);
					}
				}
				/*Store pointer for easy access*/
				db->curentry = entry;
				/*Add pointer to list*/
				db->providers = g_slist_prepend(db->providers, entry);
			}
		}
		db->curparam = MMGUI_PROVIDERS_DB_PARAM_APN;
	} else if (g_str_equal(element, "usage")) {
		if (db->curentry != NULL) {
			if ((attr_names[0] != NULL) && (attr_values[0] != NULL)) {
				if (g_str_equal(attr_names[0], "type")) {
					if (g_str_equal(attr_values[0], "internet")) {
						db->curentry->usage = MMGUI_PROVIDERS_DB_ENTRY_USAGE_INTERNET;
					} else if (g_str_equal(attr_values[0], "mms")) {
						db->curentry->usage = MMGUI_PROVIDERS_DB_ENTRY_USAGE_MMS;
					} else {
						db->curentry->usage = MMGUI_PROVIDERS_DB_ENTRY_USAGE_OTHER;
					}
				}
			}
		}
		
		db->curparam = MMGUI_PROVIDERS_DB_PARAM_USAGE;
	} else if (g_str_equal(element, "username")) {
		db->curparam = MMGUI_PROVIDERS_DB_PARAM_USERNAME;
	} else if (g_str_equal(element, "password")) {
		db->curparam = MMGUI_PROVIDERS_DB_PARAM_PASSWORD;
	} else if (g_str_equal(element, "dns")) {
		db->curparam = MMGUI_PROVIDERS_DB_PARAM_DNS;
	} else {
		db->curparam = MMGUI_PROVIDERS_DB_PARAM_NULL;
	}
}

static void mmgui_providers_db_xml_get_value(GMarkupParseContext *context, const gchar *text, gsize size, gpointer data, GError **error)
{
	mmgui_providers_db_t db;
	
	db = (mmgui_providers_db_t)data;
	
	if (db == NULL) return;
	
	if ((db->curparam == MMGUI_PROVIDERS_DB_PARAM_NULL) || (text[0] == '\n')) return;
	
	switch (db->curparam) {
		case MMGUI_PROVIDERS_DB_PARAM_NAME:
			if (db->curentry != NULL) {
				if (db->curentry->name != NULL) {
					g_free(db->curentry->name);
				}
				db->curentry->name = g_strdup(text);
			} else {
				if (db->curname != NULL) {
					g_free(db->curname);
				}
				db->curname = g_strdup(text);
			}
			break;
		case MMGUI_PROVIDERS_DB_PARAM_USERNAME:
			if (db->curentry != NULL) {
				db->curentry->username = g_strdup(text);
			}
			break;
		case MMGUI_PROVIDERS_DB_PARAM_PASSWORD:
			if (db->curentry != NULL) {
				db->curentry->password = g_strdup(text);
			}
			break;
		case MMGUI_PROVIDERS_DB_PARAM_DNS:
			if (db->curentry != NULL) {
				if (db->curentry->dns1 == NULL) {
					db->curentry->dns1 = g_strdup(text);
				} else if (db->curentry->dns2 == NULL) {
					db->curentry->dns2 = g_strdup(text);
				}
			}
			break;
		default:
			break;
	}
}

static void mmgui_providers_db_xml_end_element(GMarkupParseContext *context, const gchar *element, gpointer data, GError **error)
{
	mmgui_providers_db_t db;
	gchar *planname;
	
	db = (mmgui_providers_db_t)data;
	
	if (db == NULL) return;
	
	if ((g_str_equal(element, "cdma")) || (g_str_equal(element, "apn"))) {
		if (db->curentry->name == NULL) {
			if (db->curname != NULL) {
				db->curentry->name = g_strdup(db->curname);
			} else {
				db->curentry->name = g_strdup("New provider");
			}
		} else {
			if (db->curname != NULL) {
				if (!g_str_equal(db->curname, db->curentry->name)) {
					planname = db->curentry->name;
					db->curentry->name = g_strdup_printf("%s - %s", db->curname, planname);
					g_free(planname);
				}
			}
		}
		db->curentry = NULL;
	} else if (g_str_equal(element, "provider")) {
		if (db->curname != NULL) {
			g_free(db->curname);
			db->curname = NULL;
		}
		if (db->curid != NULL) {
			g_array_unref(db->curid);
			db->curid = NULL;
		}
	}
}
