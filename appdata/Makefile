include ../Makefile_h

FILES_PO:=$(wildcard *.po)
FILES_MO:=$(patsubst %.po,%.mo,$(FILES_PO))
APPDATADIR = $(PREFIX)/share/appdata

all: 
	for f in $(FILES_PO); do \
		msgfmt $$f -f -v -o `basename $$f .po`.mo; done
	itstool -i appdata.its -j modem-manager-gui.appdata.xml.in -o modem-manager-gui.appdata.xml $(FILES_MO);

install: 
	install -d $(INSTALLPREFIX)$(DESTDIR)$(APPDATADIR);
	cp modem-manager-gui.appdata.xml $(INSTALLPREFIX)$(DESTDIR)$(APPDATADIR)/modem-manager-gui.appdata.xml

uninstall:
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(APPDATADIR)/modem-manager-gui.appdata.xml

messages:
	itstool -i appdata.its -o modem-manager-gui.appdata.pot modem-manager-gui.appdata.xml.in
	for f in $(FILES_PO); do \
		msgmerge -U $$f modem-manager-gui.appdata.pot; done

clean:
	for f in $(FILES_MO); do \
		rm -f $$f; done
	rm -f modem-manager-gui.appdata.xml

