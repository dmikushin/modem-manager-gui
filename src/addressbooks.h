/*
 *      addressbooks.h
 *      
 *      Copyright 2013 Alex <alex@linuxonly.ru>
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

#ifndef __ADDRESSBOOKS_H__
#define __ADDRESSBOOKS_H__

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <gmodule.h>

#include "mmguicore.h"    /*For contact structure definition*/
#include "libpaths.h"

/*GNOME addressbook contact fields*/
typedef enum {

	E_CONTACT_UID = 1,	 /* string field */
	E_CONTACT_FILE_AS,	 /* string field */
	E_CONTACT_BOOK_UID,      /* string field */

	/* Name fields */
	E_CONTACT_FULL_NAME,	 /* string field */
	E_CONTACT_GIVEN_NAME,	 /* synthetic string field */
	E_CONTACT_FAMILY_NAME,	 /* synthetic string field */
	E_CONTACT_NICKNAME,	 /* string field */

	/* Email fields */
	E_CONTACT_EMAIL_1,	 /* synthetic string field */
	E_CONTACT_EMAIL_2,	 /* synthetic string field */
	E_CONTACT_EMAIL_3,	 /* synthetic string field */
	E_CONTACT_EMAIL_4,       /* synthetic string field */

	E_CONTACT_MAILER,        /* string field */

	/* Address Labels */
	E_CONTACT_ADDRESS_LABEL_HOME,  /* synthetic string field */
	E_CONTACT_ADDRESS_LABEL_WORK,  /* synthetic string field */
	E_CONTACT_ADDRESS_LABEL_OTHER, /* synthetic string field */

	/* Phone fields */
	E_CONTACT_PHONE_ASSISTANT,
	E_CONTACT_PHONE_BUSINESS,
	E_CONTACT_PHONE_BUSINESS_2,
	E_CONTACT_PHONE_BUSINESS_FAX,
	E_CONTACT_PHONE_CALLBACK,
	E_CONTACT_PHONE_CAR,
	E_CONTACT_PHONE_COMPANY,
	E_CONTACT_PHONE_HOME,
	E_CONTACT_PHONE_HOME_2,
	E_CONTACT_PHONE_HOME_FAX,
	E_CONTACT_PHONE_ISDN,
	E_CONTACT_PHONE_MOBILE,
	E_CONTACT_PHONE_OTHER,
	E_CONTACT_PHONE_OTHER_FAX,
	E_CONTACT_PHONE_PAGER,
	E_CONTACT_PHONE_PRIMARY,
	E_CONTACT_PHONE_RADIO,
	E_CONTACT_PHONE_TELEX,
	E_CONTACT_PHONE_TTYTDD,

	/* Organizational fields */
	E_CONTACT_ORG,		 /* string field */
	E_CONTACT_ORG_UNIT,	 /* string field */
	E_CONTACT_OFFICE,	 /* string field */
	E_CONTACT_TITLE,	 /* string field */
	E_CONTACT_ROLE,	 /* string field */
	E_CONTACT_MANAGER,	 /* string field */
	E_CONTACT_ASSISTANT,	 /* string field */

	/* Web fields */
	E_CONTACT_HOMEPAGE_URL,  /* string field */
	E_CONTACT_BLOG_URL,      /* string field */

	/* Contact categories */
	E_CONTACT_CATEGORIES,    /* string field */

	/* Collaboration fields */
	E_CONTACT_CALENDAR_URI,  /* string field */
	E_CONTACT_FREEBUSY_URL,  /* string field */
	E_CONTACT_ICS_CALENDAR,  /* string field */
	E_CONTACT_VIDEO_URL,      /* string field */

	/* misc fields */
	E_CONTACT_SPOUSE,        /* string field */
	E_CONTACT_NOTE,          /* string field */

	E_CONTACT_IM_AIM_HOME_1,       /* Synthetic string field */
	E_CONTACT_IM_AIM_HOME_2,       /* Synthetic string field */
	E_CONTACT_IM_AIM_HOME_3,       /* Synthetic string field */
	E_CONTACT_IM_AIM_WORK_1,       /* Synthetic string field */
	E_CONTACT_IM_AIM_WORK_2,       /* Synthetic string field */
	E_CONTACT_IM_AIM_WORK_3,       /* Synthetic string field */
	E_CONTACT_IM_GROUPWISE_HOME_1, /* Synthetic string field */
	E_CONTACT_IM_GROUPWISE_HOME_2, /* Synthetic string field */
	E_CONTACT_IM_GROUPWISE_HOME_3, /* Synthetic string field */
	E_CONTACT_IM_GROUPWISE_WORK_1, /* Synthetic string field */
	E_CONTACT_IM_GROUPWISE_WORK_2, /* Synthetic string field */
	E_CONTACT_IM_GROUPWISE_WORK_3, /* Synthetic string field */
	E_CONTACT_IM_JABBER_HOME_1,    /* Synthetic string field */
	E_CONTACT_IM_JABBER_HOME_2,    /* Synthetic string field */
	E_CONTACT_IM_JABBER_HOME_3,    /* Synthetic string field */
	E_CONTACT_IM_JABBER_WORK_1,    /* Synthetic string field */
	E_CONTACT_IM_JABBER_WORK_2,    /* Synthetic string field */
	E_CONTACT_IM_JABBER_WORK_3,    /* Synthetic string field */
	E_CONTACT_IM_YAHOO_HOME_1,     /* Synthetic string field */
	E_CONTACT_IM_YAHOO_HOME_2,     /* Synthetic string field */
	E_CONTACT_IM_YAHOO_HOME_3,     /* Synthetic string field */
	E_CONTACT_IM_YAHOO_WORK_1,     /* Synthetic string field */
	E_CONTACT_IM_YAHOO_WORK_2,     /* Synthetic string field */
	E_CONTACT_IM_YAHOO_WORK_3,     /* Synthetic string field */
	E_CONTACT_IM_MSN_HOME_1,       /* Synthetic string field */
	E_CONTACT_IM_MSN_HOME_2,       /* Synthetic string field */
	E_CONTACT_IM_MSN_HOME_3,       /* Synthetic string field */
	E_CONTACT_IM_MSN_WORK_1,       /* Synthetic string field */
	E_CONTACT_IM_MSN_WORK_2,       /* Synthetic string field */
	E_CONTACT_IM_MSN_WORK_3,       /* Synthetic string field */
	E_CONTACT_IM_ICQ_HOME_1,       /* Synthetic string field */
	E_CONTACT_IM_ICQ_HOME_2,       /* Synthetic string field */
	E_CONTACT_IM_ICQ_HOME_3,       /* Synthetic string field */
	E_CONTACT_IM_ICQ_WORK_1,       /* Synthetic string field */
	E_CONTACT_IM_ICQ_WORK_2,       /* Synthetic string field */
	E_CONTACT_IM_ICQ_WORK_3,       /* Synthetic string field */

	/* Convenience field for getting a name from the contact.
	 * Returns the first one of[File-As, Full Name, Org, Email1]
	 * to be set */
	E_CONTACT_REV,     /* string field to hold  time of last update to this vcard */
	E_CONTACT_NAME_OR_ORG,

	/* Address fields */
	E_CONTACT_ADDRESS,       /* Multi-valued structured (EContactAddress) */
	E_CONTACT_ADDRESS_HOME,  /* synthetic structured field (EContactAddress) */
	E_CONTACT_ADDRESS_WORK,  /* synthetic structured field (EContactAddress) */
	E_CONTACT_ADDRESS_OTHER, /* synthetic structured field (EContactAddress) */

	E_CONTACT_CATEGORY_LIST, /* multi-valued */

	/* Photo/Logo */
	E_CONTACT_PHOTO,	 /* structured field (EContactPhoto) */
	E_CONTACT_LOGO,	 /* structured field (EContactPhoto) */

	E_CONTACT_NAME,		 /* structured field (EContactName) */
	E_CONTACT_EMAIL,	 /* Multi-valued */

	/* Instant Messaging fields */
	E_CONTACT_IM_AIM,	 /* Multi-valued */
	E_CONTACT_IM_GROUPWISE,  /* Multi-valued */
	E_CONTACT_IM_JABBER,	 /* Multi-valued */
	E_CONTACT_IM_YAHOO,	 /* Multi-valued */
	E_CONTACT_IM_MSN,	 /* Multi-valued */
	E_CONTACT_IM_ICQ,	 /* Multi-valued */

	E_CONTACT_WANTS_HTML,    /* boolean field */

	/* fields used for describing contact lists.  a contact list
	 * is just a contact with _IS_LIST set to true.  the members
	 * are listed in the _EMAIL field. */
	E_CONTACT_IS_LIST,             /* boolean field */
	E_CONTACT_LIST_SHOW_ADDRESSES, /* boolean field */

	E_CONTACT_BIRTH_DATE,    /* structured field (EContactDate) */
	E_CONTACT_ANNIVERSARY,   /* structured field (EContactDate) */

	/* Security Fields */
	E_CONTACT_X509_CERT,     /* structured field (EContactCert) */

	E_CONTACT_IM_GADUGADU_HOME_1,  /* Synthetic string field */
	E_CONTACT_IM_GADUGADU_HOME_2,  /* Synthetic string field */
	E_CONTACT_IM_GADUGADU_HOME_3,  /* Synthetic string field */
	E_CONTACT_IM_GADUGADU_WORK_1,  /* Synthetic string field */
	E_CONTACT_IM_GADUGADU_WORK_2,  /* Synthetic string field */
	E_CONTACT_IM_GADUGADU_WORK_3,  /* Synthetic string field */

	E_CONTACT_IM_GADUGADU,   /* Multi-valued */

	E_CONTACT_GEO,	/* structured field (EContactGeo) */

	E_CONTACT_TEL, /* list of strings */

	E_CONTACT_IM_SKYPE_HOME_1,     /* Synthetic string field */
	E_CONTACT_IM_SKYPE_HOME_2,     /* Synthetic string field */
	E_CONTACT_IM_SKYPE_HOME_3,     /* Synthetic string field */
	E_CONTACT_IM_SKYPE_WORK_1,     /* Synthetic string field */
	E_CONTACT_IM_SKYPE_WORK_2,     /* Synthetic string field */
	E_CONTACT_IM_SKYPE_WORK_3,     /* Synthetic string field */
	E_CONTACT_IM_SKYPE,		/* Multi-valued */

	E_CONTACT_SIP,

	E_CONTACT_IM_GOOGLE_TALK_HOME_1,     /* Synthetic string field */
	E_CONTACT_IM_GOOGLE_TALK_HOME_2,     /* Synthetic string field */
	E_CONTACT_IM_GOOGLE_TALK_HOME_3,     /* Synthetic string field */
	E_CONTACT_IM_GOOGLE_TALK_WORK_1,     /* Synthetic string field */
	E_CONTACT_IM_GOOGLE_TALK_WORK_2,     /* Synthetic string field */
	E_CONTACT_IM_GOOGLE_TALK_WORK_3,     /* Synthetic string field */
	E_CONTACT_IM_GOOGLE_TALK,		/* Multi-valued */

	E_CONTACT_IM_TWITTER,		/* Multi-valued */

	E_CONTACT_FIELD_LAST,
	E_CONTACT_FIELD_FIRST        = E_CONTACT_UID,

	/* useful constants */
	E_CONTACT_LAST_SIMPLE_STRING = E_CONTACT_NAME_OR_ORG,
	E_CONTACT_FIRST_PHONE_ID     = E_CONTACT_PHONE_ASSISTANT,
	E_CONTACT_LAST_PHONE_ID      = E_CONTACT_PHONE_TTYTDD,
	E_CONTACT_FIRST_EMAIL_ID     = E_CONTACT_EMAIL_1,
	E_CONTACT_LAST_EMAIL_ID      = E_CONTACT_EMAIL_4,
	E_CONTACT_FIRST_ADDRESS_ID   = E_CONTACT_ADDRESS_HOME,
	E_CONTACT_LAST_ADDRESS_ID    = E_CONTACT_ADDRESS_OTHER,
	E_CONTACT_FIRST_LABEL_ID     = E_CONTACT_ADDRESS_LABEL_HOME,
	E_CONTACT_LAST_LABEL_ID      = E_CONTACT_ADDRESS_LABEL_OTHER
} EContactField;

