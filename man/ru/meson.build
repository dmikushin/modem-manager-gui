custom_target('man-ru',
		input: 'ru.po',
		output: 'modem-manager-gui.1.gz',
		install: true,
		install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'ru', 'man1'),
		command: [helper, '@INPUT@', '@OUTPUT@' ]
		)
