#include "mediaplayer2dbusadaptor.h"


MediaPlayer2DBusAdaptor::MediaPlayer2DBusAdaptor(Waver *waver, QObject *parent) : QDBusAbstractAdaptor(parent)
{
    connect(this, &MediaPlayer2DBusAdaptor::quit,  waver, &Waver::shutdown);
    connect(this, &MediaPlayer2DBusAdaptor::raise, waver, &Waver::raiseButton);
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
    return returnValue;
}


QStringList MediaPlayer2DBusAdaptor::supportedUriSchemes()
{
    QStringList returnValue;
    return returnValue;
}


void MediaPlayer2DBusAdaptor::Quit()
{
    emit quit();
}


void MediaPlayer2DBusAdaptor::Raise()
{
    emit raise();
}
