/*
 *      addressbooks.c
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
#include <gio/gio.h>
#include <gmodule.h>

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <linux/un.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

#include "mmguicore.h"
#include "dbus-utils.h"
#include "vcard.h"
#include "addressbooks.h"

#define MMGUI_ADDRESSBOOKS_GNOME_CONNECT_TIMEOUT     15

#define MMGUI_ADDRESSBOOKS_AKONADI_LINK_PATH         "%s/.local/share/akonadi/socket-%s"
#define MMGUI_ADDRESSBOOKS_AKONADI_SOCKET_PATH       "%s/akonadiserver.socket"
#define MMGUI_ADDRESSBOOKS_AKONADI_DBUS_INTERFACE    "org.freedesktop.Akonadi.Control"
#define MMGUI_ADDRESSBOOKS_AKONADI_NULL_COMMAND      "\r\n"
#define MMGUI_ADDRESSBOOKS_AKONADI_LOGIN_COMMAND     "LOGIN mmgui mmgui\r\n"
#define MMGUI_ADDRESSBOOKS_AKONADI_INFO_COMMAND      "X-AKLSUB 0 INF ()\r\n"
#define MMGUI_ADDRESSBOOKS_AKONADI_SELECT_COMMAND    "SELECT SILENT %u\r\n"
#define MMGUI_ADDRESSBOOKS_AKONADI_FETCH_COMMAND     "FETCH 1:* FULLPAYLOAD\r\n"

enum _mmgui_addressbooks_akonadi_command_status_id {
	MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ID_OK = 0,
	MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ID_NO,
	MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ID_BAD
};

enum _mmgui_addressbooks_akonadi_status {
	MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ALREADY_RUNNING 	= 2,
	MMGUI_ADDRESSBOOKS_AKONADI_STATUS_SUCCESS 	        = 1
};

struct _mmgui_addressbooks_akonadi_command {
	/*Request*/
	gchar *request;
	guint requestid;
	gsize requestlen;
	gchar requestprefix[16];
	gsize requestprefixlen;
	/*Answer*/
	gint statusid;
	gchar *statusmessage;
	gchar *answer;
};

typedef struct _mmgui_addressbooks_akonadi_command *mmgui_addressbooks_akonadi_command_t;

struct _mmgui_addressbooks_akonadi_collection {
	gint id;
	gchar *name;
};

typedef struct _mmgui_addressbooks_akonadi_collection *mmgui_addressbooks_akonadi_collection_t;

//KDE (Akonadi)
static gint mmgui_addressbooks_open_kde_socket(void);
static void mmgui_addressbooks_fill_akonadi_command_struct(mmgui_addressbooks_akonadi_command_t command, const gchar *format, ...);
static void mmgui_addressbooks_free_akonadi_command_struct(mmgui_addressbooks_akonadi_command_t command);
static gboolean mmgui_addressbooks_get_akonadi_command_status(gchar *status, mmgui_addressbooks_akonadi_command_t command);
static gboolean mmgui_addressbooks_execute_akonadi_command(mmgui_addressbooks_t addressbooks, mmgui_addressbooks_akonadi_command_t command);
static guint mmgui_addressbooks_akonadi_get_integer(gchar *text, gchar *prefix, gchar *suffix);
static gchar *mmgui_addressbooks_akonadi_get_substring(gchar *text, gchar *prefix, gchar *suffix);
static gboolean mmgui_addressbooks_akonadi_find_substring(gchar *text, gchar *prefix, gchar *suffix, gchar *substring);
static GSList *mmgui_addressbooks_akonadi_get_collections(const gchar *list);
static void mmgui_addressbooks_akonadi_free_collections_foreach(gpointer data, gpointer user_data);
static guint mmgui_addressbooks_akonadi_collection_get_contacts(mmgui_addressbooks_t addressbooks, mmgui_addressbooks_akonadi_collection_t collection, const gchar *vcardlist);
static gboolean mmgui_addressbooks_get_kde_contacts(mmgui_addressbooks_t addressbooks);
//GNOME (Evolution data server)
static void mmgui_addressbooks_get_gnome_contacts_foreach(gpointer data, gpointer user_data);
static gboolean mmgui_addressbooks_get_gnome_contacts(mmgui_addressbooks_t addressbooks, mmgui_libpaths_cache_t libcache);
//Other
static gpointer mmguicore_addressbooks_work_thread(gpointer data);
static void mmgui_addressbooks_free_contacts_list_foreach(gpointer data, gpointer user_data);


//KDE (Akonadi)
static gint mmgui_addressbooks_open_kde_socket(void)
{
	const gchar *homedir;
	struct utsname unamebuf;
	gchar *linkpath, *linkvalue;
	gchar *socketpath;
	struct stat statbuf;
	GError *error;
	gint socketfd;
	struct sockaddr_un address;

	homedir = g_get_home_dir();

	if (uname(&unamebuf) == -1) {
		g_debug("Failed to get hostname\n");
		return -1;
	}

	linkpath = g_strdup_printf(MMGUI_ADDRESSBOOKS_AKONADI_LINK_PATH, homedir, unamebuf.nodename);

	error = NULL;

	linkvalue = g_file_read_link((const gchar *)linkpath, &error);

	g_free(linkpath);

	if ((linkvalue == NULL) && (error != NULL)) {
		g_debug("Failed to read symlink: %s\n", error->message);
		g_error_free(error);
		return -1;
	}

	socketpath = g_strdup_printf(MMGUI_ADDRESSBOOKS_AKONADI_SOCKET_PATH, linkvalue);

	g_free(linkvalue);

	if (stat(socketpath, &statbuf) != 0) {
		g_debug("Failed to locate Akonadi server socket\n");
		g_free(socketpath);
		return -1;
	}

	if (!S_ISSOCK(statbuf.st_mode)) {
		g_debug("Wrong type of Akonadi IPC\n");
		g_free(socketpath);
		return -1;
	}

	socketfd = socket(AF_UNIX, SOCK_STREAM, 0);

	if(socketfd < 0) {
		g_debug("Failed to open socket\n");
		g_free(socketpath);
		return -1;
	}

	memset(&address, 0, sizeof(struct sockaddr_un));

	address.sun_family = AF_UNIX;
	strncpy(address.sun_path, socketpath, sizeof(address.sun_path)-1);
	
	g_free(socketpath);

	if(connect(socketfd, (struct sockaddr *)&address, sizeof(struct sockaddr_un)) != 0) {
		g_debug("Failed to connect socket\n");
		return -1;
	}

	return socketfd;
}

