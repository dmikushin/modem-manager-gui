/*
 *      vcard.c
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

#include <glib.h>
#include <string.h>
#include <ctype.h>

#include "mmguicore.h"
#include "vcard.h"

enum _mmgui_vcard_attribute {
	MMGUI_VCARD_ATTRIBUTE_BEGIN = 0,
	MMGUI_VCARD_ATTRIBUTE_END,
	MMGUI_VCARD_ATTRIBUTE_EMAIL,
	MMGUI_VCARD_ATTRIBUTE_FN,
	MMGUI_VCARD_ATTRIBUTE_N,
	MMGUI_VCARD_ATTRIBUTE_TEL,
	MMGUI_VCARD_ATTRIBUTE_UNKNOWN
};


static gchar *vcard_unescape_value(const gchar *valuestr, gchar *strstart, gint attribute);
static gchar *vcard_parse_attribute(const gchar *srcstr, gint attribute);

 
static gchar *vcard_unescape_value(const gchar *valuestr, gchar *strstart, gint attribute)
{
	gsize length, startlength;
	gint i, numchars;
	gchar *unescapedstr, *reallocstr;
	
	if (valuestr == NULL) return strstart;
	
	length = strlen(valuestr);
	
	if (length == 0) return strstart;
	
	i = 0;
	startlength = 0;
	numchars = 0;
	
	if (strstart != NULL) {
		startlength = strlen(strstart);
		/*Ignore space in new part of value*/
		if (valuestr[0] == ' ') {
			i = 1;
		}
	}
	
	unescapedstr = g_malloc0(startlength + length + 1);
	
	while (valuestr[i] != '\0') {
		if (valuestr[i] == '\\') {
			/*Escaped character*/
			switch (valuestr[i+1]) {
				/*Replace known sequence with single character*/
				case 'n':
					unescapedstr[startlength+numchars] = '\n';
					numchars++;
					i += 2;
					break;
				case 'r':
					unescapedstr[startlength+numchars++] = '\r';
					numchars++;
					i += 2;
					break;
				case ',':
				case ';':
				case '\\':
					unescapedstr[startlength+numchars] = valuestr[i+1];
					numchars++;
					i += 2;
					break;
				default:
					/*Unknown sequence - replace slash with space*/
					unescapedstr[startlength+numchars] = ' ';
					numchars++;
					i++;
					break;
			}
		} else if (valuestr[i] == ';') {
			/*Delimiter*/
			if ((valuestr[i+1] != ';') && (valuestr[i+1] != '\0')) {
				/*Value exists - delimiter must be replaced*/
				unescapedstr[startlength+numchars] = ',';
				numchars++;
				i++;
			} else {
				/*No value - just skip delimiter*/
				i++;
			}
		} else {
			/*Regular character - copy without change*/
			if (attribute == MMGUI_VCARD_ATTRIBUTE_TEL) {
				/*Escape phone number*/
				if ((isdigit(valuestr[i])) || ((i == 0) && (valuestr[i] == '+'))) {
					unescapedstr[startlength+numchars] = valuestr[i];
					numchars++;
				} 
			} else {
				/*Other attributes*/
				unescapedstr[startlength+numchars] = valuestr[i];
				numchars++;
			}
			i++;
		}
	}
	/*Terminate string*/
	unescapedstr[startlength+numchars] = '\0';
	
	if (numchars == 0) {
		/*String is empty*/
		g_free(unescapedstr);
		return strstart;
	}
	
	/*Truncate string*/
	if ((numchars + 1) < length) {
		reallocstr = g_realloc(unescapedstr, startlength + numchars + 1);
		if (reallocstr != NULL) {
			unescapedstr = reallocstr;
		}
	}
	/*Copy start fragment*/
	memcpy(unescapedstr, strstart, startlength);
	
	return unescapedstr;
}

static gchar *vcard_parse_attribute(const gchar *srcstr, gint attribute)
{
	gchar *valuestr;
		
	if (srcstr == NULL) return NULL;
	
	valuestr = strchr(srcstr, ':');
	
	if (valuestr == NULL) return NULL;
	
	return vcard_unescape_value(valuestr + 1, NULL, attribute);
}

