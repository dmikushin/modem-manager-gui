include ../Makefile_h

POLKITACTIONSDIR = $(PREFIX)/share/polkit-1/actions

all:
	while read lang; do \
		msgfmt $$lang.po -f -v -o $$lang.mo; \
	done < LINGUAS
	itstool -i its/polkit.its -j ru.linuxonly.modem-manager-gui.policy.in -o ru.linuxonly.modem-manager-gui.policy *.mo

install:
	install -d $(INSTALLPREFIX)$(DESTDIR)$(POLKITACTIONSDIR)
	cp ru.linuxonly.modem-manager-gui.policy $(INSTALLPREFIX)$(DESTDIR)$(POLKITACTIONSDIR)

uninstall:
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(POLKITACTIONSDIR)/ru.linuxonly.modem-manager-gui.policy

messages:
	itstool -i its/polkit.its -o ru.linuxonly.modem-manager-gui.policy.pot ru.linuxonly.modem-manager-gui.policy.in
	while read lang; do \
		msgmerge -U $$lang.po ru.linuxonly.modem-manager-gui.policy.pot; \
	done < LINGUAS

clean:
	rm -f *.mo
	rm -f ru.linuxonly.modem-manager-gui.policy

