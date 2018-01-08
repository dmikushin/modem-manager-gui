datadir = join_paths([ get_option('prefix'), get_option('datadir'), 'modem-manager-gui' ])
pngiconsdir = join_paths([ get_option('prefix'), get_option('datadir'), 'icons', 'hicolor', '128x128', 'apps' ])
svgiconsdir = join_paths([ get_option('prefix'), get_option('datadir'), 'icons', 'hicolor', 'scalable', 'apps' ])
symiconsdir = join_paths([ get_option('prefix'), get_option('datadir'), 'icons', 'hicolor', 'symbolic', 'apps' ])

install_subdir('pixmaps', install_dir: datadir)
install_subdir('sounds', install_dir: datadir)
install_subdir('ui', install_dir: datadir)

install_data('modem-manager-gui.png', install_dir: pngiconsdir)
install_data('modem-manager-gui.svg', install_dir: svgiconsdir)
install_data('modem-manager-gui-symbolic.svg', install_dir: symiconsdir)
