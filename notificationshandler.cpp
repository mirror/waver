/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "notificationshandler.h"


// constructor
NotificationsHandler::NotificationsHandler(Waver *waver) : QObject()
{
    #ifdef Q_OS_LINUX
    if (QDBusConnection::sessionBus().isConnected()) {
        new MediaPlayer2DBusAdaptor(waver, this);
        new MediaPlayer2PlayerDBusAdaptor(waver, this);

        QDBusConnection::sessionBus().registerService("org.mpris.MediaPlayer2.waver");
        QDBusConnection::sessionBus().registerObject("/org/mpris/MediaPlayer2", this);
    }
    #endif

    #ifdef Q_OS_WIN
    trayIcon = new TrayIcon(waver, this);
    #endif
}


// destrucor
NotificationsHandler::~NotificationsHandler()
{
    #ifdef Q_OS_LINUX
    if (QDBusConnection::sessionBus().isConnected()) {
        QDBusConnection::sessionBus().unregisterObject("/org/mpris/MediaPlayer2", QDBusConnection::UnregisterTree);
        QDBusConnection::sessionBus().unregisterService("org.mpris.MediaPlayer2.waver");
    }
    #endif

    #ifdef Q_OS_WIN
    delete trayIcon;
    #endif
}
