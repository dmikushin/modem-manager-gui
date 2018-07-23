Name:    modem-manager-gui
Summary: Modem Manager GUI
Version: 0.0.19.1
Release: 1%{dist}
License: GPLv3
URL:     https://linuxonly.ru/page/modem-manager-gui
Source:	 http://download.tuxfamily.org/gsf/source/modem-manager-gui-%{version}.tar.gz

Requires: filesystem, hicolor-icon-theme, mobile-broadband-provider-info >= 1.20120614, yelp >= 3.10
BuildRequires: gcc, desktop-file-utils, gdbm-devel > 1.10, gettext, glib2-devel > 2.32.1, gtk3-devel >= 3.4.0, gtkspell3-devel >= 3.0.3, itstool, libappindicator-gtk3-devel >= 0.4.92, libappstream-glib, libnotify-devel >= 0.7.5, meson, ofono-devel >= 1.09, pkgconfig, po4a

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
%setup -q

%build
%meson
%meson_build

%install
%meson_install
%find_lang %{name} --with-gnome

%check
appstream-util validate --nonet %{buildroot}/%{_datadir}/metainfo/*.appdata.xml
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop

%clean
rm -rf %{buildroot}

%files -f %{name}.lang
%license LICENSE
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
%{_datadir}/metainfo/%{name}.appdata.xml
%{_datadir}/polkit-1/actions/ru.linuxonly.%{name}.policy
%{_datadir}/applications/%{name}.desktop
%{_mandir}/man1/%{name}.1.*
%{_mandir}/*/man1/%{name}.1.*

%changelog
* Fri Apr 6 2018 Alex <alex@linuxonly.ru> - 0.0.19.1-1.fc27
- Version 0.0.19.1

* Sun Dec 31 2017 Alex <alex@linuxonly.ru> - 0.0.19-1.fc27
- Version 0.0.19

* Sat Jul 6 2013 Alex <alex@linuxonly.ru> - 0.0.16-1.fc19
- Version 0.0.16 fixes

* Wed Aug 08 2012 Alex <alex@linuxonly.ru>
- released spec

