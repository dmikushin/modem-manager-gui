Summary: Modem Manager GUI
Name: modem-manager-gui
Version: 0.0.18
Release: %mkrel 1
License: GPLv3
Group: Communications/Mobile
URL: http://linuxonly.ru
Source:	%{name}-%{version}.tar.gz
Vendor:	Alex <alex@linuxonly.ru>
#BuildArch: x86_64

BuildRequires: pkgconfig
BuildRequires: libgtk+3.0-devel >= 3.4.0
BuildRequires: glib2-devel >= 2.32.1
BuildRequires: libgdbm-devel >= 1.10
BuildRequires: po4a >= 0.45
BuildRequires: itstool >= 1.2.0
BuildRequires: libgtkspell3-devel >= 3.0.6 #Spell checker 0.0.18

Requires: gtk+3.0 >= 3.4.0
Requires: glib2 >= 2.32.1
Requires: libgdbm4 >= 1.10
Requires: modemmanager >= 0.5.0.0
Requires: gtkspell3 >= 3.0.6 #Spell checker 0.0.18
#Requires: libnotify >= 0.7.5
#Suggests: networkmanager
Suggests: libnotify >= 0.7.5
Suggests: libcanberra0 >= 0.28
Suggests: evolution-data-server >= 3.4.1
Suggests: mobile-broadband-provider-info >= 20120614

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
#cp -f %{SOURCE1} ./src/

%build
%configure
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install INSTALLPREFIX=%{buildroot}
#desktop-file-install --vendor mageia --dir %{buildroot}%{_datadir}/applications --add-category X-Mageia --delete-original %{buildroot}%{_datadir}/applications/extras-%{name}.desktop
%find_lang %{name}

%clean
rm -rf %{buildroot}

%files -f %{name}.lang
%defattr(-,root,root,-)
%doc LICENSE
%doc AUTHORS
%doc Changelog
%{_bindir}/%{name}
%{_libdir}/%{name}/modules/*
%{_datadir}/pixmaps/%{name}.png
%{_datadir}/%{name}/pixmaps/*
%{_datadir}/%{name}/sounds/*
%{_datadir}/%{name}/ui/%{name}.ui
%{_datadir}/applications/%{name}.desktop
%{_datadir}/appdata/%{name}.appdata.xml
%{_datadir}/polkit-1/actions/ru.linuxonly.%{name}.policy
%{_mandir}/man1/%{name}.1*
%{_mandir}/*/man1/*
%doc %{_datadir}/help/*/%{name}/*

%changelog

* Sat Oct 10 2015 Alex <alex@linuxonly.ru>
- Version 0.0.18 changes

* Tue Jun 18 2013 Alex <alex@linuxonly.ru>
- Version 0.0.16 changes

* Fri Apr 12 2013 AlexL <loginov.alex.valer@gmail.com>
- fixed Group for Mageia
- added modemmanager to Requires
- added networkmanager to Suggests
- added patch modem-manager-gui.ui

#* Mon Apr 07 2013 Zomby <Zomby@ukr.net>
#- repack for Mageia Russian Community

#* Tue Dec 16 2012 Alex <alex@linuxonly.ru>
#- added additional pictures for 0.0.15 release

* Wed Aug 08 2012 Alex <alex@linuxonly.ru>
- released spec

