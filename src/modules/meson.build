mm06_c_sources = [
	'../smsdb.c',
	'../encoding.c',
	'../dbus-utils.c',
	'mm06.c'
]

mm06_c_args = [
    	
]

mm06 = shared_library('modmm_mm06',
	mm06_c_sources,
	c_args: mm06_c_args,
	install: true,
	install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'modem-manager-gui', 'modules'),
	dependencies : [glib, gobject, gio, gmodule, gdbm])

mm07_c_sources = [
	'../smsdb.c',
	'../encoding.c',
	'../dbus-utils.c',
	'mm07.c'
]

mm07_c_args = [
    	
]

mm07 = shared_library('modmm_mm07',
	mm07_c_sources,
	c_args: mm07_c_args,
	install: true,
	install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'modem-manager-gui', 'modules'),
	dependencies : [glib, gobject, gio, gmodule, gdbm])

nm09_c_sources = [
	'uuid.c',
	'nm09.c'
]

nm09_c_args = [
    	
]

nm09 = shared_library('modcm_nm09',
	nm09_c_sources,
	c_args: nm09_c_args,
	install: true,
	install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'modem-manager-gui', 'modules'),
	dependencies : [glib, gobject, gio, gmodule])

pppd245_c_sources = [
	'pppd245.c'
]

pppd245_c_args = [
    	
]

pppd245 = shared_library('modcm_pppd245',
	pppd245_c_sources,
	c_args: pppd245_c_args,
	install: true,
	install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'modem-manager-gui', 'modules'),
	dependencies : [glib, gobject, gio, gmodule])

ofono109_c_sources = [
	'../smsdb.c',
	'../encoding.c',
	'../vcard.c',
	'historyshm.c',
	'ofono109.c'
]

ofono109_c_args = [
    	
]

ofono109 = shared_library('modmm_ofono109',
	ofono109_c_sources,
	c_args: ofono109_c_args,
	install: true,
	install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'modem-manager-gui', 'modules'),
	dependencies : [glib, gobject, gio, gmodule, gdbm, rt])

connman112_c_sources = [
	'uuid.c',
	'connman112.c'
]

connman112_c_args = [
    	
]

connman112 = shared_library('modcm_connman112',
	connman112_c_sources,
	c_args: connman112_c_args,
	install: true,
	install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'modem-manager-gui', 'modules'),
	dependencies : [glib, gobject, gio, gmodule])
