include ../Makefile_h

RESUIDIR   = $(PREFIX)/share/modem-manager-gui/ui
RESPIXDIR  = $(PREFIX)/share/modem-manager-gui/pixmaps
RESSNDDIR  = $(PREFIX)/share/modem-manager-gui/sounds
PIXDESTDIR = $(PREFIX)/share/pixmaps
APPSDIR    = $(PREFIX)/share/applications

install:
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(RESUIDIR)
	cp modem-manager-gui.ui $(INSTALLPREFIX)$(DESTDIR)$(RESUIDIR)
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp modem-manager-gui.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp dev-tb.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp sms-tb.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp ussd-tb.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp info-tb.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp scan-tb.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp cont-tb.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp traffic-tb.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp sms-unread.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp sms-read.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp message-received.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp message-sent.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp message-drafts.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp info-equipment.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp info-network.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp info-location.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp signal-0.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp signal-25.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp signal-50.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp signal-75.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	cp signal-100.png $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(RESSNDDIR)
	cp message.ogg $(INSTALLPREFIX)$(DESTDIR)$(RESSNDDIR)
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(PIXDESTDIR)
	cp modem-manager-gui.png $(INSTALLPREFIX)$(DESTDIR)$(PIXDESTDIR)
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(APPSDIR)
	cp modem-manager-gui.desktop $(INSTALLPREFIX)$(DESTDIR)$(APPSDIR)
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(POLICYDIR)

uninstall:
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESUIDIR)/modem-manager-gui.ui
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/modem-manager-gui.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/dev-tb.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/sms-tb.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/ussd-tb.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/info-tb.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/scan-tb.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/cont-tb.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/traffic-tb.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/sms-unread.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/sms-read.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/message-received.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/message-sent.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/message-drafts.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/info-equipment.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/info-network.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/info-location.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/signal-0.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/signal-25.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/signal-50.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/signal-75.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESPIXDIR)/signal-100.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(RESSNDDIR)/message.ogg
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(PIXDESTDIR)/modem-manager-gui.png
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(APPSDIR)/modem-manager-gui.desktop

clean:
	rm -f modem-manager-gui.desktop

