custom_target('man-pl',
		input: 'pl.po',
		output: 'modem-manager-gui.1.gz',
		install: true,
		install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'pl', 'man1'),
		command: [helper, '@INPUT@', '@OUTPUT@' ]
		)
