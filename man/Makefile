include ../Makefile_h

CMANDIR = $(PREFIX)/share/man/man1
LMANDIR = $(PREFIX)/share/man

all:
	while read lang; do \
		po4a-translate -f man -m modem-manager-gui.1 -p $$lang/$$lang.po -l $$lang/modem-manager-gui.1 -k 1; \
	done < LINGUAS

install:
	install -d $(INSTALLPREFIX)$(DESTDIR)$(CMANDIR)
	cp modem-manager-gui.1 $(INSTALLPREFIX)$(DESTDIR)$(CMANDIR)
	gzip -f --best $(INSTALLPREFIX)$(DESTDIR)$(CMANDIR)/modem-manager-gui.1
	while read lang; do \
		install -d $(INSTALLPREFIX)$(DESTDIR)$(LMANDIR)/$$lang/man1; \
		cp $$lang/modem-manager-gui.1 $(INSTALLPREFIX)$(DESTDIR)$(LMANDIR)/$$lang/man1; \
		gzip -f --best $(INSTALLPREFIX)$(DESTDIR)$(LMANDIR)/$$lang/man1/modem-manager-gui.1; \
	done < LINGUAS

uninstall:
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(CMANDIR)/modem-manager-gui.1.gz
	while read lang; do \
		rm -f $(INSTALLPREFIX)$(DESTDIR)$(LMANDIR)/$$lang/man1/modem-manager-gui.1.gz; \
	done < LINGUAS

messages:
	po4a-gettextize -f man -m modem-manager-gui.1 -p modem-manager-gui.pot
	while read lang; do \
		msgmerge -U $$lang/$$lang.po modem-manager-gui.pot; \
	done < LINGUAS

clean:
	while read lang; do \
		rm -f $$lang/modem-manager-gui.1; \
	done < LINGUAS

