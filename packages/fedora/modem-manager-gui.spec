Summary: Modem Manager GUI
Name: modem-manager-gui
Version: 0.0.19.1
Release: 1%{dist}
License: GPLv3
Group: Applications/Communications
URL: https://linuxonly.ru/page/modem-manager-gui
Source:	%{name}-%{version}.tar.gz
Vendor:	Alex <alex@linuxonly.ru>
#BuildArch: x86_64

Requires: gtk3 >= 3.4.0, glib2 >= 2.32.1, gdbm >= 1.10, libnotify >= 0.7.5, gtkspell3 >= 3.0.3, libappindicator-gtk3 >= 0.4.92 
BuildRequires: pkgconfig, po4a, itstool, meson >= 0.37.0, gtk3-devel >= 3.4.0, glib2-devel >= 2.32.1, gdbm-devel >= 1.10, gtkspell3-devel >= 3.0.3, libappindicator-gtk3-devel >= 0.4.92, ofono-devel >= 1.09, desktop-file-utils

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
* Fri Apr 6 2018 Alex <alex@linuxonly.ru> - 0.0.19.1-1.fc27
- Version 0.0.19.1

* Sun Dec 31 2017 Alex <alex@linuxonly.ru> - 0.0.19-1.fc27
- Version 0.0.19

* Sat Jul 6 2013 Alex <alex@linuxonly.ru> - 0.0.16-1.fc19
- Version 0.0.16 fixes

* Wed Aug 08 2012 Alex <alex@linuxonly.ru>
- released spec

