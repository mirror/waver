#include "mediaplayer2playerdbusadaptor.h"


MediaPlayer2PlayerDBusAdaptor::MediaPlayer2PlayerDBusAdaptor(QObject *parent, WaverServer *waverServer) : QDBusAbstractAdaptor(parent)
{
    this->ipcMessageUtils = new IpcMessageUtils();
    this->waverServer     = waverServer;

    firstTrack     = true;
    startTimeStamp = QDateTime::currentMSecsSinceEpoch();
    notificationId = 0;

    connect(this->waverServer, SIGNAL(ipcSend(QString)), this, SLOT(waverServerIpcSend(QString)));
}

MediaPlayer2PlayerDBusAdaptor::~MediaPlayer2PlayerDBusAdaptor()
{
    delete this->ipcMessageUtils;
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
    return false;
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
    TrackInfo trackInfo = waverServer->notificationsHelper_Metadata();

    QString mprisId = QString("/org/mpris/MediaPlayer2/Waver/Track/%1").arg(trackInfo.url.fileName(QUrl::FullyEncoded));

    QVariantMap returnValue;

    returnValue.insert("mpris:trackid",     mprisId);
    returnValue.insert("xesam:url",         QString(trackInfo.url.toEncoded()));
    returnValue.insert("xesam:title",       trackInfo.title);
    returnValue.insert("xesam:artist",      QStringList(trackInfo.performer));
    returnValue.insert("xesam:album",       trackInfo.album);
    returnValue.insert("xesam:trackNumber", trackInfo.track);

    if (trackInfo.pictures.count() > 0) {
        returnValue.insert("mpris:artUrl", QString(trackInfo.pictures.at(0).toEncoded()));
    }

    return returnValue;
}


double MediaPlayer2PlayerDBusAdaptor::minRate()
{
    return 1.0;
}


QString MediaPlayer2PlayerDBusAdaptor::playbackStatus()
{
    Track::Status status = waverServer->notificationsHelper_PlaybackStatus();

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
    return waverServer->notificationsHelper_Position();
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
    // TODO implement this
    Q_UNUSED(newVolume);
}


bool MediaPlayer2PlayerDBusAdaptor::shuffle()
{
    return true;
}


double MediaPlayer2PlayerDBusAdaptor::volume()
{
    return waverServer->notificationsHelper_Volume();
}


void MediaPlayer2PlayerDBusAdaptor::Next()
{
    waverServer->notificationsHelper_Next();
}


void MediaPlayer2PlayerDBusAdaptor::OpenUri(QString uri)
{
    waverServer->notificationsHelper_OpenUri(uri);
}


void MediaPlayer2PlayerDBusAdaptor::Pause()
{
    waverServer->notificationsHelper_Pause();
}


void MediaPlayer2PlayerDBusAdaptor::Play()
{
    waverServer->notificationsHelper_Play();
}


void MediaPlayer2PlayerDBusAdaptor::PlayPause()
{
    if (QDateTime::currentMSecsSinceEpoch() > (startTimeStamp + 2500)) {
        waverServer->notificationsHelper_PlayPause();
    }
}


void MediaPlayer2PlayerDBusAdaptor::Previous()
{
    return;
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
    waverServer->notificationsHelper_Stop();
}


void MediaPlayer2PlayerDBusAdaptor::waverServerIpcSend(QString data)
{
    this->ipcMessageUtils->processIpcString(data);

    QVariantMap properties;

    for (int i = 0; i < this->ipcMessageUtils->processedCount(); i++) {
        switch (ipcMessageUtils->processedIpcMessage(i)) {
            case IpcMessageUtils::Pause:
            case IpcMessageUtils::Resume:
                properties.insert("PlaybackStatus", playbackStatus());
                break;
            case IpcMessageUtils::TrackInfos:
                properties.insert("Metadata", metadata());
                properties.insert("PlaybackStatus", playbackStatus());
                break;
            default:
                break;
        }
    }

    if (properties.count() > 0) {
        QDBusMessage mprisMessage = QDBusMessage::createSignal("/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged");
        mprisMessage << "org.mpris.MediaPlayer2.Player";
        mprisMessage << properties;
        mprisMessage << QStringList();

        QDBusConnection::sessionBus().send(mprisMessage);

        if (properties.contains("Metadata")) {
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

            if (reply.type() == QDBusMessage::ReplyMessage) {
                if (reply.arguments().count() > 0) {
                    notificationId = reply.arguments().at(0).toUInt();
                }
            }
        }
    }
}
