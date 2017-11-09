custom_target('man-tr',
		input: 'tr.po',
		output: 'modem-manager-gui.1.gz',
		install: true,
		install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'tr', 'man1'),
		command: [helper, '@INPUT@', '@OUTPUT@' ]
		)
