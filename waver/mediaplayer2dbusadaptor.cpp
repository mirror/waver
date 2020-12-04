#include "mediaplayer2dbusadaptor.h"


MediaPlayer2DBusAdaptor::MediaPlayer2DBusAdaptor(QObject *parent, WaverServer *waverServer) : QDBusAbstractAdaptor(parent)
{
    this->waverServer = waverServer;
}


bool MediaPlayer2DBusAdaptor::canQuit()
{
    return true;
}


bool MediaPlayer2DBusAdaptor::canRaise()
{
    return true;
}


bool MediaPlayer2DBusAdaptor::canSetFullscreen()
{
    return false;
}


QString MediaPlayer2DBusAdaptor::desktopEntry()
{
    return "waver";
}


bool MediaPlayer2DBusAdaptor::fullscreen()
{
    return false;
}


bool MediaPlayer2DBusAdaptor::hasTrackList()
{
    return false;
}


QString MediaPlayer2DBusAdaptor::identity()
{
    return "Waver";
}


void MediaPlayer2DBusAdaptor::setFullscreen(bool fs)
{
    Q_UNUSED(fs);
}


QStringList MediaPlayer2DBusAdaptor::supportedMimeTypes()
{
    QStringList returnValue;

    // TODO this should be more
    returnValue.append("audio/mpeg");

    return returnValue;
}


QStringList MediaPlayer2DBusAdaptor::supportedUriSchemes()
{
    QStringList returnValue;

    returnValue.append("file");
    returnValue.append("http");

    return returnValue;
}


void MediaPlayer2DBusAdaptor::Quit()
{
    waverServer->notificationsHelper_Quit();
}


void MediaPlayer2DBusAdaptor::Raise()
{
    waverServer->notificationsHelper_Raise();
}