static void mmgui_addressbooks_fill_akonadi_command_struct(mmgui_addressbooks_akonadi_command_t command, const gchar *format, ...)
{
	gssize reqsegmsize, reqlen;
	gchar *reqsegm, *newreqsegm;
	va_list args;

	if ((command == NULL) || (format == NULL)) return;

	/*Request prefix*/
	memset(command->requestprefix, 0, sizeof(command->requestprefix));
	if (command->requestid > 0) {
		/*Regular request*/
		command->requestprefixlen = snprintf(command->requestprefix, sizeof(command->requestprefix), "A%u ", command->requestid);
	} else {
		/*Server greeting*/
		command->requestprefixlen = snprintf(command->requestprefix, sizeof(command->requestprefix), "* ");
	}

	/*Clear request*/
	if (command->request != NULL) {
		g_free(command->request);
		command->request = NULL;
	}

	if (command->requestid > 0) {
		/*Form request*/
		reqsegmsize = 100 - command->requestprefixlen;
		reqsegm = g_malloc0(reqsegmsize);
		while (TRUE) {
			/*Try to use allocated segment*/
			va_start(args, format);
			reqlen = vsnprintf(reqsegm + command->requestprefixlen, reqsegmsize, format, args);
			va_end(args);
			/*Test returned value*/
			if (reqlen < 0) {
				command->request = NULL;
				command->requestlen = 0;
				break;
			}
			/*Test if request is ready and return*/
			if (reqlen < reqsegmsize) {
				/*Add prefix*/
				memcpy(reqsegm, command->requestprefix, command->requestprefixlen);
				/*Return request*/
				command->request = reqsegm;
				command->requestlen = (gsize)reqlen + command->requestprefixlen;
				break;
			}
			/*Increase segment size*/
			reqsegmsize = reqlen + 50;
			/*Reallocate segment*/
			newreqsegm = g_realloc(reqsegm, reqsegmsize);
			if (newreqsegm != NULL) {
				/*Zero reallocated memory before use*/
				memset(newreqsegm, 0, sizeof(reqsegmsize));
				reqsegm = newreqsegm;
			} else {
				/*No chance to reallocate*/
				command->request = NULL;
				command->requestlen = 0;
				break;
			}

		}
	} else {
		/*No request must be sent*/
		command->request = NULL;
		command->requestlen = 0;
	}
	/*Next request ID*/
	command->requestid++;

	/*Set default command status id*/
	command->statusid = MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ID_OK;
	/*Clear server status message*/
	if (command->statusmessage != NULL) {
		g_free(command->statusmessage);
		command->statusmessage = NULL;
	}
	/*Clear answer*/
	if (command->answer != NULL) {
		g_free(command->answer);
		command->answer = NULL;
	}
}

static void mmgui_addressbooks_free_akonadi_command_struct(mmgui_addressbooks_akonadi_command_t command)
{
	if (command == NULL) return;

	/*Clear request prefix*/
	memset(command->requestprefix, 0, sizeof(command->requestprefix));
	command->requestprefixlen = 0;
	/*Reset request ID*/
	command->requestid = 0;
	/*Clear request*/
	if (command->request != NULL) {
		g_free(command->request);
		command->request = NULL;
	}
	command->requestlen = 0;

	/*Set default command status id*/
	command->statusid = MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ID_OK;
	/*Clear server status message*/
	if (command->statusmessage != NULL) {
		g_free(command->statusmessage);
		command->statusmessage = NULL;
	}
	/*Clear answer*/
	if (command->answer != NULL) {
		g_free(command->answer);
		command->answer = NULL;
	}
}

static gboolean mmgui_addressbooks_get_akonadi_command_status(gchar *status, mmgui_addressbooks_akonadi_command_t command)
{
	if ((status == NULL) || (command == NULL)) return FALSE;

	/*Get status identifier*/
	if ((status[0] == 'O') && (status[1] == 'K')) {
		command->statusid = MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ID_OK;
	} else if ((status[0] == 'N') && (status[1] == 'O')) {
		command->statusid = MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ID_NO;
	} else if ((status[0] == 'B') && (status[1] == 'A') && (status[2] == 'D')) {
		command->statusid = MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ID_BAD;
	} else {
		command->statusid = MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ID_NO;
	}
	/*Get status message*/
	command->statusmessage = strchr(status, ' ');
	if (command->statusmessage != NULL) {
		command->statusmessage = g_strdup(command->statusmessage + 1);
	}

	return TRUE;
}