/*GNOME addressbook structures*/
typedef struct _ESource ESource;
typedef struct _ESourceRegistry ESourceRegistry;
typedef struct _EBook EBook;
typedef struct _EBookClient EBookClient;
typedef struct _EClient EClient;
typedef struct _EBookQuery EBookQuery;
typedef struct _EContact EContact;

/*GNOME addressbook function prototypes*/
typedef EBookQuery *(*e_book_query_field_exists_func)(EContactField field);
typedef EBookQuery *(*e_book_query_or_func)(gint nqs, EBookQuery **qs, gboolean unref);
typedef ESourceRegistry *(*e_source_registry_new_sync_func)(gpointer cancellable, GError **error);
typedef ESource *(*e_source_registry_ref_builtin_address_book_func)(ESourceRegistry *registry);
typedef const gchar *(*e_source_get_display_name_func)(ESource *source);
typedef EBookClient *(*e_book_client_new_func)(ESource *source, GError **error);
typedef gboolean (*e_client_open_sync_func)(EClient *client, gboolean only_if_exists, gpointer cancellable, GError **error);
typedef EClient *(*e_book_client_connect_sync_func)(ESource *source, gpointer cancellable, GError **error);
typedef EClient *(*e_book_client_connect_sync316_func)(ESource *source, guint32 wait_for_connected_seconds, gpointer cancellable, GError **error);
typedef gchar *(*e_book_query_to_string_func)(EBookQuery *q);
typedef gboolean (*e_book_client_get_contacts_sync_func)(EBookClient *client, const gchar *sexp, GSList **out_contacts, gpointer cancellable, GError **error);
typedef EBook *(*e_book_new_system_addressbook_func)(GError **error);
typedef ESource *(*e_book_get_source_func)(EBook *book);
typedef gboolean (*e_book_open_func)(EBook *book, gboolean only_if_exists, GError **error);
typedef gboolean (*e_book_get_contacts_func)(EBook *book, EBookQuery *query, GList **contacts, GError **error);
typedef void (*e_book_query_unref_func)(EBookQuery *q);
typedef gconstpointer (*e_contact_get_const_func)(EContact *contact, EContactField field_id);
typedef gpointer (*e_contact_get_func)(EContact *contact, EContactField field_id);

