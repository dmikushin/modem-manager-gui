c_sources = [
	'settings.c',
	'strformat.c',
	'libpaths.c',
	'dbus-utils.c',
	'notifications.c',
	'addressbooks.c',
	'ayatana.c',
	'smsdb.c',
	'trafficdb.c',
	'providersdb.c',
	'modem-settings.c',
	'ussdlist.c',
	'encoding.c',
	'vcard.c',
	'netlink.c',
	'polkit.c',
	'svcmanager.c',
	'mmguicore.c',
	'contacts-page.c',
	'traffic-page.c',
	'scan-page.c',
	'info-page.c',
	'ussd-page.c',
	'sms-page.c',
	'devices-page.c',
	'preferences-window.c',
	'welcome-window.c',
	'connection-editor-window.c',
	'main.c'
]

c_args = [
    	
]

link_args = [
	'-export-dynamic'
]

mmgui = executable('modem-manager-gui',
	c_sources,
	c_args: c_args,
	link_args: link_args,
	install: true,
	dependencies : [glib, gobject, gio, gmodule, gtk, gdbm, gtkspell, appindicator, m])
