#include "notificationshandler.h"


// constructor
NotificationsHandler::NotificationsHandler(WaverServer *waverServer) : QObject()
{
    #ifdef Q_OS_LINUX
    if (QDBusConnection::sessionBus().isConnected()) {
        new MediaPlayer2DBusAdaptor((QObject *)this, waverServer);
        new MediaPlayer2PlayerDBusAdaptor((QObject *)this, waverServer);

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
