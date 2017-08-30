/*
 *      main.h
 *      
 *      Copyright 2012-2017 Alex <alex@linuxonly.ru>
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

#ifndef __MAIN_H__
#define __MAIN_H__

#include <gtk/gtk.h>

#include "resources.h"
#include "settings.h"
#include "modem-settings.h"
#include "notifications.h"
#include "ayatana.h"
#include "addressbooks.h"

#if RESOURCE_SPELLCHECKER_ENABLED
	#include <gtkspell/gtkspell.h>
#endif

#if RESOURCE_INDICATOR_ENABLED
	#include <libappindicator/app-indicator.h>
#endif

#define MMGUI_MAIN_DEFAULT_DEVICE_IDENTIFIER  "00000000000000000000000"

#define MMGUI_MAIN_OPERATION_TIMEOUT          120


enum _mmgui_main_pages {
	MMGUI_MAIN_PAGE_DEVICES = 0,
	MMGUI_MAIN_PAGE_SMS,
	MMGUI_MAIN_PAGE_USSD,
	MMGUI_MAIN_PAGE_INFO,
	MMGUI_MAIN_PAGE_SCAN,
	MMGUI_MAIN_PAGE_TRAFFIC,
	MMGUI_MAIN_PAGE_CONTACTS,
	MMGUI_MAIN_PAGE_NUMBER
};

/*Infobar types*/
enum _mmgui_main_infobar_type {
	MMGUI_MAIN_INFOBAR_TYPE_INFO = 0,
	MMGUI_MAIN_INFOBAR_TYPE_WARNING,
	MMGUI_MAIN_INFOBAR_TYPE_ERROR,
	MMGUI_MAIN_INFOBAR_TYPE_PROGRESS
};

/*Infobar results*/
enum _mmgui_main_infobar_result {
	MMGUI_MAIN_INFOBAR_RESULT_INTERRUPT = 0,
	MMGUI_MAIN_INFOBAR_RESULT_SUCCESS,
	MMGUI_MAIN_INFOBAR_RESULT_FAIL,
	MMGUI_MAIN_INFOBAR_RESULT_TIMEOUT
};

