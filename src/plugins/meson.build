if ofono.found()
	ofonoplugin_c_sources = [
		'ofonohistory.c'
	]
	
	ofonoplugin_c_args = [
		    	
	]
	
	ofonoplugin = shared_module('mmgui-ofono-history',
		ofonoplugin_c_sources,
		c_args: ofonoplugin_c_args,
		install: true,
		install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'ofono', 'plugins'),
		dependencies : [glib, gobject, gio, gmodule, gdbm, rt])
endif
