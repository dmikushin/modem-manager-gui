include ../../Makefile_h

GCCMOD     = gcc -fPIC
GCCLMOD    = gcc -shared
INCMOD     = `pkg-config --cflags glib-2.0` `pkg-config --cflags  gmodule-2.0` `pkg-config --cflags gio-2.0`
LIBMOD     = `pkg-config --libs glib-2.0` `pkg-config --libs gmodule-2.0` `pkg-config --libs gio-2.0`
LIBMOD1    = `pkg-config --libs glib-2.0` `pkg-config --libs gmodule-2.0` `pkg-config --libs gio-2.0` -lgdbm
LIBMOD2    = `pkg-config --libs glib-2.0` `pkg-config --libs gmodule-2.0` `pkg-config --libs gio-2.0` -lgdbm
LIBMOD5    = `pkg-config --libs glib-2.0` `pkg-config --libs gmodule-2.0` `pkg-config --libs gio-2.0` -lgdbm -lrt
OBJMOD1    = smsdb.o encoding.o mm06.o
OBJMOD2    = smsdb.o encoding.o mm07.o
OBJMOD3    = uuid.o nm09.o
OBJMOD4    = pppd245.o
OBJMOD5    = smsdb.o encoding.o historyshm.o vcard.o ofono109.o
OBJMOD6    = uuid.o connman112.o
LIBDIR     = $(LIBPATH)/modem-manager-gui/modules/

all: mm06 mm07 nm09 pppd245 ofono109 connman112

#Modem Manager <= 0.6.2
mm06: $(OBJMOD1)
	$(GCCLMOD) $(INCMOD) $(LDFLAGS) $(OBJMOD1) $(LIBMOD1) -o libmodmm_mm06.so
#Modem Manager >= 0.7.0
mm07: $(OBJMOD2)
	$(GCCLMOD) $(INCMOD) $(LDFLAGS) $(OBJMOD2) $(LIBMOD2) -o libmodmm_mm07.so
#Network Manager >= 0.9.0
nm09: $(OBJMOD3)
	$(GCCLMOD) $(INCMOD) $(LDFLAGS) $(OBJMOD3) $(LIBMOD) -o libmodcm_nm09.so
#PPPD >= 2.4.5
pppd245: $(OBJMOD4)
	$(GCCLMOD) $(INCMOD) $(LDFLAGS) $(OBJMOD4) $(LIBMOD) -o libmodcm_pppd245.so
#oFono >= 1.9
ofono109: $(OBJMOD5)
	$(GCCLMOD) $(INCMOD) $(LDFLAGS) $(OBJMOD5) $(LIBMOD5) -o libmodmm_ofono109.so
#Connman >= 1.12
connman112: $(OBJMOD6)
	$(GCCLMOD) $(INCMOD) $(LDFLAGS) $(OBJMOD6) $(LIBMOD) -o libmodcm_connman112.so
#Shared SMS database
smsdb.o: ../smsdb.c
	$(GCCMOD) $(INCMOD) -c $(CPPFLAGS) $(CFLAGS) $< -o $@
#Shared string encoding functions
encoding.o: ../encoding.c
	$(GCCMOD) $(INCMOD) -c $(CPPFLAGS) $(CFLAGS) $< -o $@
#Shared VCard format parser
vcard.o: ../vcard.c
	$(GCCMOD) $(INCMOD) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

.c.o:
	$(GCCMOD) $(INCMOD) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

install:
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(LIBDIR)
	cp *.so $(INSTALLPREFIX)$(DESTDIR)$(LIBDIR)

uninstall:
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(LIBDIR)/*.so

clean:
	rm -f *.o
	rm -f *.so
