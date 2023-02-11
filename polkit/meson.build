i18n = import('i18n')

i18n.merge_file(
	input: 'ru.linuxonly.modem-manager-gui.policy.in',
	output: 'ru.linuxonly.modem-manager-gui.policy',
	type: 'xml',
	data_dirs: join_paths(meson.source_root(), 'polkit'),
	po_dir: join_paths(meson.source_root(), 'polkit'),
	install: true,
	install_dir: join_paths(get_option('datadir'), 'polkit-1', 'actions')
)