struct _mmgui_main_window {
	//Window
	GtkWidget *window;
	GtkWidget *windowbox;
	GtkWidget *toolbar;
	GtkWidget *statusbar;
	GtkWidget *infobar;
	GtkWidget *infobarspinner;
	GtkWidget *infobarimage;
	GtkWidget *infobarlabel;
	GtkWidget *infobarstopbutton;
	guint infobartimeout;
	guint sbcontext;
	GtkWidget *signalimage;
	GdkPixbuf *signal0icon;
	GdkPixbuf *signal25icon;
	GdkPixbuf *signal50icon;
	GdkPixbuf *signal75icon;
	GdkPixbuf *signal100icon;
	GdkPixbuf *mainicon;
	//Dialogs
	GtkWidget *aboutdialog;
	GtkWidget *prefdialog;
	GtkWidget *questiondialog;
	GtkWidget *errordialog;
	GtkWidget *exitdialog;
	/*Welcome window*/
	GtkWidget *welcomewindow;
	GtkWidget *welcomeimage;
	GtkWidget *welcomenotebook;
	GtkWidget *welcomemmcombo;
	GtkWidget *welcomecmcombo;
	GtkWidget *welcomeenablecb;
	GtkWidget *welcomebutton;
	GtkWidget *welcomeacttreeview;
	/*New SMS dialog*/
	GtkWidget *newsmsdialog;
	GtkWidget *smsnumberentry;
	GtkWidget *smsnumbercombo;
	GtkWidget *smstextview;
	GtkWidget *newsmssendtb;
	GtkWidget *newsmssavetb;
	GtkWidget *newsmsspellchecktb;
	GtkTreeStore *smsnumlistmodel;
	GtkTreePath *smsnumlistmodempath;
	GtkTreePath *smsnumlistgnomepath;
	GtkTreePath *smsnumlistkdepath;
	GSList *smsnumlisthistory;
	GtkEntryCompletion *smscompletion;
    GtkListStore *smscompletionmodel;
    #if RESOURCE_SPELLCHECKER_ENABLED
        GtkSpellChecker *newsmsspellchecker;
        GtkWidget *newsmslangmenu;
        GSList *newsmscodes;
    #endif
	//Toolbar
	GtkWidget *devbutton;
	GtkWidget *smsbutton;
	GtkWidget *ussdbutton;
	GtkWidget *infobutton;
	GtkWidget *scanbutton;
	GtkWidget *contactsbutton;
	GtkWidget *trafficbutton;
	//Pages
	GtkWidget *notebook;
	//Devices page
	GtkWidget *devlist;
	GtkWidget *devconnctl;
	GtkWidget *devconneditor;
	GtkWidget *devconncb;
	/*Connections dialog*/
	GtkWidget *conneditdialog;
	GtkWidget *connaddtoolbutton;
	GtkWidget *connremovetoolbutton;
	GtkWidget *connsavetoolbutton;
	GtkWidget *contreeview;
	GtkWidget *connnamecombobox;
	GtkWidget *connnamecomboboxentry;
	GtkWidget *connnameapnentry;
	GtkWidget *connnethomeradiobutton;
	GtkWidget *connnetroamradiobutton;
	GtkWidget *connnetidentry;
	GtkWidget *connauthnumberentry;
	GtkWidget *connauthusernameentry;
	GtkWidget *connauthpassentry;
	GtkWidget *conndnsdynradiobutton;
	GtkWidget *conndnsstradiobutton;
	GtkWidget *conndns1entry;
	GtkWidget *conndns2entry;
	/*Add connection dialog*/
	GtkWidget *connadddialog;
	//SMS page
	GtkWidget *smsinfobar;
	GtkWidget *smsinfobarlabel;
	GtkWidget *smslist;
	GtkWidget *smstext;
	GtkWidget *newsmsbutton;
	GtkWidget *removesmsbutton;
	GtkWidget *answersmsbutton;
	GdkPixbuf *smsreadicon;
	GdkPixbuf *smsunreadicon;
	GtkTreePath *incomingpath;
	GtkTreePath *sentpath;
	GtkTreePath *draftspath;
	GtkTextTag *smsheadingtag;
	GtkTextTag *smsdatetag;
	//Info page
	GtkWidget *devicevlabel;
	GtkWidget *operatorvlabel;
	GtkWidget *operatorcodevlabel;
	GtkWidget *regstatevlabel;
	GtkWidget *modevlabel;
	GtkWidget *imeivlabel;
	GtkWidget *imsivlabel;
	GtkWidget *signallevelprogressbar;
	GtkWidget *info3gpplocvlabel;
	GtkWidget *infogpslocvlabel;
	GtkWidget *equipmentimage;
	GtkWidget *networkimage;
	GtkWidget *locationimage;
	//USSD page
	GtkWidget *ussdinfobar;
	GtkWidget *ussdinfobarlabel;
	GtkWidget *ussdentry;
	GtkWidget *ussdcombobox;
	GtkWidget *ussdeditor;
	GtkWidget *ussdsend;
	GtkWidget *ussdtext;
	GtkTextTag *ussdrequesttag;
	GtkTextTag *ussdhinttag;
	GtkTextTag *ussdanswertag;
	GtkEntryCompletion *ussdcompletion;
    GtkListStore *ussdcompletionmodel;
	//Scan page
	GtkWidget *scaninfobar;
	GtkWidget *scaninfobarlabel;
	GtkWidget *scanlist;
	GtkWidget *startscanbutton;
	GtkWidget *scancreateconnectionbutton;
	//Traffic page
	GtkWidget *trafficparamslist;
	GtkWidget *trafficdrawingarea;
	GtkWidget *trafficlimitbutton;
	GtkWidget *trafficconnbutton;
	//Contacts page
	GtkWidget *contactsinfobar;
	GtkWidget *contactsinfobarlabel;
	GtkWidget *newcontactbutton;
	GtkWidget *removecontactbutton;
	GtkWidget *smstocontactbutton;
	GtkWidget *contactstreeview;
	GtkWidget *contactssmsmenu;
	GtkTreePath *contmodempath;
	GtkTreePath *contgnomepath;
	GtkTreePath *contkdepath;
	//New contact dialog
	GtkWidget *newcontactdialog;
	GtkWidget *contactnameentry;
	GtkWidget *contactnumberentry;
	GtkWidget *contactemailentry;
	GtkWidget *contactgroupentry;
	GtkWidget *contactname2entry;
	GtkWidget *contactnumber2entry;
	GtkWidget *newcontactaddbutton;
	//Limits dialog
	GtkWidget *trafficlimitsdialog;
	GtkWidget *trafficlimitcheckbutton;
	GtkWidget *trafficamount;
	GtkWidget *trafficunits;
	GtkWidget *trafficmessage;
	GtkWidget *trafficaction;
	GtkWidget *timelimitcheckbutton;
	GtkWidget *timeamount;
	GtkWidget *timeunits;
	GtkWidget *timemessage;
	GtkWidget *timeaction;
	//Connections dialog
	GtkAccelGroup *connaccelgroup;
	GtkWidget *conndialog;
	GtkWidget *connscrolledwindow;
	GtkWidget *conntreeview;
	GtkWidget *conntermtoolbutton;
	//Traffic statistics dialog
	GtkWidget *trafficstatsdialog;
	GtkWidget *trafficstatstreeview;
	GtkWidget *trafficstatsmonthcb;
	GtkWidget *trafficstatsyearcb;
	//USSD editor dialog
	GtkAccelGroup *ussdaccelgroup;
	GtkWidget *ussdeditdialog;
	GtkWidget *ussdedittreeview;
	GtkWidget *newussdtoolbutton;
	GtkWidget *removeussdtoolbutton;
	GtkWidget *ussdencodingtoolbutton;
	//Preferences
	GtkWidget *prefsmsconcat;
	GtkWidget *prefsmsexpand;
	GtkWidget *prefsmsoldontop;
	GtkWidget *prefsmsvalidityscale;
	GtkWidget *prefsmsreportcb;
	GtkWidget *preftrafficrxcolor;
	GtkWidget *preftraffictxcolor;
	GtkWidget *preftrafficmovdircombo;
	GtkWidget *prefbehavioursounds;
	GtkWidget *prefbehaviourhide;
	GtkWidget *prefbehaviourgeom;
	GtkWidget *prefbehaviourautostart;
	GtkWidget *prefenabletimeoutscale;
	GtkWidget *prefsendsmstimeoutscale;
	GtkWidget *prefsendussdtimeoutscale;
	GtkWidget *prefscannetworkstimeoutscale;
	GtkWidget *prefmodulesmmcombo;
	GtkWidget *prefmodulescmcombo;
	GtkWidget *prefactivepagessmscb;
	GtkWidget *prefactivepagesussdcb;
	GtkWidget *prefactivepagesinfocb;
	GtkWidget *prefactivepagesscancb;
	GtkWidget *prefactivepagestrafficcb;
	GtkWidget *prefactivepagescontactscb;
	//Exit dialog
	GtkWidget *exitaskagain;
	GtkWidget *exitcloseradio;
	GtkWidget *exithideradio;
	/*Keyboard acceletators*/
	GtkAccelGroup *accelgroup;
	/*Pages menu section*/
	GMenu *appsection;
	/*Page shortcuts*/
	GSList *pageshortcuts;
	/*Closures for SMS page*/
	GClosure *newsmsclosure;
	GClosure *removesmsclosure;
	GClosure *answersmsclosure;
	/*Closures for USSD page*/
	GClosure *ussdeditorclosure;
	GClosure *ussdsendclosure;
	/*Closures for Scan page*/
	GClosure *startscanclosure;
	/*Closures for Traffic page*/
	GClosure *trafficlimitclosure;
	GClosure *trafficconnclosure;
	GClosure *trafficstatsclosure;
	/*Closures for Contacts page*/
	GClosure *newcontactclosure;
	GClosure *removecontactclosure;
	GClosure *smstocontactclosure;
	/*Indicator*/
	AppIndicator *indicator;
	GtkWidget *indmenu;
	GtkWidget *showwin_ind, *sep1_ind, *newsms_ind, *sep2_ind, *quit_ind;
	gulong traysigid;
};

