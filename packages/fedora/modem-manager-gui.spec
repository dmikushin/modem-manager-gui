Summary: Modem Manager GUI
Name: modem-manager-gui
Version: 0.0.18
Release: 9%{dist}
License: GPLv3
Group: Applications/Communications
URL: http://linuxonly.ru
Source:	%{name}-%{version}.tar.gz
Vendor:	Alex <alex@linuxonly.ru>
#BuildArch: x86_64

Requires: gtk3 >= 3.4.0, glib2 >= 2.32.1, gdbm >= 1.10, libnotify >= 0.7.5, gtkspell3 >= 3.0.3 
BuildRequires: pkgconfig, gtk3-devel >= 3.4.0, glib2-devel >= 2.32.1, gdbm-devel >= 1.10, gtkspell3-devel >= 3.0.3, desktop-file-utils

%description
This program is simple graphical interface for Modem Manager 
daemon dbus interface.
Current features:
- View device information: Operator name, Mode, IMEI, IMSI,
  Signal level.
- Send and receive SMS messages with long massages 
  concatenation and store messages in database.
- Send USSD requests and read answers in GSM7 and UCS2 formats
  converted to system UTF8 charset.
- Scan available mobile networks.

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install INSTALLPREFIX=%{buildroot}
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
%{_datadir}/pixmaps/%{name}.png
%{_datadir}/%{name}/pixmaps/*.png
%{_datadir}/%{name}/sounds/message.ogg
%{_datadir}/%{name}/ui/%{name}.ui
%{_datadir}/help/*
%{_datadir}/appdata/%{name}.appdata.xml
%{_datadir}/polkit-1/actions/ru.linuxonly.%{name}.policy
%{_datadir}/applications/%{name}.desktop
%{_mandir}/*

%changelog
* Sat Jul 6 2013 Alex <alex@linuxonly.ru> - 0.0.16-1.fc19
- Version 0.0.16 fixes

* Wed Aug 08 2012 Alex <alex@linuxonly.ru>
- released spec