static gboolean mmgui_addressbooks_execute_akonadi_command(mmgui_addressbooks_t addressbooks, mmgui_addressbooks_akonadi_command_t command)
{
	GString *fullanswer;
	gchar buffer[1024];
	gint res, bytes, i;
	gboolean completed;

	if ((addressbooks == NULL) || (command == NULL)) return FALSE;

	if (command->request != NULL) {
		bytes = send(addressbooks->aksocket, command->request, command->requestlen, 0);
		if (bytes <= 0) {
			return FALSE;
		}
	}

	fullanswer = g_string_new(NULL);

	completed = FALSE;

	bytes = 0;
	memset(buffer, 0, sizeof(buffer));

	do {
		res = recv(addressbooks->aksocket, buffer, sizeof(buffer)-1, 0);
		if (res > 0) {
			bytes += res;
			buffer[res] = '\0';
			fullanswer = g_string_append(fullanswer, buffer);
			if ((fullanswer->len > command->requestprefixlen+2) && (fullanswer->str[fullanswer->len-2] == '\r') && (fullanswer->str[fullanswer->len-1] == '\n')) {
				for (i=fullanswer->len-4; i>=0; i--) {
					/*Multiline answer*/
					if ((fullanswer->str[i] == '\r') && (fullanswer->str[i+1] == '\n')) {
						if (strncmp(fullanswer->str+i+2, command->requestprefix, command->requestprefixlen) == 0) {
							completed = mmgui_addressbooks_get_akonadi_command_status(fullanswer->str+i+2+command->requestprefixlen, command);
							fullanswer = g_string_truncate(fullanswer, i);
							command->answer = g_string_free(fullanswer, FALSE);
							break;
						}
					} else if (i == 0) {
						/*Single line answer*/
						if (strncmp(fullanswer->str, command->requestprefix, command->requestprefixlen) == 0) {
							completed = mmgui_addressbooks_get_akonadi_command_status(fullanswer->str+command->requestprefixlen, command);
							command->answer = g_string_free(fullanswer, TRUE);
							break;
						}
					}
				}
			}
		} else {
			break;
		}
	} while ((res != 0) && (!completed));

	return completed;
}

static guint mmgui_addressbooks_akonadi_get_integer(gchar *text, gchar *prefix, gchar *suffix)
{
	gchar *start, *end;
	gsize fragmentlen;
	gint i, d;
	guint mult, res;
	gchar curdigit;
	gchar digits[2][10] = {{'0','1','2','3','4','5','6','7','8','9'}, {0,1,2,3,4,5,6,7,8,9}};

	if (text == NULL) return 0;

	start = text;
	mult = 1;
	res = 0;

	/*Number start*/
	if (prefix != NULL) {
		start = strstr(text, prefix);
		if (start != NULL) {
			start += strlen(prefix);
		}
	}

	/*Number end*/
	end = strstr(start + 1, suffix);

	if (end != NULL) {
		fragmentlen = end - start;
		if (fragmentlen > 0) {
			for (i=fragmentlen-1; i>=0; i--) {
				/*Seqrch for digit*/
				curdigit = -1;
				for (d=0; d<10; d++) {
					if (digits[0][d] == start[i]) {
						curdigit = digits[1][d];
						break;
					}
				}
				/*Add digit*/
				if (curdigit != -1) {
					res += mult * curdigit;
					mult *= 10;
				} else {
					res = 0;
					break;
				}
			}
		}
	}

	return res;
}

static gchar *mmgui_addressbooks_akonadi_get_substring(gchar *text, gchar *prefix, gchar *suffix)
{
	gchar *start, *end;
	gsize fragmentlen;
	gchar *res;

	if (text == NULL) return NULL;

	start = text;
	res = NULL;

	/*Substring start*/
	if (prefix != NULL) {
		start = strstr(text, prefix);
		if (start != NULL) {
			start += strlen(prefix);
		}
	}

	/*Substring end*/
	end = strstr(start + 1, suffix);

	if (end != NULL) {
		fragmentlen = end - start;
		if (fragmentlen > 0) {
			res = g_malloc0(fragmentlen+1);
			strncpy(res, start, fragmentlen);
		}
	}

	return res;
}

static gboolean mmgui_addressbooks_akonadi_find_substring(gchar *text, gchar *prefix, gchar *suffix, gchar *substring)
{
	gchar *start, *end;
	gsize fragmentlen;
	gboolean res;

	if ((text == NULL) || (substring == NULL)) return FALSE;

	start = text;
	res = FALSE;

	/*Substring start*/
	if (prefix != NULL) {
		start = strstr(text, prefix);
		if (start != NULL) {
			start += strlen(prefix);
		}
	}

	/*Substring end*/
	end = strstr(start + 1, suffix);

	if (end != NULL) {
		fragmentlen = end - start;
		if (fragmentlen > 0) {
			if (strncmp(start, substring, fragmentlen) == 0) {
				res = TRUE;
			}
		}
	}

	return res;
}

static GSList *mmgui_addressbooks_akonadi_get_collections(const gchar *list)
{
	GSList *collections;
	gchar **listrows;
	gint i;
	mmgui_addressbooks_akonadi_collection_t collection;

	if (list == NULL) return NULL;

	listrows = g_strsplit(list, "\r\n", 0);

	if (listrows == NULL) return NULL;

	i = 0;
	collections = NULL;

	while (listrows[i] != NULL) {
		if ((strstr(listrows[i], "application/x-vnd.kde.contactgroup") != NULL) && (strstr(listrows[i], "text/directory") != NULL)) {
			collection = g_new0(struct _mmgui_addressbooks_akonadi_collection, 1);
			collection->id = mmgui_addressbooks_akonadi_get_integer(listrows[i], "* ", " ");
			collection->name = mmgui_addressbooks_akonadi_get_substring(listrows[i], " (NAME \"", "\" MIMETYPE ");
			collections = g_slist_prepend(collections, collection);
		}
		i++;
	}

	g_strfreev(listrows);

	return collections;
}

static void mmgui_addressbooks_akonadi_free_collections_foreach(gpointer data, gpointer user_data)
{
	mmgui_addressbooks_akonadi_collection_t collection;

	collection = (mmgui_addressbooks_akonadi_collection_t)data;

	if (collection->name != NULL) {
		g_free(collection->name);
	}
}

