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
    new TrayIcon((QObject *)this, waverServer);
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
}
