include ../Makefile_h

PACKAGE = modem-manager-gui
MSGFMT = msgfmt
FILES_PO:=$(wildcard *.po)
FILES_MO:=$(wildcard *.mo)
LOCALEDIR = $(PREFIX)/share/locale
MSGFMT_OPT = -f -v -o

all: 
	for f in $(FILES_PO); do $(MSGFMT) $$f $(MSGFMT_OPT) `basename $$f .po`.mo; done

install: 
	for f in $(FILES_MO); do install -d $(INSTALLPREFIX)$(DESTDIR)$(LOCALEDIR)/`basename $$f .mo`/LC_MESSAGES; \
		cp $$f $(INSTALLPREFIX)$(DESTDIR)$(LOCALEDIR)/`basename $$f .mo`/LC_MESSAGES/$(PACKAGE).mo; done

uninstall:
	for f in $(FILES_MO); do \
		rm -f $(INSTALLPREFIX)$(DESTDIR)$(LOCALEDIR)/`basename $$f .mo`/LC_MESSAGES/$(PACKAGE).mo; done

messages:
	xgettext -k_ -kN_ ../src/*.c -o modem-manager-gui.pot
	xgettext -j -L Glade ../resources/*.ui -o modem-manager-gui.pot

clean:
	for f in $(FILES_MO); do \
		rm -f $$f; done

