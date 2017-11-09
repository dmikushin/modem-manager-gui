include ../Makefile_h

LOCALEDIR = $(PREFIX)/share/locale

all:
	while read lang; do \
		msgfmt $$lang.po -f -v -o $$lang.mo; \
	done < LINGUAS

install: 
	while read lang; do \
		install -d $(INSTALLPREFIX)$(DESTDIR)$(LOCALEDIR)/$$lang/LC_MESSAGES; \
		cp $$lang.mo $(INSTALLPREFIX)$(DESTDIR)$(LOCALEDIR)/$$lang/LC_MESSAGES/modem-manager-gui.mo; \
	done < LINGUAS

uninstall:
	while read lang; do \
		rm -f $(INSTALLPREFIX)$(DESTDIR)$(LOCALEDIR)/$$lang/LC_MESSAGES/modem-manager-gui.mo; \
	done < LINGUAS

messages:
	xgettext -k_ -kN_ ../src/*.c -o modem-manager-gui.pot
	xgettext -j -L Glade ../resources/ui/*.ui -o modem-manager-gui.pot
	while read lang; do \
		msgmerge -U $$lang.po modem-manager-gui.pot; \
	done < LINGUAS

clean:
	rm -f *.mo