static guint mmgui_addressbooks_akonadi_collection_get_contacts(mmgui_addressbooks_t addressbooks, mmgui_addressbooks_akonadi_collection_t collection, const gchar *vcardlist)
{
	gchar **vcardlistrows;
	GSList *vcardrows;
	gboolean validrow;
	gint i, vcardnum;

	if ((addressbooks == NULL) || (collection == NULL) || (vcardlist == NULL)) return 0;

	vcardlistrows = g_strsplit(vcardlist, "\r\n", 0);

	if (vcardlistrows == NULL) return 0;

	i = 0;
	validrow = FALSE;
	vcardrows = NULL;

	while (vcardlistrows[i] != NULL) {
		if ((vcardlistrows[i][0] == '*') && (mmgui_addressbooks_akonadi_find_substring(vcardlistrows[i], "MIMETYPE \"", "\" COLLECTIONID", "text/directory"))) {
			/*VCard start*/
			validrow = TRUE;
		} else if (vcardlistrows[i][0] == ')') {
			/*VCard end*/
			validrow = FALSE;
		} else {
			if (validrow) {
				vcardrows = g_slist_prepend(vcardrows, vcardlistrows[i]);
			}
		}
		i++;
	}

	if (vcardrows != NULL) {
		vcardrows = g_slist_reverse(vcardrows);
		/*Parse vcards*/
		vcardnum = vcard_parse_list(vcardrows, &(addressbooks->kdecontacts), collection->name);
		/*Free linked list*/
		g_slist_free(vcardrows);
	}

	/*Free strings list*/
	g_strfreev(vcardlistrows);


	return vcardnum;
}

static gboolean mmgui_addressbooks_get_kde_contacts(mmgui_addressbooks_t addressbooks)
{
	struct _mmgui_addressbooks_akonadi_command command;
	guint protocol;
	GSList *collections, *iterator;
	mmgui_addressbooks_akonadi_collection_t collection;

	memset(&command, 0, sizeof(command));
	mmgui_addressbooks_fill_akonadi_command_struct(&command, MMGUI_ADDRESSBOOKS_AKONADI_NULL_COMMAND);
	/*Greeting*/
	if (mmgui_addressbooks_execute_akonadi_command(addressbooks, &command)) {
		if (command.statusid == MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ID_OK) {
			protocol = mmgui_addressbooks_akonadi_get_integer(command.statusmessage, "[PROTOCOL ", "]");
			if (protocol < 33) {
				g_debug("Protocol isn't supported: %u\n", protocol);
				mmgui_addressbooks_free_akonadi_command_struct(&command);
				return FALSE;
			}
		} else {
			g_debug("Unable to get protocol number: %s\n", command.statusmessage);
			mmgui_addressbooks_free_akonadi_command_struct(&command);
			return FALSE;
		}
	} else {
		g_debug("Failed to receive protocol number");
		mmgui_addressbooks_free_akonadi_command_struct(&command);
		return FALSE;
	}

	/*Login*/
	mmgui_addressbooks_fill_akonadi_command_struct(&command, MMGUI_ADDRESSBOOKS_AKONADI_LOGIN_COMMAND);
	if (mmgui_addressbooks_execute_akonadi_command(addressbooks, &command)) {
		if (command.statusid != MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ID_OK) {
			g_debug("Unable to login: %s\n", command.statusmessage);
			mmgui_addressbooks_free_akonadi_command_struct(&command);
			return FALSE;
		}
	} else {
		g_debug("Failed to send login request");
		mmgui_addressbooks_free_akonadi_command_struct(&command);
		return FALSE;
	}

	/*Info*/
	mmgui_addressbooks_fill_akonadi_command_struct(&command, MMGUI_ADDRESSBOOKS_AKONADI_INFO_COMMAND);
	if (mmgui_addressbooks_execute_akonadi_command(addressbooks, &command)) {
		if (command.statusid == MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ID_OK) {
			collections = mmgui_addressbooks_akonadi_get_collections((const gchar *)command.answer);
			if (collections == NULL) {
				g_debug("Unable to get collections identifiers\n");
				mmgui_addressbooks_free_akonadi_command_struct(&command);
				return FALSE;
			}
		} else {
			g_debug("Unable to get server info: %s\n", command.statusmessage);
			mmgui_addressbooks_free_akonadi_command_struct(&command);
			return FALSE;
		}
	} else {
		g_debug("Failed to receive Akonadi information");
		mmgui_addressbooks_free_akonadi_command_struct(&command);
		return FALSE;
	}

	/*Select+Fetch*/
	if (collections != NULL) {
		collections = g_slist_reverse(collections);
		for (iterator = collections; iterator != NULL; iterator = iterator->next) {
			collection = (mmgui_addressbooks_akonadi_collection_t)iterator->data;
			if (collection != NULL) {

				/*Select*/
				mmgui_addressbooks_fill_akonadi_command_struct(&command, MMGUI_ADDRESSBOOKS_AKONADI_SELECT_COMMAND, collection->id);
				if (mmgui_addressbooks_execute_akonadi_command(addressbooks, &command)) {
					if (command.statusid != MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ID_OK) {
						g_debug("Unable to select collection: %s\n", command.statusmessage);
						continue;
					}
				} else {
					g_debug("Failed to send select request");
					continue;
				}

				/*Fetch*/
				mmgui_addressbooks_fill_akonadi_command_struct(&command, MMGUI_ADDRESSBOOKS_AKONADI_FETCH_COMMAND);
				if (mmgui_addressbooks_execute_akonadi_command(addressbooks, &command)) {
					if (command.statusid == MMGUI_ADDRESSBOOKS_AKONADI_STATUS_ID_OK) {
						mmgui_addressbooks_akonadi_collection_get_contacts(addressbooks, collection, (const gchar *)command.answer);
					} else {
						g_debug("Unable to fecth collection data: %s\n", command.statusmessage);
						continue;
					}
				} else {
					g_debug("Failed to send fetch request");
					continue;
				}
			}
		}
		/*Free resources allocated for collections*/
		g_slist_foreach(collections, mmgui_addressbooks_akonadi_free_collections_foreach, NULL);
		g_slist_free(collections);
	}

	/*Free command struct*/
	mmgui_addressbooks_free_akonadi_command_struct(&command);

	return TRUE;
}

