project('Modem Manager GUI', 'c',
        version: '0.0.19.1',
        license : 'GPL3+',
        meson_version: '>=0.38')

glib = dependency('glib-2.0', version: '>=2.32.1')
gobject = dependency('gobject-2.0', version: '>=2.32.1')
gio = dependency('gio-2.0', version: '>=2.32.1')
gmodule = dependency('gmodule-2.0', version: '>=2.32.1')
gtk = dependency('gtk+-3.0', version: '>=3.4.0')
ofono = dependency('ofono', version: '>=1.9', required: false)
gtkspell = dependency('gtkspell3-3.0', version: '>=3.0.3', required: false)
appindicator = dependency('ayatana-appindicator3-0.1', version: '>=0.4.92', required: false)
gdbm = meson.get_compiler('c').find_library('gdbm')
rt = meson.get_compiler('c').find_library('rt')
m = meson.get_compiler('c').find_library('m')

po4a = find_program('po4a')

resources_h = configuration_data()
resources_h.set('RESOURCE_NAME', '"@0@"'.format(meson.project_name()))
resources_h.set('RESOURCE_VERSION', '"@0@"'.format(meson.project_version()))
resources_h.set('RESOURCE_SCALABLE_ICONS_DIR', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'icons', 'hicolor', 'scalable', 'apps' ])))
resources_h.set('RESOURCE_SYMBOLIC_ICONS_DIR', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'icons', 'hicolor', 'symbolic', 'apps' ])))
resources_h.set('RESOURCE_PNG_ICONS_DIR', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'icons', 'hicolor', '128x128', 'apps' ])))
resources_h.set('RESOURCE_PIXMAPS_DIR', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'pixmaps' ])))
resources_h.set('RESOURCE_SOUNDS_DIR', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'sounds' ])))
resources_h.set('RESOURCE_UI_DIR', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui', 'ui' ])))
resources_h.set('RESOURCE_MODULES_DIR', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('libdir'), 'modem-manager-gui', 'modules' ])))
resources_h.set('RESOURCE_LOCALE_DIR', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'locale' ])))
resources_h.set('RESOURCE_LOCALE_DOMAIN', '"modem-manager-gui"')
resources_h.set('RESOURCE_DESKTOP_FILE', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'applications', 'modem-manager-gui.desktop' ])))
resources_h.set('RESOURCE_PROVIDERS_DB', '"@0@"'.format(join_paths([ get_option('prefix'), get_option('datadir'), 'mobile-broadband-provider-info', 'serviceproviders.xml' ])))

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
