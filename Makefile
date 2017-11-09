include Makefile_h

DESTDIR=$(PREFIX)/bin

all: modem-manager-gui

modem-manager-gui:
	(cd src && ${MAKE} all)
	(cd src/modules && ${MAKE} all)
	(cd src/plugins && ${MAKE} all)
	(cd po && ${MAKE} all)
	(cd man && ${MAKE} all)
	(cd help && ${MAKE} all)
	(cd appdata && ${MAKE} all)
	(cd polkit && ${MAKE} all)

install:
	(cd src && ${MAKE} install)
	(cd src/modules && ${MAKE} install)
	(cd src/plugins && ${MAKE} install)
	(cd resources && ${MAKE} install)
	(cd po && ${MAKE} install)
	(cd man && ${MAKE} install)
	(cd help && ${MAKE} install)
	(cd appdata && ${MAKE} install)
	(cd polkit && ${MAKE} install)

uninstall:
	(cd src && ${MAKE} uninstall)
	(cd src/modules && ${MAKE} uninstall)
	(cd src/plugins && ${MAKE} uninstall)
	(cd resources && ${MAKE} uninstall)
	(cd po && ${MAKE} uninstall)
	(cd man && ${MAKE} uninstall)
	(cd help && ${MAKE} uninstall)
	(cd appdata && ${MAKE} uninstall)
	(cd polkit && ${MAKE} uninstall)

clean:
	(cd src && ${MAKE} clean)
	(cd src/modules && ${MAKE} clean)
	(cd src/plugins && ${MAKE} clean)
	(cd po && ${MAKE} clean)
	(cd man && ${MAKE} clean)
	(cd help && ${MAKE} clean)
	(cd appdata && ${MAKE} clean)
	(cd polkit && ${MAKE} clean)

messages:
	(cd appdata && ${MAKE} messages)
	(cd help && ${MAKE} messages)
	(cd man && ${MAKE} messages)
	(cd po && ${MAKE} messages)
	(cd polkit && ${MAKE} messages)

upload-translations-sources:
	tx push -s

download-translations:
	tx pull -a

source-archive:
	(cd src && ${MAKE} clean)
	(cd src/modules && ${MAKE} clean)
	(cd src/plugins && ${MAKE} clean)
	(cd resources && ${MAKE} clean)
	(cd po && ${MAKE} clean)
	(cd man && ${MAKE} clean)
	(cd help && ${MAKE} clean)
	(cd appdata && ${MAKE} clean)
	(cd polkit && ${MAKE} clean)
	chmod +x configure
	tar --exclude='./Makefile_h' --exclude='./src/resources.h' --transform 's,^\.,$(notdir $(CURDIR)),' -zcvf ../$(notdir $(CURDIR)).tar.gz .