//GNOME (Evolution data server)
static void mmgui_addressbooks_get_gnome_contacts_foreach(gpointer data, gpointer user_data)
{
	mmgui_addressbooks_t addressbooks;
	EContact *econtact;
	mmgui_contact_t contact;
	const gchar *fullname, *nickname, *primaryphone, *mobilephone, *email;
	GList *emails;

	addressbooks = (mmgui_addressbooks_t)user_data;
	econtact = (EContact *)data;

	if ((addressbooks == NULL) || (econtact == NULL)) return;

	fullname = (addressbooks->e_contact_get_const)(econtact, E_CONTACT_FULL_NAME);
	nickname = (addressbooks->e_contact_get_const)(econtact, E_CONTACT_NICKNAME);
	primaryphone = (addressbooks->e_contact_get_const)(econtact, E_CONTACT_PHONE_HOME);
	mobilephone = (addressbooks->e_contact_get_const)(econtact, E_CONTACT_PHONE_MOBILE);
	emails = (addressbooks->e_contact_get)(econtact, E_CONTACT_EMAIL);
	email = g_list_nth_data(emails, 0);

	contact = g_new0(struct _mmgui_contact, 1);

	contact->name = g_strdup(fullname);
	contact->number = g_strdup(primaryphone);
	contact->email = g_strdup(email);
	contact->group = g_strdup(addressbooks->gnomesourcename);
	contact->name2 = g_strdup(nickname);
	contact->number2 = g_strdup(mobilephone);
	contact->id = addressbooks->counter;
	contact->hidden = FALSE;
	contact->storage = MMGUI_CONTACTS_STORAGE_UNKNOWN;

	addressbooks->gnomecontacts = g_slist_prepend(addressbooks->gnomecontacts, contact);

	addressbooks->counter++;

	g_list_foreach(emails, (GFunc)g_free, NULL);
	g_list_free(emails);
}

static gboolean mmgui_addressbooks_get_gnome_contacts(mmgui_addressbooks_t addressbooks, mmgui_libpaths_cache_t libcache)
{
	EBookQuery *queryelements[2];
	EBookQuery *query;
	GError *error;
	gchar *s;
	/*New API*/
	ESourceRegistry *registry;
	ESource *source;
	EBookClient *client;
	GSList *scontacts;
	/*Old API*/
	EBook *book;
	GList *dcontacts;

	if ((addressbooks == NULL) || (libcache == NULL)) return FALSE;
	if (!addressbooks->gnomesupported) return FALSE;
	if (addressbooks->ebookmodule == NULL) return FALSE;

	error = NULL;

	if (!mmgui_libpaths_cache_check_library_version(libcache, "libebook-1.2", 12, 3, 0)) {
		g_debug("GNOME contacts API isn't supported\n");
		return FALSE;
	}

	queryelements[0] = (addressbooks->e_book_query_field_exists)(E_CONTACT_PHONE_HOME);
	queryelements[1] = (addressbooks->e_book_query_field_exists)(E_CONTACT_PHONE_MOBILE);

	query = (addressbooks->e_book_query_or)(2, queryelements, TRUE);
	if (query == NULL) {
		g_debug("Failed to form GNOME contacts query\n");
		return FALSE;
	}

	if (mmgui_libpaths_cache_check_library_version(libcache, "libebook-1.2", 14, 3, 0)) {
		g_debug("Using GNOME contacts API version 3.6 ... ?\n");
		registry = (addressbooks->e_source_registry_new_sync)(NULL, &error);
		if (registry == NULL) {
			(addressbooks->e_book_query_unref)(query);
			g_debug("Failed to get ESourceRegistry: %s\n", error->message);
			g_error_free(error);
			return FALSE;
		}

		source = (addressbooks->e_source_registry_ref_builtin_address_book)(registry);
		if (source == NULL) {
			(addressbooks->e_book_query_unref)(query);
			g_debug("Failed to get ESource\n");
			return FALSE;
		}

		addressbooks->gnomesourcename = (addressbooks->e_source_get_display_name)(source);

		if (mmgui_libpaths_cache_check_library_version(libcache, "libebook-1.2", 16, 3, 0)) {
			/*Version 3.16 ... ?*/
			client = (EBookClient *)(addressbooks->e_book_client_connect_sync316)(source, MMGUI_ADDRESSBOOKS_GNOME_CONNECT_TIMEOUT, NULL, &error);
			if (client == NULL) {
				(addressbooks->e_book_query_unref)(query);
				g_debug("Failed to get EBookClient: %s\n", error->message);
				g_error_free(error);
				return FALSE;
			}
		} else {
			/*Version 3.8 ... 3.16*/
			client = (EBookClient *)(addressbooks->e_book_client_connect_sync)(source, NULL, &error);
			if (client == NULL) {
				(addressbooks->e_book_query_unref)(query);
				g_debug("Failed to get EBookClient: %s\n", error->message);
				g_error_free(error);
				return FALSE;
			}
		}

		s = (addressbooks->e_book_query_to_string)(query);
		if (s == NULL) {
			(addressbooks->e_book_query_unref)(query);
			g_debug("Failed to get GNOME addressbook request in string format\n");
			return FALSE;
		}

		g_debug("GNOME addressbook request: %s\n", s);

		if (!(addressbooks->e_book_client_get_contacts_sync)(client, s, &scontacts, NULL, &error)) {
			(addressbooks->e_book_query_unref)(query);
			g_debug("Failed to get GNOME addressbook query results: %s\n", error->message);
			g_error_free(error);
			return FALSE;
		}
	} else if (mmgui_libpaths_cache_check_library_version(libcache, "libebook-1.2", 13, 3, 0)) {
		/*Versions 3.4 ... 3.8*/
		g_debug("Using GNOME contacts API version 3.4 .. 3.6\n");
		registry = (addressbooks->e_source_registry_new_sync)(NULL, &error);
		if (registry == NULL) {
			(addressbooks->e_book_query_unref)(query);
			g_debug("Failed to get ESourceRegistry: %s\n", error->message);
			g_error_free(error);
			return FALSE;
		}

		source = (addressbooks->e_source_registry_ref_builtin_address_book)(registry);
		if (source == NULL) {
			(addressbooks->e_book_query_unref)(query);
			g_debug("Failed to get ESource\n");
			return FALSE;
		}

		addressbooks->gnomesourcename = (addressbooks->e_source_get_display_name)(source);

		client = (addressbooks->e_book_client_new)(source, &error);
		if (client == NULL) {
			(addressbooks->e_book_query_unref)(query);
			g_debug("Failed to get EBookClient: %s\n", error->message);
			g_error_free(error);
			return FALSE;
		}

		if (!(addressbooks->e_client_open_sync)((EClient *)client, TRUE, NULL, &error)) {
			(addressbooks->e_book_query_unref)(query);
			g_debug("Failed to open EBookClient: %s\n", error->message);
			g_error_free(error);
			return FALSE;
		}

		s = (addressbooks->e_book_query_to_string)(query);
		if (s == NULL) {
			(addressbooks->e_book_query_unref)(query);
			g_debug("Failed to get GNOME addressbook request in string format\n");
			return FALSE;
		}

		g_debug("GNOME addressbook request: %s\n", s);

		if (!(addressbooks->e_book_client_get_contacts_sync)(client, s, &scontacts, NULL, &error)) {
			(addressbooks->e_book_query_unref)(query);
			g_debug("Failed to get GNOME addressbook query results: %s\n", error->message);
			g_error_free(error);
			return FALSE;
		}
	} else {
		/*Versions 3.0 ... 3.4*/
		g_debug("Using GNOME contacts API version 3.0 .. 3.4\n");
		book = (addressbooks->e_book_new_system_addressbook)(&error);
		if (book == NULL) {
			(addressbooks->e_book_query_unref)(query);
			g_debug("Failed to load GNOME system addressbook: %s\n", error->message);
			g_error_free(error);
			return FALSE;
		}

		if (!(addressbooks->e_book_open)(book, TRUE, &error)) {
			(addressbooks->e_book_query_unref)(query);
			g_debug("Failed to load GNOME system addressbook: %s\n", error->message);
			g_error_free(error);
			return FALSE;
		}

		source = (addressbooks->e_book_get_source)(book);

		addressbooks->gnomesourcename = (addressbooks->e_source_get_display_name)(source);

		if (!(addressbooks->e_book_get_contacts)(book, query, &dcontacts, &error)) {
			(addressbooks->e_book_query_unref)(query);
			g_debug("Failed to get query GNOME addressbook results: %s\n", error->message);
			g_error_free(error);
			return FALSE;
		}
	}

	(addressbooks->e_book_query_unref)(query);

	if (mmgui_libpaths_cache_check_library_version(libcache, "libebook-1.2", 13, 3, 0)) {
		if (scontacts != NULL) {
			addressbooks->counter = 0;
			g_slist_foreach(scontacts, (GFunc)mmgui_addressbooks_get_gnome_contacts_foreach, addressbooks);
		} else {
			g_debug("No suitable contacts found\n");
		}
	} else {
		if (dcontacts != NULL) {
			addressbooks->counter = 0;
			g_list_foreach(dcontacts, (GFunc)mmgui_addressbooks_get_gnome_contacts_foreach, addressbooks);
		} else {
			g_debug("No suitable contacts found\n");
		}
	}

	addressbooks->counter = 0;

	return TRUE;
}

