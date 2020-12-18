#ifndef MEDIAPLAYER2DBUSADAPTOR_H
#define MEDIAPLAYER2DBUSADAPTOR_H

#include <QDBusAbstractAdaptor>
#include <QObject>
#include <QString>
#include <QStringList>

#include "server.h"


class MediaPlayer2DBusAdaptor : public QDBusAbstractAdaptor {

        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")

        Q_PROPERTY(bool        CanQuit             READ canQuit)
        Q_PROPERTY(bool        CanRaise            READ canRaise)
        Q_PROPERTY(bool        CanSetFullscreen    READ canSetFullscreen)
        Q_PROPERTY(QString     DesktopEntry        READ desktopEntry)
        Q_PROPERTY(bool        Fullscreen          READ fullscreen WRITE setFullscreen)
        Q_PROPERTY(bool        HasTrackList        READ hasTrackList)
        Q_PROPERTY(QString     Identity            READ identity)
        Q_PROPERTY(QStringList SupportedMimeTypes  READ supportedMimeTypes)
        Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)


    public:

        explicit MediaPlayer2DBusAdaptor(QObject *parent, WaverServer *waverServer);


    private:

        WaverServer *waverServer;

        bool        canQuit();
        bool        canRaise();
        bool        canSetFullscreen();
        QString     desktopEntry();
        bool        fullscreen();
        bool        hasTrackList();
        QString     identity();
        void        setFullscreen(bool fs);
        QStringList supportedMimeTypes();
        QStringList supportedUriSchemes();


    public slots:

        Q_NOREPLY void Quit();
        Q_NOREPLY void Raise();

};

#endif // MEDIAPLAYER2DBUSADAPTOR_H
