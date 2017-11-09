custom_target('man-pt_BR',
		input: 'pt_BR.po',
		output: 'modem-manager-gui.1.gz',
		install: true,
		install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'pt_BR', 'man1'),
		command: [helper, '@INPUT@', '@OUTPUT@' ]
		)