static gpointer mmguicore_addressbooks_work_thread(gpointer data)
{
	mmgui_addressbooks_t addressbooks;
	gboolean result;

	addressbooks = (mmgui_addressbooks_t)data;

	if (addressbooks == NULL) return NULL;

	result = TRUE;

	if (addressbooks->gnomesupported) {
		result = result && mmgui_addressbooks_get_gnome_contacts(addressbooks, addressbooks->libcache);
	}

	if (addressbooks->kdesupported) {
		result = result && mmgui_addressbooks_get_kde_contacts(addressbooks);
	}

	if (result) {
		if (addressbooks->callback != NULL) {
			(addressbooks->callback)(MMGUI_EVENT_ADDRESS_BOOKS_EXPORT_FINISHED, NULL, NULL, addressbooks->userdata);
		}
	}

	addressbooks->workthread = NULL;

	return NULL;
}

mmgui_addressbooks_t mmgui_addressbooks_new(mmgui_event_ext_callback callback, mmgui_libpaths_cache_t libcache, gpointer userdata)
{
	mmgui_addressbooks_t addressbooks;
	gboolean libopened;
	guint akonadistatus;
	const gchar *desktop;
	const gchar *version;
	
	if (callback == NULL) return NULL;

	addressbooks = g_new0(struct _mmgui_addressbooks, 1);

	addressbooks->callback = callback;
	addressbooks->libcache = libcache;
	addressbooks->userdata = userdata;
	addressbooks->workthread = NULL;
	
	/*Get name of current desktop enviroment*/
	desktop = getenv("XDG_CURRENT_DESKTOP");
	
	/*libebook*/
	addressbooks->ebookmodule = NULL;
	addressbooks->gnomesupported = FALSE;
	addressbooks->gnomecontacts = NULL;
	
	/*GNOME code path*/
	if (g_strrstr(desktop, "GNOME") != NULL) {
		if (mmgui_libpaths_cache_check_library_version(libcache, "libebook-1.2", 12, 3, 0)) {
			/*Open module*/
			addressbooks->ebookmodule = g_module_open(mmgui_libpaths_cache_get_library_full_path(libcache, "libebook-1.2"), G_MODULE_BIND_LAZY);
			if (addressbooks->ebookmodule != NULL) {
				libopened = TRUE;
				libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_query_field_exists", (gpointer *)&(addressbooks->e_book_query_field_exists));
				libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_query_or", (gpointer *)&(addressbooks->e_book_query_or));
				libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_query_unref", (gpointer *)&(addressbooks->e_book_query_unref));
				libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_contact_get_const", (gpointer *)&(addressbooks->e_contact_get_const));
				libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_contact_get", (gpointer *)&(addressbooks->e_contact_get));
				if ((libopened) && (mmgui_libpaths_cache_check_library_version(libcache, "libebook-1.2", 16, 3, 0))) {
					/*Version 3.16 ... ?*/
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_client_connect_sync", (gpointer *)&(addressbooks->e_book_client_connect_sync316));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_source_registry_new_sync", (gpointer *)&(addressbooks->e_source_registry_new_sync));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_source_registry_ref_builtin_address_book", (gpointer *)&(addressbooks->e_source_registry_ref_builtin_address_book));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_query_to_string", (gpointer *)&(addressbooks->e_book_query_to_string));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_client_get_contacts_sync", (gpointer *)&(addressbooks->e_book_client_get_contacts_sync));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_source_get_display_name", (gpointer *)&(addressbooks->e_source_get_display_name));
					addressbooks->e_book_client_connect_sync = NULL;
					addressbooks->e_book_new_system_addressbook = NULL;
					addressbooks->e_book_open = NULL;
					addressbooks->e_book_get_contacts = NULL;
					addressbooks->e_book_get_source = NULL;
				} else if ((libopened) && (mmgui_libpaths_cache_check_library_version(libcache, "libebook-1.2", 14, 3, 0))) {
					/*Version 3.8 ... 3.16*/
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_client_connect_sync", (gpointer *)&(addressbooks->e_book_client_connect_sync));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_source_registry_new_sync", (gpointer *)&(addressbooks->e_source_registry_new_sync));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_source_registry_ref_builtin_address_book", (gpointer *)&(addressbooks->e_source_registry_ref_builtin_address_book));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_query_to_string", (gpointer *)&(addressbooks->e_book_query_to_string));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_client_get_contacts_sync", (gpointer *)&(addressbooks->e_book_client_get_contacts_sync));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_source_get_display_name", (gpointer *)&(addressbooks->e_source_get_display_name));
					addressbooks->e_book_client_connect_sync316 = NULL;
					addressbooks->e_book_new_system_addressbook = NULL;
					addressbooks->e_book_open = NULL;
					addressbooks->e_book_get_contacts = NULL;
					addressbooks->e_book_get_source = NULL;
				} else if ((libopened) && (mmgui_libpaths_cache_check_library_version(libcache, "libebook-1.2", 13, 3, 0))) {
					/*Versions 3.4 ... 3.8*/
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_client_new", (gpointer *)&(addressbooks->e_book_client_new));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_client_open_sync", (gpointer *)&(addressbooks->e_client_open_sync));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_source_registry_new_sync", (gpointer *)&(addressbooks->e_source_registry_new_sync));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_source_registry_ref_builtin_address_book", (gpointer *)&(addressbooks->e_source_registry_ref_builtin_address_book));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_query_to_string", (gpointer *)&(addressbooks->e_book_query_to_string));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_client_get_contacts_sync", (gpointer *)&(addressbooks->e_book_client_get_contacts_sync));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_source_peek_name", (gpointer *)&(addressbooks->e_source_get_display_name));
					addressbooks->e_book_client_connect_sync = NULL;
					addressbooks->e_book_client_connect_sync316 = NULL;
					addressbooks->e_book_new_system_addressbook = NULL;
					addressbooks->e_book_open = NULL;
					addressbooks->e_book_get_contacts = NULL;
					addressbooks->e_book_get_source = NULL;
				} else if (libopened) {
					/*Versions 3.0 ... 3.4*/
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_new_system_addressbook", (gpointer *)&(addressbooks->e_book_new_system_addressbook));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_open", (gpointer *)&(addressbooks->e_book_open));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_get_contacts", (gpointer *)&(addressbooks->e_book_get_contacts));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_book_get_source", (gpointer *)&(addressbooks->e_book_get_source));
					libopened = libopened && g_module_symbol(addressbooks->ebookmodule, "e_source_peek_name", (gpointer *)&(addressbooks->e_source_get_display_name));
					addressbooks->e_book_client_connect_sync = NULL;
					addressbooks->e_book_client_connect_sync316 = NULL;
					addressbooks->e_source_registry_new_sync = NULL;
					addressbooks->e_source_registry_ref_builtin_address_book = NULL;
					addressbooks->e_book_client_new = NULL;
					addressbooks->e_client_open_sync = NULL;
					addressbooks->e_book_query_to_string = NULL;
					addressbooks->e_book_client_get_contacts_sync = NULL;
				}
				/*If some functions not exported, close library*/
				if (!libopened) {
					addressbooks->e_book_query_field_exists = NULL;
					addressbooks->e_book_query_or = NULL;
					addressbooks->e_source_registry_new_sync = NULL;
					addressbooks->e_source_registry_ref_builtin_address_book = NULL;
					addressbooks->e_source_get_display_name = NULL;
					addressbooks->e_book_client_new = NULL;
					addressbooks->e_client_open_sync = NULL;
					addressbooks->e_book_query_to_string = NULL;
					addressbooks->e_book_client_get_contacts_sync = NULL;
					addressbooks->e_book_new_system_addressbook = NULL;
					addressbooks->e_book_open = NULL;
					addressbooks->e_book_get_contacts = NULL;
					addressbooks->e_book_query_unref = NULL;
					addressbooks->e_contact_get_const = NULL;
					addressbooks->e_contact_get = NULL;
					/*Close module*/
					g_module_close(addressbooks->ebookmodule);
					addressbooks->ebookmodule = NULL;
					addressbooks->gnomesupported = FALSE;
				} else {
					/*Get contacts*/
					addressbooks->gnomesupported = TRUE;
					//mmgui_addressbooks_get_gnome_contacts(addressbooks, libcache);
				}
			}
		}
	}

	/*KDE addressbook*/
	addressbooks->kdecontacts = NULL;
	addressbooks->kdesupported = FALSE;
	
	/*KDE code path*/
	if (g_strcmp0(desktop, "KDE") == 0) {
		/*Get KDE version information*/
		version = getenv("KDE_SESSION_VERSION");
		/*For now only KDE 4 version supported*/
		if (g_strcmp0(version, "4") == 0) {
			if (mmgui_dbus_utils_session_service_activate(MMGUI_ADDRESSBOOKS_AKONADI_DBUS_INTERFACE, &akonadistatus)) {
				/*Open socket*/
				addressbooks->aksocket = mmgui_addressbooks_open_kde_socket();
				if (addressbooks->aksocket != -1) {
					/*Akonadi server started*/
					addressbooks->kdesupported = TRUE;
					//mmgui_addressbooks_get_kde_contacts(addressbooks);
				} else {
					/*Akonadi server not available*/
					addressbooks->kdesupported = FALSE;
				}
			}
		}
	}

	if ((addressbooks->gnomesupported) || (addressbooks->kdesupported)) {
		#if GLIB_CHECK_VERSION(2,32,0)
			addressbooks->workthread = g_thread_new("Modem Manager GUI contacts export thread", mmguicore_addressbooks_work_thread, addressbooks);
		#else
			addressbooks->workthread = g_thread_create(mmguicore_addressbooks_work_thread, addressbooks, TRUE, NULL);
		#endif
	}

	return addressbooks;
}

