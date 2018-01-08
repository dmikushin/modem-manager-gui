include ../Makefile_h

RESDIR      = $(PREFIX)/share/modem-manager-gui
ICONSPNGDIR = $(PREFIX)/share/icons/hicolor/128x128/apps
ICONSSVGDIR = $(PREFIX)/share/icons/hicolor/scalable/apps
ICONSSYMDIR = $(PREFIX)/share/icons/hicolor/symbolic/apps

install:
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(RESDIR)
	cp -r pixmaps $(INSTALLPREFIX)$(DESTDIR)$(RESDIR)
	cp -r sounds $(INSTALLPREFIX)$(DESTDIR)$(RESDIR)
	cp -r ui $(INSTALLPREFIX)$(DESTDIR)$(RESDIR)
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(ICONSPNGDIR)
	cp modem-manager-gui.png $(INSTALLPREFIX)$(DESTDIR)$(ICONSPNGDIR)
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(ICONSSVGDIR)
	cp modem-manager-gui.svg $(INSTALLPREFIX)$(DESTDIR)$(ICONSSVGDIR)
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(ICONSSYMDIR)
	cp modem-manager-gui-symbolic.svg $(INSTALLPREFIX)$(DESTDIR)$(ICONSSYMDIR)
	
uninstall:
	rm -rf $(INSTALLPREFIX)$(DESTDIR)$(RESDIR)
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(ICONSPNGDIR)/modem-manager-gui.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(ICONSSVGDIR)/modem-manager-gui.svg
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(ICONSSYMDIR)/modem-manager-gui-symbolic.svg


