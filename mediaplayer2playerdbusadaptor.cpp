#include "mediaplayer2playerdbusadaptor.h"


MediaPlayer2PlayerDBusAdaptor::MediaPlayer2PlayerDBusAdaptor(Waver *waver, QObject *parent) : QDBusAbstractAdaptor(parent)
{
    this->waver = waver;

    startTimeStamp = QDateTime::currentMSecsSinceEpoch();
    notificationId = 0;

    connect(waver, &Waver::notify, this, &MediaPlayer2PlayerDBusAdaptor::sendCurrentDBusMessage);

    connect(this, &MediaPlayer2PlayerDBusAdaptor::next, waver, &Waver::nextButton);
    connect(this, &MediaPlayer2PlayerDBusAdaptor::pause, waver, &Waver::pauseButton);
    connect(this, &MediaPlayer2PlayerDBusAdaptor::play, waver, &Waver::playButton);
    connect(this, &MediaPlayer2PlayerDBusAdaptor::previous, waver, &Waver::previousButton);
    connect(this, &MediaPlayer2PlayerDBusAdaptor::stop, waver, &Waver::stopButton);
}


MediaPlayer2PlayerDBusAdaptor::~MediaPlayer2PlayerDBusAdaptor()
{
    if (notificationId != 0) {
        QDBusMessage notificationMessage = QDBusMessage::createMethodCall("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "CloseNotification");
        notificationMessage << notificationId;
        QDBusConnection::sessionBus().call(notificationMessage);
    }
}


bool MediaPlayer2PlayerDBusAdaptor::canControl()
{
    return true;
}


bool MediaPlayer2PlayerDBusAdaptor::canGoNext()
{
    return true;
}


bool MediaPlayer2PlayerDBusAdaptor::canGoPrevious()
{
    return true;
}


bool MediaPlayer2PlayerDBusAdaptor::canPause()
{
    return true;
}


bool MediaPlayer2PlayerDBusAdaptor::canPlay()
{
    return true;
}


bool MediaPlayer2PlayerDBusAdaptor::canSeek()
{
    return false;
}


QString MediaPlayer2PlayerDBusAdaptor::loopStatus()
{
    return "None";
}


double MediaPlayer2PlayerDBusAdaptor::maxRate()
{
    return 1.0;
}


QVariantMap MediaPlayer2PlayerDBusAdaptor::metadata()
{
    Track::TrackInfo trackInfo = waver->getCurrentTrackInfo();

    QString mprisId = QString("/org/mpris/MediaPlayer2/Waver/Track/%1").arg(trackInfo.url.fileName(QUrl::FullyEncoded));

    QVariantMap returnValue;

    returnValue.insert("mpris:trackid",     mprisId);
    returnValue.insert("xesam:url",         QString(trackInfo.url.toEncoded()));
    returnValue.insert("xesam:title",       trackInfo.title);
    returnValue.insert("xesam:artist",      QStringList(trackInfo.artist));
    returnValue.insert("xesam:album",       trackInfo.album);
    returnValue.insert("xesam:trackNumber", trackInfo.track);

    if (trackInfo.arts.count() > 0) {
        returnValue.insert("mpris:artUrl", QString(trackInfo.arts.at(0).toEncoded()));
    }

    return returnValue;
}


double MediaPlayer2PlayerDBusAdaptor::minRate()
{
    return 1.0;
}


QString MediaPlayer2PlayerDBusAdaptor::playbackStatus()
{
    Track::Status status = waver->getCurrentTrackStatus();

    if (status == Track::Playing) {
        return "Playing";
    }
    if (status == Track::Paused) {
        return "Paused";
    }
    return "Stopped";
}


qlonglong MediaPlayer2PlayerDBusAdaptor::position()
{
    return waver->getLastPositionMilliseconds() * 1000;
}


double MediaPlayer2PlayerDBusAdaptor::rate()
{
    return 1.0;
}


void MediaPlayer2PlayerDBusAdaptor::setLoopStatus(QString newLoopStatus)
{
    Q_UNUSED(newLoopStatus);
}


void MediaPlayer2PlayerDBusAdaptor::setRate(double r)
{
    Q_UNUSED(r);
}


void MediaPlayer2PlayerDBusAdaptor::setShuffle(bool newShuffle)
{
    Q_UNUSED(newShuffle);
}


void MediaPlayer2PlayerDBusAdaptor::setVolume(double newVolume)
{
    Q_UNUSED(newVolume);
}


bool MediaPlayer2PlayerDBusAdaptor::shuffle()
{
    return true;
}


double MediaPlayer2PlayerDBusAdaptor::volume()
{
    return 0.8;
}


void MediaPlayer2PlayerDBusAdaptor::Next()
{
    emit next();
}


void MediaPlayer2PlayerDBusAdaptor::OpenUri(QString uri)
{
    Q_UNUSED(uri);
}


void MediaPlayer2PlayerDBusAdaptor::Pause()
{
    emit pause();
}


void MediaPlayer2PlayerDBusAdaptor::Play()
{
    emit play();
}


void MediaPlayer2PlayerDBusAdaptor::PlayPause()
{
    if (QDateTime::currentMSecsSinceEpoch() > (startTimeStamp + 2500)) {
        Track::Status status = waver->getCurrentTrackStatus();

        if (status == Track::Playing) {
            emit pause();
        }
        if (status == Track::Paused) {
            emit play();
        }
    }
}


void MediaPlayer2PlayerDBusAdaptor::Previous()
{
    emit previous(0);
}


void MediaPlayer2PlayerDBusAdaptor::Seek(qlonglong offset)
{
    Q_UNUSED(offset);
}


void MediaPlayer2PlayerDBusAdaptor::SetPosition(QString trackId, qlonglong position)
{
    Q_UNUSED(trackId);
    Q_UNUSED(position);
}


void MediaPlayer2PlayerDBusAdaptor::Stop()
{
    emit stop();
}


void MediaPlayer2PlayerDBusAdaptor::sendCurrentDBusMessage(NotificationDataToSend dataToSend)
{
    QVariantMap properties;

    properties.insert("Metadata", metadata());

    if ((dataToSend == All) || (dataToSend == PlaybackStatus)) {
        properties.insert("PlaybackStatus", playbackStatus());

        QDBusMessage mprisMessage = QDBusMessage::createSignal("/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged");
        mprisMessage << "org.mpris.MediaPlayer2.Player";
        mprisMessage << properties;
        mprisMessage << QStringList();

        QDBusConnection::sessionBus().send(mprisMessage);
    }
    if ((dataToSend == All) || (dataToSend == MetaData)) {
        QDBusMessage notificationMessage = QDBusMessage::createMethodCall("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify");
        notificationMessage << "Waver";
        notificationMessage << notificationId;
        notificationMessage << "";
        notificationMessage << properties.value("Metadata").toMap().value("xesam:title");
        notificationMessage << properties.value("Metadata").toMap().value("xesam:artist").toStringList().at(0);
        notificationMessage << QStringList();
        notificationMessage << QVariantMap();
        notificationMessage << 4000;

        QDBusMessage reply = QDBusConnection::sessionBus().call(notificationMessage);

        if ((reply.type() == QDBusMessage::ReplyMessage) && (reply.arguments().count() > 0)) {
            notificationId = reply.arguments().at(0).toUInt();
        }
    }
}
