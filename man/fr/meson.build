custom_target('man-fr',
		input: 'fr.po',
		output: 'modem-manager-gui.1.gz',
		install: true,
		install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'fr', 'man1'),
		command: [helper, '@INPUT@', '@OUTPUT@' ]
		)
