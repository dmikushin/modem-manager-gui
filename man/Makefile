include ../Makefile_h

FILES_MAN:=$(wildcard *.1)
FILES_MAN:= $(filter-out modem-manager-gui.1, $(FILES_MAN))
FILES_PO:=$(wildcard *.po)
CMANDIR = $(PREFIX)/share/man/man1
LMANDIR = $(PREFIX)/share/man/
LMANFILE = /man1/modem-manager-gui.1

all:
	for f in $(FILES_PO); do \
		po4a-translate -f man -m modem-manager-gui.1 -p $$f -l `basename $$f .po`.1 -k 5; \
	done

install:
	install -d $(INSTALLPREFIX)$(DESTDIR)$(CMANDIR);
	install -m 644 modem-manager-gui.1 $(INSTALLPREFIX)$(DESTDIR)$(CMANDIR);
	gzip -f --best $(INSTALLPREFIX)$(DESTDIR)$(CMANDIR)/modem-manager-gui.1;
	for f in $(FILES_MAN); do \
		install -d $(INSTALLPREFIX)$(DESTDIR)$(LMANDIR)`basename $$f .1`/man1; \
		cp $$f $(INSTALLPREFIX)$(DESTDIR)$(LMANDIR)`basename $$f .1`$(LMANFILE); \
		gzip -f --best $(INSTALLPREFIX)$(DESTDIR)$(LMANDIR)`basename $$f .1`$(LMANFILE); \
	done

uninstall:
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(CMANDIR)/modem-manager-gui.1.gz;
	for f in $(FILES_MAN); do \
		rm -f $(INSTALLPREFIX)$(DESTDIR)$(LMANDIR)`basename $$f .1`$(LMANFILE).gz; \
	done

messages:
	po4a-gettextize -f man -m modem-manager-gui.1 -p modem-manager-gui.pot

clean:
	for f in $(FILES_MAN); do \
		rm -f $$f; \
	done

