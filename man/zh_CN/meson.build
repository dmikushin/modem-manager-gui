custom_target('man-zh_CN',
		input: 'zh_CN.po',
		output: 'modem-manager-gui.1.gz',
		install: true,
		install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'zh_CN', 'man1'),
		command: [helper, '@INPUT@', '@OUTPUT@' ]
		)
