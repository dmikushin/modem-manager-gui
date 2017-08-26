# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=4

DESCRIPTION="GUI for Modem Manager daemon, capable to read and send SMS, send USSD requests, display modem informaion."
HOMEPAGE="http://linuxonly.ru/"
SRC_URI="http://download.tuxfamily.org/gsf/source/modem-manager-gui-0.0.15.1.tar.gz"

LICENSE="GPL"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE=""

DEPEND=">=x11-libs/gtk+-3.4.0
		x11-libs/libnotify"
RDEPEND="${DEPEND}"

