=================
Modem Manager GUI
=================
Summary
-------
Modem Manager GUI is a simple `GTK`_ based graphical interface compatible with `Modem manager`_, `Wader`_ and `oFono`_ system services able to control EDGE/3G/4G broadband modem specific functions. You can check balance of your SIM card, send or receive SMS messages, control mobile traffic consumption and more using Modem Manager GUI.

Current features:

- Create and control mobile broadband connections
- Send and receive SMS messages and store messages in database
- Initiate USSD requests and read answers (also using interactive sessions)
- View device information: operator name, device mode, IMEI, IMSI, signal level
- Scan available mobile networks
- View mobile traffic statistics and set limits

Main window:

.. image:: https://linuxonly.ru/e107_media/18d0a3f656/images/2017-06/modem_manager_gui_devices.png

Building from source
--------------------
First of all, you'll need to install such development packages:

- GTK version 3.4.0 or later
- Glib version 2.32.1 or later
- GDBM version 1.10 or later
- po4a version 0.45 or later
- itstool version 1.2.0 or later
- libayatana-appindicator3 version 0.4.92 or later (optional)
- libgtkspell3 version 3.0.3 or later (optional)
- Meson version 0.37 or later

For Ubuntu 22.04, this yields to the following list of packages to install:

```
sudo apt install meson libgtk-3-dev libgtkspell3-3-dev libayatana-appindicator3-dev ofono-dev libgdbm-dev po4a itstool
```

Next you have to go to directory with program's source code and issue these commands::

    meson build
    cd build
    ninja
    sudo ninja install

If you prefer good old Autotools, you can use these commands instead::

    sh configure
    make
    sudo make install

Program files will be copied into your system directories. It's better to build package and install it though. Packages specification files are available in 'packages' directory of source code tree.

More info
---------
Please visit `official project's page`_.

.. _`GTK`: https://gtk.org/
.. _`Modem manager`: https://www.freedesktop.org/wiki/Software/ModemManager/
.. _`Wader`: https://github.com/andrewbird/wader
.. _`oFono`: https://01.org/ofono
.. _`official project's page`: https://linuxonly.ru/page/modem-manager-gui/