struct _mmgui_addressbooks {
	//Modules
	GModule *ebookmodule;
	//GNOME addressbook functions
	e_book_query_field_exists_func e_book_query_field_exists;
	e_book_query_or_func e_book_query_or;
	e_source_registry_new_sync_func e_source_registry_new_sync;
	e_source_registry_ref_builtin_address_book_func e_source_registry_ref_builtin_address_book;
	e_source_get_display_name_func e_source_get_display_name;
	e_book_client_new_func e_book_client_new;
	e_client_open_sync_func e_client_open_sync;
	e_book_client_connect_sync_func e_book_client_connect_sync;
	e_book_client_connect_sync316_func e_book_client_connect_sync316;
	e_book_query_to_string_func e_book_query_to_string;
	e_book_client_get_contacts_sync_func e_book_client_get_contacts_sync;
	e_book_new_system_addressbook_func e_book_new_system_addressbook;
	e_book_get_source_func e_book_get_source;
	e_book_open_func e_book_open;
	e_book_get_contacts_func e_book_get_contacts;
	e_book_query_unref_func e_book_query_unref;
	e_contact_get_const_func e_contact_get_const;
	e_contact_get_func e_contact_get;
	//GNOME stuff
	gboolean gnomesupported;
	GSList *gnomecontacts;
	const gchar *gnomesourcename;
	//Akonadi access data
	gint aksocket;
	//KDE stuff
	gboolean kdesupported;
	GSList *kdecontacts;
	//Counter for internal contacts identification
	guint counter;
	/*Callback*/
	mmgui_event_ext_callback callback;
	/*Library cache*/
	mmgui_libpaths_cache_t libcache;
	/*User data pointer*/
	gpointer userdata;
	/*Work thread*/
	GThread *workthread;
};

typedef struct _mmgui_addressbooks *mmgui_addressbooks_t;

mmgui_addressbooks_t mmgui_addressbooks_new(mmgui_event_ext_callback callback, mmgui_libpaths_cache_t libcache, gpointer userdata);
gboolean mmgui_addressbooks_get_gnome_contacts_available(mmgui_addressbooks_t addressbooks);
gboolean mmgui_addressbooks_get_kde_contacts_available(mmgui_addressbooks_t addressbooks);
GSList *mmgui_addressbooks_get_gnome_contacts_list(mmgui_addressbooks_t addressbooks);
GSList *mmgui_addressbooks_get_kde_contacts_list(mmgui_addressbooks_t addressbooks);
mmgui_contact_t mmgui_addressbooks_get_gnome_contact(mmgui_addressbooks_t addressbooks, guint index);
mmgui_contact_t mmgui_addressbooks_get_kde_contact(mmgui_addressbooks_t addressbooks, guint index);
void mmgui_addressbooks_close(mmgui_addressbooks_t addressbooks);

#endif /* __ADDRESSBOOKS_H__ */