gboolean mmgui_addressbooks_get_gnome_contacts_available(mmgui_addressbooks_t addressbooks)
{
	if (addressbooks == NULL) return FALSE;

	return addressbooks->gnomesupported;
}

gboolean mmgui_addressbooks_get_kde_contacts_available(mmgui_addressbooks_t addressbooks)
{
	if (addressbooks == NULL) return FALSE;

	return addressbooks->kdesupported;
}

GSList *mmgui_addressbooks_get_gnome_contacts_list(mmgui_addressbooks_t addressbooks)
{
	if (addressbooks == NULL) return NULL;
	if (!addressbooks->gnomesupported) return NULL;

	return addressbooks->gnomecontacts;
}

GSList *mmgui_addressbooks_get_kde_contacts_list(mmgui_addressbooks_t addressbooks)
{
	if (addressbooks == NULL) return NULL;
	if (!addressbooks->kdesupported) return NULL;

	return addressbooks->kdecontacts;
}


static gint mmgui_addressbooks_get_contact_compare(gconstpointer a, gconstpointer b)
{
	mmgui_contact_t contact;
	guint id;

	contact = (mmgui_contact_t)a;
	id = GPOINTER_TO_UINT(b);

	if (contact->id < id) {
		return 1;
	} else if (contact->id > id) {
		return -1;
	} else {
		return 0;
	}
}

