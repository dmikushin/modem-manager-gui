Summary: Modem Manager GUI
Name: modem-manager-gui
Version: 0.0.19
Release: 1%{dist}
License: GPLv3
Group: Applications/Communications
URL: http://linuxonly.ru/page/modem-manager-gui
Source:	%{name}-%{version}.tar.gz
Vendor:	Alex <alex@linuxonly.ru>
#BuildArch: x86_64

Requires: gtk3 >= 3.4.0, glib2 >= 2.32.1, gdbm >= 1.10, libnotify >= 0.7.5, gtkspell3 >= 3.0.3, libappindicator-gtk3 >= 0.4.92 
BuildRequires: pkgconfig, gtk3-devel >= 3.4.0, glib2-devel >= 2.32.1, gdbm-devel >= 1.10, gtkspell3-devel >= 3.0.3, libappindicator-gtk3-devel >= 0.4.92, ofono-devel >= 1.09, desktop-file-utils

%description
Simple graphical interface compatible with Modem manager, Wader and oFono
system services able to control EDGE/3G/4G broadband modem specific functions.
Current features:
- Create and control mobile broadband connections
- Send and receive SMS messages and store messages in database
- Initiate USSD requests and read answers (also using interactive sessions)
- View device information: operator name, device mode, IMEI, IMSI, signal level
- Scan available mobile networks
- View mobile traffic statistics and set limits

%prep
%autosetup

%build
%meson
%meson_build

%install
%meson_install
desktop-file-install --dir %{buildroot}%{_datadir}/applications --delete-original %{buildroot}%{_datadir}/applications/%{name}.desktop
%find_lang %{name}

%clean
rm -rf %{buildroot}

%files -f %{name}.lang
%defattr(-,root,root,-)
%doc LICENSE
%doc AUTHORS
%doc Changelog
%{_bindir}/%{name}
%{_libdir}/%{name}/modules/*.so
%{_libdir}/ofono/plugins/libmmgui-ofono-history.so
%{_sysconfdir}/NetworkManager/dispatcher.d/95-mmgui-timestamp-notifier
%{_datadir}/icons/hicolor/128x128/apps/%{name}.png
%{_datadir}/icons/hicolor/scalable/apps/%{name}.svg
%{_datadir}/icons/hicolor/symbolic/apps/%{name}-symbolic.svg
%{_datadir}/%{name}/pixmaps/*.png
%{_datadir}/%{name}/sounds/message.ogg
%{_datadir}/%{name}/ui/%{name}.ui
%{_datadir}/help/*
%{_datadir}/metainfo/%{name}.appdata.xml
%{_datadir}/polkit-1/actions/ru.linuxonly.%{name}.policy
%{_datadir}/applications/%{name}.desktop
%{_mandir}/*

%changelog
* Sat Mar 17 2018 Alex <alex@linuxonly.ru> - 0.0.19-1.fc27
- New connection control functionality for NetworkManager/Connman has been added
- Builtin PIN code input dialog can be used to unlock SIM
- User is informed about PIN/PUK SIM lock
- Active pages can be selected from properties dialog
- User can set custom command to be executed on new SMS reception
- MMGUI notifications can be disabled with GNOME desktop settings center
- Traffic graph movement can be configured from properties dialog
- Modules are checked for compatibility with each other before loading
- Meson build system is fully supported
- Theme MMGUI icons are used when available (thanks to Andrei)
- New monochrome and scalable icons have been added for better desktop integration
- Deprecated GtkStatusIcon has been replaced with AppIndicator (Debian bug #826399)
- UI has been refreshed for better look and feel (thanks to Andrei)
- Registration handler in MM06 module has been fixed (thanks to Alexey)
- Invisible infobar bug has been fixed with GTK+ bug workaround
- Stalled connection to Akonadi server bug has been fixed (Debian bug #834697)
- Akonadi server error handler typo has been corrected
- Timestamp parser for legacy MM versions has been fixed (thanks to Vin)
- Appdata file format has been updated
- Some other bugs have been fixed too
- Most translations have been updated
- Version 0.0.19 release

* Sat Jul 6 2013 Alex <alex@linuxonly.ru> - 0.0.16-1.fc19
- Version 0.0.16 fixes

* Wed Aug 08 2012 Alex <alex@linuxonly.ru>
- released spec

