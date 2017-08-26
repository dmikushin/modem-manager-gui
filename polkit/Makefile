include ../Makefile_h

FILES_PO:=$(wildcard *.po)
FILES_MO:=$(patsubst %.po,%.mo,$(FILES_PO))
POLKITACTIONSDIR = $(PREFIX)/share/polkit-1/actions

all:
	for f in $(FILES_PO); do \
		msgfmt $$f -f -v -o `basename $$f .po`.mo; done
	itstool -i polkit.its -j ru.linuxonly.modem-manager-gui.policy.in -o ru.linuxonly.modem-manager-gui.policy $(FILES_MO);

install:
	install -d $(INSTALLPREFIX)$(DESTDIR)$(POLKITACTIONSDIR);
	cp ru.linuxonly.modem-manager-gui.policy $(INSTALLPREFIX)$(DESTDIR)$(POLKITACTIONSDIR)/ru.linuxonly.modem-manager-gui.policy

uninstall:
	rm -f $(INSTALLPREFIX)$(DESTDIR)$(POLKITACTIONSDIR)/ru.linuxonly.modem-manager-gui.policy

messages:
	itstool -i polkit.its -o ru.linuxonly.modem-manager-gui.policy.pot ru.linuxonly.modem-manager-gui.policy.in
	for f in $(FILES_PO); do \
		msgmerge -U $$f ru.linuxonly.modem-manager-gui.policy.pot; done

clean:
	for f in $(FILES_MO); do \
		rm -f $$f; done
	rm -f ru.linuxonly.modem-manager-gui.policy

