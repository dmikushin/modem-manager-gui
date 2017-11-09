custom_target('man-de',
		input: 'de.po',
		output: 'modem-manager-gui.1.gz',
		install: true,
		install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'de', 'man1'),
		command: [helper, '@INPUT@', '@OUTPUT@' ]
		)
