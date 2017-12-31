project('Modem Manager GUI', 'c',
        version: '0.0.19',
        license : 'GPL3+',
        meson_version: '>=0.37')

glib = dependency('glib-2.0', version: '>=2.32.1')
gobject = dependency('gobject-2.0', version: '>=2.32.1')
gio = dependency('gio-2.0', version: '>=2.32.1')
gmodule = dependency('gmodule-2.0', version: '>=2.32.1')
gtk = dependency('gtk+-3.0', version: '>=3.4.0')
ofono = dependency('ofono', version: '>=1.9', required: false)
gtkspell = dependency('gtkspell3-3.0', version: '>=3.0.3', required: false)
appindicator = dependency('appindicator3-0.1', version: '>=0.4.92', required: false)
gdbm = meson.get_compiler('c').find_library('gdbm')
rt = meson.get_compiler('c').find_library('rt')

po4a = find_program('po4a')

resources_h = configuration_data()
resources_h.set('RESOURCE_NAME', '"@0@"'.format(meson.project_name()))
resources_h.set('RESOURCE_VERSION', '"@0@"'.format(meson.project_version()))
resources_h.set('RESOURCE_MAINWINDOW_UI', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'ui', 'modem-manager-gui.ui' ])))
resources_h.set('RESOURCE_MAINWINDOW_ICON', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'modem-manager-gui.png' ])))
resources_h.set('RESOURCE_TOOLBAR_DEV', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'dev-tb.png' ])))
resources_h.set('RESOURCE_TOOLBAR_SMS', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'sms-tb.png' ])))
resources_h.set('RESOURCE_TOOLBAR_USSD', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'ussd-tb.png' ])))
resources_h.set('RESOURCE_TOOLBAR_INFO', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'info-tb.png' ])))
resources_h.set('RESOURCE_TOOLBAR_SCAN', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'scan-tb.png' ])))
resources_h.set('RESOURCE_TOOLBAR_CONT', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'cont-tb.png' ])))
resources_h.set('RESOURCE_TOOLBAR_TRAFFIC', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'traffic-tb.png' ])))
resources_h.set('RESOURCE_SMS_READ', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'sms-read.png' ])))
resources_h.set('RESOURCE_SMS_UNREAD', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'sms-unread.png' ])))
resources_h.set('RESOURCE_MESSAGE_RECEIVED', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'message-received.png' ])))
resources_h.set('RESOURCE_MESSAGE_SENT', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'message-sent.png' ])))
resources_h.set('RESOURCE_MESSAGE_DRAFTS', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'message-drafts.png' ])))
resources_h.set('RESOURCE_INFO_EQUIPMENT', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'info-equipment.png' ])))
resources_h.set('RESOURCE_INFO_NETWORK', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'info-network.png' ])))
resources_h.set('RESOURCE_INFO_LOCATION', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'info-location.png' ])))
resources_h.set('RESOURCE_SIGNAL_0', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'signal-0.png' ])))
resources_h.set('RESOURCE_SIGNAL_25', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'signal-25.png' ])))
resources_h.set('RESOURCE_SIGNAL_50', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'signal-50.png' ])))
resources_h.set('RESOURCE_SIGNAL_75', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'signal-75.png' ])))
resources_h.set('RESOURCE_SIGNAL_100', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps', 'signal-100.png' ])))
resources_h.set('RESOURCE_SOUND_MESSAGE', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'sounds', 'message.ogg' ])))
resources_h.set('RESOURCE_DESKTOP_FILE', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'applications', 'modem-manager-gui.desktop' ])))
resources_h.set('RESOURCE_PROVIDERS_DB', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'mobile-broadband-provider-info', 'serviceproviders.xml' ])))
resources_h.set('RESOURCE_MODULES_DIR', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('libdir'), 'modem-manager-gui', 'modules' ])))
resources_h.set('RESOURCE_LOCALE_DIR', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'locale' ])))
resources_h.set('RESOURCE_LOCALE_DOMAIN', '"modem-manager-gui"')

if gtkspell.found()
	resources_h.set('RESOURCE_SPELLCHECKER_ENABLED', '1')
else
	resources_h.set('RESOURCE_SPELLCHECKER_ENABLED', '0')
endif
if appindicator.found()
	resources_h.set('RESOURCE_INDICATOR_ENABLED', '1')
else
	resources_h.set('RESOURCE_INDICATOR_ENABLED', '0')
endif
configure_file(input: 'src/resources.h.meson',
		output: 'resources.h',
		configuration: resources_h)

subdir('appdata')
subdir('help')
subdir('man')
subdir('po')
subdir('polkit')
subdir('resources')
subdir('src')
subdir('src/modules')
subdir('src/plugins')
subdir('src/scripts')
