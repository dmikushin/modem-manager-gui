include ../Makefile_h

APPDATADIR = $(PREFIX)/share/metainfo
APPSDIR    = $(PREFIX)/share/applications

all:
	while read lang; do \
		msgfmt $$lang.po -f -v -o $$lang.mo; \
	done < LINGUAS
	itstool -i its/appdata.its -j modem-manager-gui.appdata.xml.in -o modem-manager-gui.appdata.xml *.mo
	msgfmt --desktop --template modem-manager-gui.desktop.in -d . -o modem-manager-gui.desktop

install: 
	install -d $(INSTALLPREFIX)$(DESTDIR)$(APPDATADIR)
	cp modem-manager-gui.appdata.xml $(INSTALLPREFIX)$(DESTDIR)$(APPDATADIR)
	install -d $(INSTALLPREFIX)$(DESTDIR)$(APPSDIR)
	cp modem-manager-gui.desktop $(INSTALLPREFIX)$(DESTDIR)$(APPSDIR)

uninstall:
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(APPDATADIR)/modem-manager-gui.appdata.xml
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(APPSDIR)/modem-manager-gui.desktop

messages:
	itstool -i its/appdata.its -o modem-manager-gui.appdata.pot modem-manager-gui.appdata.xml.in
	xgettext -j -o modem-manager-gui.appdata.pot modem-manager-gui.desktop.in
	while read lang; do \
		msgmerge -U $$lang.po modem-manager-gui.appdata.pot; \
	done < LINGUAS

clean:
	rm -f *.mo
	rm -f modem-manager-gui.appdata.xml
	rm -f modem-manager-gui.desktop
