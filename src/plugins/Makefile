include ../../Makefile_h

GCCMOD        = gcc -fPIC
GCCLMOD       = gcc -shared
INCMOD        = `pkg-config --cflags glib-2.0 ofono`
OFONOLIBMOD   = `pkg-config --libs glib-2.0` -lgdbm -lrt
OFONOOBJMOD   = ofonohistory.o
OFONOLIBDIR   = $(LIBPATH)/ofono/plugins/
FILES_OFONO:= $(wildcard libmmgui-ofono*.so)

all: ofonohistory

ifeq ($(OFONOPLUGIN),true)
ofonohistory: $(OFONOOBJMOD)
	$(GCCLMOD) $(INCMOD) $(LDFLAGS) $(OFONOOBJMOD) $(OFONOLIBMOD) -o libmmgui-ofono-history.so
else
ofonohistory:
	
endif

.c.o:
	$(GCCMOD) $(INCMOD) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

install:
	mkdir -p $(INSTALLPREFIX)$(DESTDIR)$(OFONOLIBDIR)
	for f in $(FILES_OFONO); do \
		cp $$f $(INSTALLPREFIX)$(DESTDIR)$(OFONOLIBDIR); \
	done

uninstall:
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(OFONOLIBDIR)/*.so

clean:
	rm -f *.o
	rm -f *.so

