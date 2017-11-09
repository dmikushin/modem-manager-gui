include ../Makefile_h

RESDIR     = $(PREFIX)/share/modem-manager-gui
PIXDIR     = $(PREFIX)/share/pixmaps

install:
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(RESDIR)
	cp -r pixmaps $(INSTALLPREFIX)$(DESTDIR)$(RESDIR)
	cp -r sounds $(INSTALLPREFIX)$(DESTDIR)$(RESDIR)
	cp -r ui $(INSTALLPREFIX)$(DESTDIR)$(RESDIR)
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(PIXDIR)
	cp pixmaps/modem-manager-gui.png $(INSTALLPREFIX)$(DESTDIR)$(PIXDIR)
	
uninstall:
	rm -rf $(INSTALLPREFIX)$(DESTDIR)$(RESDIR)
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(PIXDIR)/modem-manager-gui.png


