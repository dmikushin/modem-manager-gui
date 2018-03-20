include ../Makefile_h

BINDIR    = $(PREFIX)/bin
GCC       = gcc
ifeq ($(ADDLIBSNAMES),)
INC       = `pkg-config --cflags gtk+-3.0 gthread-2.0 gmodule-2.0`
LIB       = `pkg-config --libs gtk+-3.0 gthread-2.0 gmodule-2.0` -lgdbm -lm
else
INC       = `pkg-config --cflags gtk+-3.0 gthread-2.0 gmodule-2.0 $(ADDLIBSNAMES)`
LIB       = `pkg-config --libs gtk+-3.0 gthread-2.0 gmodule-2.0 $(ADDLIBSNAMES)` -lgdbm -lm
endif
OBJ       = settings.o strformat.o libpaths.o dbus-utils.o notifications.o addressbooks.o ayatana.o smsdb.o trafficdb.o providersdb.o modem-settings.o ussdlist.o encoding.o vcard.o netlink.o polkit.o svcmanager.o mmguicore.o contacts-page.o traffic-page.o scan-page.o info-page.o ussd-page.o sms-page.o devices-page.o preferences-window.o welcome-window.o connection-editor-window.o main.o

all: modem-manager-gui

modem-manager-gui: $(OBJ)
	$(GCC) $(INC) $(LDFLAGS) $(OBJ) $(LIB) -o modem-manager-gui

.c.o:
	$(GCC) $(INC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

install:
	install -d $(INSTALLPREFIX)$(DESTDIR)$(BINDIR)
	install modem-manager-gui $(INSTALLPREFIX)$(DESTDIR)$(BINDIR)

uninstall:
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(BINDIR)/modem-manager-gui

clean:
	rm -f *.o
	rm -f modem-manager-gui
