#include "notificationshandler.h"

/*

this must be in the Ubuntu installer post script
must be more smart so it doesn't just overwrite but add

gsettings get com.canonical.indicator.sound interested-media-players
gsettings set com.canonical.indicator.sound interested-media-players "['rhythmbox.desktop', 'waver']"

also somehow it must be refreshed otherwise it won't take effect until next login

*/


// constructor
NotificationsHandler::NotificationsHandler(WaverServer *waverServer) : QObject((QObject*)waverServer)
{
    #ifdef Q_OS_LINUX

        if (QDBusConnection::sessionBus().isConnected()) {

            new MediaPlayer2DBusAdaptor((QObject*)this, waverServer);
            new MediaPlayer2PlayerDBusAdaptor((QObject*)this, waverServer);
            // TODO implement org.mpris.MediaPlayer2.TrackList and org.mpris.MediaPlayer2.Playlists

            QDBusConnection::sessionBus().registerService("org.mpris.MediaPlayer2.waver");
            QDBusConnection::sessionBus().registerObject("/org/mpris/MediaPlayer2", this);
        }

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


