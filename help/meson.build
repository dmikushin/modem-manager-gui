gnome = import('gnome')

help_files = [
	'about.page',
	'contrib-code.page',
	'contrib-translations.page',
	'index.page',
	'license.page',
	'report-bugs.page',
	'usage-config.page',
	'usage-contacts.page',
	'usage-getinfo.page',
	'usage-modem.page',
	'usage-netsearch.page',
	'usage-sms.page',
	'usage-traffic.page',
	'usage-ussd.page'
]

help_media = [
	'figures/contacts-window.png',
	'figures/modem-manager-gui-logo.png',
	'figures/network-info.png',
	'figures/scan-window.png',
	'figures/sms-window.png',
	'figures/traffic-window.png',
	'figures/ussd-window.png',
	'figures/startup-window.png'
]

gnome.yelp(meson.project_name(),
		sources: help_files,
		media: help_media,
		symlink_media: true,
)
