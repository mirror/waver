#ifndef MEDIAPLAYER2PLAYERDBUSADAPTOR_H
#define MEDIAPLAYER2PLAYERDBUSADAPTOR_H

#include <QDateTime>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "globals.h"
#include "track.h"
#include "waver.h"


class MediaPlayer2PlayerDBusAdaptor : public QDBusAbstractAdaptor {

        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")

        Q_PROPERTY(bool        CanControl     READ canControl)
        Q_PROPERTY(bool        CanGoNext      READ canGoNext)
        Q_PROPERTY(bool        CanGoPrevious  READ canGoPrevious)
        Q_PROPERTY(bool        CanPause       READ canPause)
        Q_PROPERTY(bool        CanPlay        READ canPlay)
        Q_PROPERTY(bool        CanSeek        READ canSeek)
        Q_PROPERTY(QString     LoopStatus     READ loopStatus WRITE setLoopStatus)
        Q_PROPERTY(double      MaximumRate    READ maxRate)
        Q_PROPERTY(QVariantMap Metadata       READ metadata)
        Q_PROPERTY(double      MinimumRate    READ minRate)
        Q_PROPERTY(QString     PlaybackStatus READ playbackStatus)
        Q_PROPERTY(qlonglong   Position       READ position)
        Q_PROPERTY(double      Rate           READ rate       WRITE setRate)
        Q_PROPERTY(bool        Shuffle        READ shuffle    WRITE setShuffle)
        Q_PROPERTY(double      Volume         READ volume     WRITE setVolume)


    public:

        explicit MediaPlayer2PlayerDBusAdaptor(Waver *waver, QObject *parent);
        ~MediaPlayer2PlayerDBusAdaptor();


    private:

        Waver *waver;

        qint64  startTimeStamp;
        quint32 notificationId;

        bool        canControl();
        bool        canGoNext();
        bool        canGoPrevious();
        bool        canPause();
        bool        canPlay();
        bool        canSeek();
        QString     loopStatus();
        double      maxRate();
        QVariantMap metadata();
        double      minRate();
        QString     playbackStatus();
        qlonglong   position();
        double      rate();
        void        setLoopStatus(QString newLoopStatus);
        void        setRate(double r);
        void        setShuffle(bool newShuffle);
        void        setVolume(double newVolume);
        bool        shuffle();
        double      volume();


    public slots:

        Q_NOREPLY void Next();
        Q_NOREPLY void OpenUri(QString uri);
        Q_NOREPLY void Pause();
        Q_NOREPLY void Play();
        Q_NOREPLY void PlayPause();
        Q_NOREPLY void Previous();
        Q_NOREPLY void Seek(qlonglong offset);
        Q_NOREPLY void SetPosition(QString trackId, qlonglong position);
        Q_NOREPLY void Stop();

        void sendCurrentDBusMessage(NotificationDataToSend dataToSend);


    signals:

        void next();
        void pause();
        void play();
        void previous(int index);
        void stop();

};

#endif // MEDIAPLAYER2PLAYERDBUSADAPTOR_H