typedef struct _mmgui_main_window *mmgui_main_window_t;

struct _mmgui_cli_options {
	gboolean invisible;
	gboolean minimized;
	gboolean hidenotifyshown;
	/*SMS*/
	gboolean concatsms;
	gboolean smsexpandfolders;
	gboolean smsoldontop;
	gboolean smsdeliveryreport;
	gint smsvalidityperiod;
	/*Traffic graph*/
	#if GTK_CHECK_VERSION(3,4,0)
		GdkRGBA rxtrafficcolor;
		GdkRGBA txtrafficcolor;
	#else
		GdkColor rxtrafficcolor;
		GdkColor txtrafficcolor;
	#endif
	gboolean graphrighttoleft;
	/*Behaviour*/
	gboolean usesounds;
	gboolean hidetotray;
	gboolean askforhide;
	gboolean savegeometry;
	/*Window geometry*/
	gint wgwidth;
	gint wgheight;
	gint wgposx;
	gint wgposy;
	/*Active pages*/
	gboolean smspageenabled;
	gboolean ussdpageenabled;
	gboolean infopageenabled;
	gboolean scanpageenabled;
	gboolean trafficpageenabled;
	gboolean contactspageenabled;
};

typedef struct _mmgui_cli_options *mmgui_cli_options_t;

