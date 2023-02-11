i18n = import('i18n')

i18n.merge_file(
	input: 'modem-manager-gui.appdata.xml.in',
	output: 'modem-manager-gui.appdata.xml',
	type: 'xml',
	data_dirs: join_paths(meson.source_root(), 'appdata'),
	po_dir: join_paths(meson.source_root(), 'appdata'),
	install: true,
	install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'metainfo')
)

i18n.merge_file(
	input: 'modem-manager-gui.desktop.in',
	output: 'modem-manager-gui.desktop',
	type: 'desktop',
	po_dir: join_paths(meson.source_root(), 'appdata'),
	install: true,
	install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'applications')
)
