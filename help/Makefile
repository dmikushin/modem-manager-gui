include ../Makefile_h

FILES_PO:=$(wildcard *.po)
HELPDIR = $(PREFIX)/share/help
APPDIR = modem-manager-gui

all:
	for f in $(FILES_PO); do \
		mkdir `basename $$f .po`;\
		msgfmt $$f -f -v -o `basename $$f .po`.mo;\
		itstool -m `basename $$f .po`.mo -o `basename $$f .po`/ C/*.page; \
	done

install:
	install -d $(INSTALLPREFIX)$(DESTDIR)$(HELPDIR)/C/$(APPDIR);
	cp -r C/* $(INSTALLPREFIX)$(DESTDIR)$(HELPDIR)/C/$(APPDIR);
	for f in $(FILES_PO); do \
		install -d $(INSTALLPREFIX)$(DESTDIR)$(HELPDIR)/`basename $$f .po`/$(APPDIR)/figures; \
		cp C/figures/* $(INSTALLPREFIX)$(DESTDIR)$(HELPDIR)/`basename $$f .po`/$(APPDIR)/figures/; \
		cp `basename $$f .po`/*.page $(INSTALLPREFIX)$(DESTDIR)$(HELPDIR)/`basename $$f .po`/$(APPDIR)/; \
	done

uninstall:
	rm -rf $(INSTALLPREFIX)$(DESTDIR)$(HELPDIR)/C/$(APPDIR);
	for f in $(FILES_PO); do \
		rm -rf $(INSTALLPREFIX)$(DESTDIR)$(HELPDIR)/`basename $$f .po`/$(APPDIR); \
	done

messages:
	itstool -o modem-manager-gui-help.pot C/*.page;
	for f in $(FILES_PO); do \
		msgmerge -U $$f modem-manager-gui-help.pot; done

clean:
	for f in $(FILES_PO); do \
		rm -rf `basename $$f .po`; \
		rm -f `basename $$f .po`.mo; \
	done