mmgui_contact_t mmgui_addressbooks_get_gnome_contact(mmgui_addressbooks_t addressbooks, guint index)
{
	GSList *contactptr;
	mmgui_contact_t contact;

	if (addressbooks == NULL) return NULL;
	if (!addressbooks->gnomesupported) return NULL;
	if (addressbooks->gnomecontacts == NULL) return NULL;

	contactptr = g_slist_find_custom(addressbooks->gnomecontacts, GUINT_TO_POINTER(index), mmgui_addressbooks_get_contact_compare);

	if (contactptr != NULL) {
		contact = (mmgui_contact_t)contactptr->data;
		return contact;
	} else {
		return NULL;
	}
}

mmgui_contact_t mmgui_addressbooks_get_kde_contact(mmgui_addressbooks_t addressbooks, guint index)
{
	GSList *contactptr;
	mmgui_contact_t contact;

	if (addressbooks == NULL) return NULL;
	if (!addressbooks->kdesupported) return NULL;
	if (addressbooks->kdecontacts == NULL) return NULL;

	contactptr = g_slist_find_custom(addressbooks->kdecontacts, GUINT_TO_POINTER(index), mmgui_addressbooks_get_contact_compare);

	if (contactptr != NULL) {
		contact = (mmgui_contact_t)contactptr->data;
		return contact;
	} else {
		return NULL;
	}
}

static void mmgui_addressbooks_free_contacts_list_foreach(gpointer data, gpointer user_data)
{
	mmgui_contact_t contact;

	if (data == NULL) return;

	contact = (mmgui_contact_t)data;

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
}

void mmgui_addressbooks_close(mmgui_addressbooks_t addressbooks)
{
	if (addressbooks == NULL) return;

	/*Wait for thread*/
	if (addressbooks->workthread != NULL) {
		g_thread_join(addressbooks->workthread);
	}

	/*GNOME addressbook*/
	addressbooks->gnomesupported = FALSE;
	if (addressbooks->ebookmodule != NULL) {
		/*First free contacts list*/
		if (addressbooks->gnomecontacts != NULL) {
			g_slist_foreach(addressbooks->gnomecontacts, mmgui_addressbooks_free_contacts_list_foreach, NULL);
			g_slist_free(addressbooks->gnomecontacts);
			addressbooks->gnomecontacts = NULL;
		}
		/*Then unload module*/
		g_module_close(addressbooks->ebookmodule);
		addressbooks->ebookmodule = NULL;
	}

	/*KDE addressbook*/
	addressbooks->kdesupported = FALSE;
	if (addressbooks->kdecontacts != NULL) {
		/*Only free contacts list*/
		g_slist_foreach(addressbooks->kdecontacts, mmgui_addressbooks_free_contacts_list_foreach, NULL);
		g_slist_free(addressbooks->kdecontacts);
		addressbooks->kdecontacts = NULL;
	}

	g_free(addressbooks);
}
