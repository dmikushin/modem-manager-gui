custom_target('man-uk',
		input: 'uk.po',
		output: 'modem-manager-gui.1.gz',
		install: true,
		install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'uk', 'man1'),
		command: [helper, '@INPUT@', '@OUTPUT@' ]
		)
