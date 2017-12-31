include ../../Makefile_h

NMDISPDIR     = /etc/NetworkManager/dispatcher.d

install:
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(NMDISPDIR)
	install 95-mmgui-timestamp-notifier $(INSTALLPREFIX)$(DESTDIR)$(NMDISPDIR)
	
	
uninstall:
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(NMDISPDIR)/95-mmgui-timestamp-notifier