struct _mmgui_application {
	//GTK+ application object
	GtkApplication *gtkapplication;
	//Allocated structures
	mmgui_main_window_t window;
	mmgui_cli_options_t options;
	//mmgui_traffic_limits_t limits;
	mmgui_core_options_t coreoptions;
	//Objects
	mmguicore_t core;
	settings_t settings;
	modem_settings_t modemsettings;
	mmgui_libpaths_cache_t libcache;
	mmgui_notifications_t notifications;
	mmgui_addressbooks_t addressbooks;
	mmgui_ayatana_t ayatana;
};

typedef struct _mmgui_application *mmgui_application_t;

struct _mmgui_application_data {
	mmgui_application_t mmguiapp;
	gpointer data;
};

typedef struct _mmgui_application_data *mmgui_application_data_t;


gboolean mmgui_main_ui_question_dialog_open(mmgui_application_t mmguiapp, gchar *caption, gchar *text);
gboolean mmgui_main_ui_error_dialog_open(mmgui_application_t mmguiapp, gchar *caption, gchar *text);

void mmgui_ui_infobar_show_result(mmgui_application_t mmguiapp, gint result, gchar *message);
void mmgui_ui_infobar_show(mmgui_application_t mmguiapp, gchar *message, gint type);

gboolean mmgui_main_ui_test_device_state(mmgui_application_t mmguiapp, guint setpage);
void mmgui_main_ui_devices_button_toggled_signal(GObject *object, gpointer data);
void mmgui_main_ui_sms_button_toggled_signal(GObject *object, gpointer data);
void mmgui_main_ui_ussd_button_toggled_signal(GObject *object, gpointer data);
void mmgui_main_ui_info_button_toggled_signal(GObject *object, gpointer data);
void mmgui_main_ui_scan_button_toggled_signal(GObject *object, gpointer data);
void mmgui_main_ui_traffic_button_toggled_signal(GObject *object, gpointer data);
void mmgui_main_ui_contacts_button_toggled_signal(GObject *object, gpointer data);
void mmgui_main_window_update_active_pages(mmgui_application_t mmguiapp);
gboolean mmgui_main_ui_window_delete_event_signal(GtkWidget *widget, GdkEvent  *event, gpointer data);
void mmgui_main_ui_window_destroy_signal(GObject *object, gpointer data);
gchar *mmgui_main_ui_message_validity_scale_value_format(GtkScale *scale, gdouble value, gpointer user_data);
gchar *mmgui_main_ui_timeout_scale_value_format(GtkScale *scale, gdouble value, gpointer user_data);
void mmgui_main_ui_control_buttons_disable(mmgui_application_t mmguiapp, gboolean disable);
void mmgui_main_ui_interrupt_operation_button_clicked_signal(GObject *object, gpointer data);
gboolean mmgui_main_ui_update_statusbar_from_thread(gpointer data);

#endif /* __MAIN_H__ */