gint vcard_parse_string(const gchar *srcstr, GSList **contacts, gchar *group)
{
	guint numcontacts, strnum;
	gchar **strings;
	GSList *vcardrows;
		
	if ((srcstr == NULL) || (contacts == NULL)) return 0;
	
	/*Split string by line delimeters*/
	strings = g_strsplit(srcstr, "\r\n", 0);
	
	/*String is empty*/
	if (strings == NULL) return 0;	
	
	/*Linked list with rows*/
	strnum = 0;
	vcardrows = NULL;
	
	while (strings[strnum] != NULL) {
		if (strings[strnum][0] != '\0') {
			vcardrows = g_slist_prepend(vcardrows, strings[strnum]);
		}
		strnum++;
	}
	
	numcontacts = 0;
	
	if (vcardrows != NULL) {
		/*Reverse linked list*/
		vcardrows = g_slist_reverse(vcardrows);
		/*Parse contacts*/
		numcontacts = vcard_parse_list(vcardrows, contacts, group);
	}
	
	/*Free string array*/
	g_strfreev(strings);
		
	return numcontacts;
}

gint vcard_parse_list(GSList *vcardrows, GSList **contacts, gchar *group)
{
	guint numcontacts;
	GSList *iterator;
	gchar *value, *row;
	gint curattribute;
	mmgui_contact_t contact;
	
	if ((vcardrows == NULL) || (contacts == NULL)) return 0;
	
	numcontacts = 0;
	contact = NULL;
	curattribute = MMGUI_VCARD_ATTRIBUTE_UNKNOWN;
	
	for (iterator = vcardrows; iterator != NULL; iterator = iterator->next) {
		row = (gchar *)iterator->data;
		if (row != NULL) {
			if ((row[0] != '\0') && (row[0] != '\r') && (row[0] != '\n')) {
				if (strchr(row, ':') == NULL) {
					/*String break*/
					if (contact != NULL) {
						switch (curattribute) {
							/*Concatenate row*/
							case MMGUI_VCARD_ATTRIBUTE_EMAIL:
								contact->email = vcard_unescape_value(row, contact->email, curattribute);
								break;
							case MMGUI_VCARD_ATTRIBUTE_FN:
								if (contact->name != NULL) {
									contact->name = vcard_unescape_value(row, contact->name, curattribute);
								} else if (contact->name2 == NULL) {
									contact->name2 = vcard_unescape_value(row, contact->name2, curattribute);
								}
								break;
							case MMGUI_VCARD_ATTRIBUTE_N:
								if (contact->name2 != NULL) {
									contact->name2 = vcard_unescape_value(row, contact->name2, curattribute);
								} else if (contact->name2 == NULL) {
									contact->name = vcard_unescape_value(row, contact->name, curattribute);
								}
								break;
							case MMGUI_VCARD_ATTRIBUTE_TEL:
								if (contact->number == NULL) {
									contact->number = vcard_unescape_value(row, contact->number, curattribute);
								} else if (contact->number2 == NULL) {
									contact->number2 = vcard_unescape_value(row, contact->number2, curattribute);
								}
								break;
							/*Do nothing*/
							case MMGUI_VCARD_ATTRIBUTE_BEGIN:
							case MMGUI_VCARD_ATTRIBUTE_END:
							case MMGUI_VCARD_ATTRIBUTE_UNKNOWN:
							default:
								break;
						}
					}
				} else {
					/*New attribute*/
					switch (row[0]) {
						case 'b':
						case 'B':
							if (g_ascii_strcasecmp((const gchar *)row, "BEGIN:VCARD") == 0) {
								/*VCard beginning*/
								curattribute = MMGUI_VCARD_ATTRIBUTE_BEGIN;
								contact = g_new0(struct _mmgui_contact, 1);
								contact->id = numcontacts;
								/*Full name of the contact*/
								contact->name = NULL;
								/*Telephone number*/
								contact->number = NULL;
								/*Email address*/
								contact->email = NULL;
								/*Group this contact belongs to*/
								contact->group = g_strdup(group);
								/*Additional contact name*/
								contact->name2 = NULL;
								/*Additional contact telephone number*/
								contact->number2 = NULL;
								/*Boolean flag to specify whether this entry is hidden or not*/
								contact->hidden = FALSE;
								/*Phonebook in which the contact is stored*/
								contact->storage = MMGUI_MODEM_CONTACTS_STORAGE_ME;
							}
							break;
						case 'e':
						case 'E':
							if (g_ascii_strcasecmp((const gchar *)row, "END:VCARD") == 0) {
								/*VCard ending*/
								curattribute = MMGUI_VCARD_ATTRIBUTE_END;
								if (contact != NULL) {
									if (((contact->name != NULL) || (contact->email != NULL) || (contact->name2 != NULL)) && ((contact->number != NULL) || (contact->number2 != NULL))) {
										/*Set primary name*/
										if (contact->name == NULL) {
											if (contact->email != NULL) {
												contact->name = g_strdup(contact->email);
											} else if (contact->name2 != NULL) {
												contact->name = g_strdup(contact->name2);
											}
										}
										/*Add contact to list*/
										*contacts = g_slist_prepend(*contacts, contact);
										numcontacts++;
									} else {
										/*Free contact data*/
										if (contact->name != NULL) {
											g_free(contact->name);
										}
										if (contact->number != NULL) {
											g_free(contact->number);
										}
										if (contact->email != NULL) {
											g_free(contact->email);
										}
										if (contact->group != NULL) {
											g_free(contact->group);
										}
										if (contact->name2 != NULL) {
											g_free(contact->name2);
										}
										if (contact->number2 != NULL) {
											g_free(contact->number2);
										}
										g_free(contact);
									}
								}
							} else if (g_ascii_strncasecmp((const gchar *)row, "EMAIL", 5) == 0) {
								/*Email*/
								curattribute = MMGUI_VCARD_ATTRIBUTE_EMAIL;
								contact->email = vcard_parse_attribute(row, MMGUI_VCARD_ATTRIBUTE_EMAIL);
							}
							break;
						case 'f':
						case 'F':
							if (g_ascii_strncasecmp((const gchar *)row, "FN:", 3) == 0) {
								/*Formatted name*/
								curattribute = MMGUI_VCARD_ATTRIBUTE_FN;
								value = vcard_parse_attribute(row, MMGUI_VCARD_ATTRIBUTE_FN);
								if (value != NULL) {
									if (contact->name == NULL) {
										contact->name = value;
									} else if (contact->name2 == NULL) {
										contact->name2 = value;
									}
								}
							}
							break;
						case 'n':
						case 'N':
							if (g_ascii_strncasecmp((const gchar *)row, "N:", 2) == 0) {
								/*Name*/
								curattribute = MMGUI_VCARD_ATTRIBUTE_N;
								value = vcard_parse_attribute(row, MMGUI_VCARD_ATTRIBUTE_N);
								if (value != NULL) {
									if (contact->name2 == NULL) {
										contact->name2 = value;
									} else if (contact->name == NULL) {
										contact->name = value;
									}
								}
							}
							break;
						case 't':
						case 'T':
							if (g_ascii_strncasecmp((const gchar *)row, "TEL", 3) == 0) {
								/*Telephone*/
								curattribute = MMGUI_VCARD_ATTRIBUTE_TEL;
								value = vcard_parse_attribute(row, MMGUI_VCARD_ATTRIBUTE_TEL);
								if (value != NULL) {
									if (contact->number == NULL) {
										contact->number = value;
									} else if (contact->number2 == NULL) {
										contact->number2 = value;
									}
								}
							}
							break;
						default:
							/*Unknown entry*/
							curattribute = MMGUI_VCARD_ATTRIBUTE_UNKNOWN;
							break;
					}
				}
			}
		}
	}
	
	/*Reverse list if contacts found*/
	if (numcontacts > 0) {
		*contacts = g_slist_reverse(*contacts);
	}
	
	return numcontacts;
}
