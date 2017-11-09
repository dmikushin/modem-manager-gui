include ../Makefile_h

HELPDIR = $(PREFIX)/share/help

all:
	while read lang; do \
		msgfmt $$lang/$$lang.po -f -v -o $$lang/$$lang.mo;\
		for page in C/*.page; do \
			itstool -m $$lang/$$lang.mo -o $$lang/ $$page; \
		done \
	done < LINGUAS

install:
	install -d $(INSTALLPREFIX)$(DESTDIR)$(HELPDIR)/C/modem-manager-gui
	cp -r C/* $(INSTALLPREFIX)$(DESTDIR)$(HELPDIR)/C/modem-manager-gui
	while read lang; do \
		install -d $(INSTALLPREFIX)$(DESTDIR)$(HELPDIR)/$$lang/modem-manager-gui/figures \
		cp C/figures/* $(INSTALLPREFIX)$(DESTDIR)$(HELPDIR)/$$lang/modem-manager-gui/figures/ \
		cp $$lang/*.page $(INSTALLPREFIX)$(DESTDIR)$(HELPDIR)/$$lang/modem-manager-gui/ \
	done < LINGUAS

uninstall:
	rm -rf $(INSTALLPREFIX)$(DESTDIR)$(HELPDIR)/C/modem-manager-gui
	while read lang; do \
		rm -rf $(INSTALLPREFIX)$(DESTDIR)$(HELPDIR)/$$lang/modem-manager-gui \
	done < LINGUAS

messages:
	itstool -o modem-manager-gui-help.pot C/*.page;
	while read lang; do \
		msgmerge -U $$lang/$$lang.po modem-manager-gui-help.pot; \
	done < LINGUAS

clean:
	while read lang; do \
		rm -f $$lang/$$lang.mo; \
		rm -f $$lang/*.page; \
	done < LINGUAS
