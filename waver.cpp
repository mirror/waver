/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/

#include "waver.h"

const QString Waver::UI_ID_PREFIX_LOCALDIR                   = "A";
const QString Waver::UI_ID_PREFIX_LOCALDIR_SUBDIR            = "B";
const QString Waver::UI_ID_PREFIX_LOCALDIR_FILE              = "C";
const QString Waver::UI_ID_PREFIX_SERVER                     = "D";
const QString Waver::UI_ID_PREFIX_SERVER_SEARCH              = "E";
const QString Waver::UI_ID_PREFIX_SERVER_SEARCHRESULT        = "F";
const QString Waver::UI_ID_PREFIX_SERVER_BROWSE              = "G";
const QString Waver::UI_ID_PREFIX_SERVER_BROWSEALPHABET      = "H";
const QString Waver::UI_ID_PREFIX_SERVER_BROWSEARTIST        = "I";
const QString Waver::UI_ID_PREFIX_SERVER_BROWSEALBUM         = "J";
const QString Waver::UI_ID_PREFIX_SERVER_BROWSESONG          = "K";
const QString Waver::UI_ID_PREFIX_SERVER_PLAYLISTS           = "L";
const QString Waver::UI_ID_PREFIX_SERVER_PLAYLIST            = "M";
const QString Waver::UI_ID_PREFIX_SERVER_SMARTPLAYLISTS      = "N";
const QString Waver::UI_ID_PREFIX_SERVER_SMARTPLAYLIST       = "O";
const QString Waver::UI_ID_PREFIX_SERVER_PLAYLIST_ITEM       = "P";
const QString Waver::UI_ID_PREFIX_SERVER_RADIOSTATIONS       = "R";
const QString Waver::UI_ID_PREFIX_SERVER_RADIOSTATION        = "S";
const QString Waver::UI_ID_PREFIX_SERVER_SHUFFLE             = "T";
const QString Waver::UI_ID_PREFIX_SERVER_SHUFFLETAG          = "U";
const QString Waver::UI_ID_PREFIX_SERVER_SHUFFLE_FAVORITES   = "V";
const QString Waver::UI_ID_PREFIX_SERVER_SHUFFLE_NEVERPLAYED = "X";


Waver::Waver() : QObject()
{
    qRegisterMetaType<Track::Status>("Track::Status");
    qRegisterMetaType<NotificationDataToSend>("NotificationDataToSend");

    globalConstantsView = new QQuickView(QUrl("qrc:/qml/GlobalConstants.qml"));
    globalConstants = (QObject*)globalConstantsView->rootObject();

    currentTrack  = nullptr;
    previousTrack = nullptr;

    lastPositionMilliseconds = 0;

    QSettings settings;
    crossfadeTags.append(settings.value("options/crossfade_tags", DEFAULT_CROSSFADE_TAGS).toString().split(","));
    peakFPSMax            = settings.value("options/max_peak_fps", DEFAULT_MAX_PEAK_FPS).toInt();
    peakDelayOn           = settings.value("options/peak_delay_on", DEFAULT_PEAK_DELAY_ON).toBool();
    peakDelayMilliseconds = settings.value("options/peak_delay_ms", DEFAULT_PEAK_DELAY_MS).toInt();

    shuffleCountdownPercent = 1.0;
    shuffleServerIndex      = 0;
    shuffleFirstAfterStart  = true;

    peakCallbackInfo.callbackObject = (PeakCallback *)this;
    peakCallbackInfo.callbackMethod = (PeakCallback::PeakCallbackPointer)&Waver::peakCallback;
    peakCallbackInfo.trackPointer   = nullptr;
    peakCallbackInfo.peakFPS        = &peakFPS;
    peakCallbackInfo.peakFPSMutex   = &peakFPSMutex;

    peakCallbackCount    = 0;
    peakUILagCount       = 0;
    peakLagCount         = 0;
    peakLagIgnoreEnd     = 0;
    peakFPSIncreaseStart = 0;

    peakFPSMutex.lock();
    peakFPS = peakFPSMax;
    peakFPSMutex.unlock();

    stopByShutdown    = false;
    shutdownCompleted = false;

    addToLog("waver", tr("Waver constructed"), "");
}


Waver::~Waver()
{
    if (shuffleCountdownTimer != nullptr) {
        shuffleCountdownTimer->stop();
        delete shuffleCountdownTimer;
        shuffleCountdownTimer = nullptr;
    }

    foreach (FileScanner *fileScanner, fileScanners) {
        disconnect(fileScanner, &FileScanner::finished, this, &Waver::fileScanFinished);
        fileScanner->requestInterruption();
        fileScanner->wait();
        delete fileScanner;
    }

    foreach(AmpacheServer *server, servers) {
        delete server;
    }

    globalConstantsView->deleteLater();

    addToLog("waver", tr("Waver destructed"), "");
}


void Waver::actionPlay(Track::TrackInfo trackInfo)
{
    Track *track = new Track(trackInfo, peakCallbackInfo);
    connectTrackSignals(track);

    actionPlay(track);
}


void Waver::actionPlay(Track *track)
{
    killPreviousTrack();

    playlist.prepend(track);
    if (currentTrack == nullptr) {
        startNextTrack();
    }
    else {
        bool crossfade = isCrossfade(currentTrack, track);

        previousTrack = currentTrack;
        currentTrack  = nullptr;

        if (!crossfade) {
            previousTrack->setStatus(Track::Paused);
        }
        previousTrack->setStatus(Track::Idle);
        if (crossfade) {
            startNextTrack();
        }
    }
}


void Waver::addServer(QString host, QString user, QString psw)
{
    AmpacheServer *server = new AmpacheServer(QUrl(host), user);
    QString        id     = QString("%1%2").arg(UI_ID_PREFIX_SERVER).arg(servers.size());

    server->setId(id);
    server->setPassword(psw);
    server->setSettingsId(QUuid::createUuid());

    servers.append(server);
    emit explorerAddItem(id, QVariant::fromValue(nullptr), server->formattedName(), "qrc:/icons/remote.ico", QVariant::fromValue(nullptr), true, false, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));

    connect(server, &AmpacheServer::operationFinished, this, &Waver::serverOperationFinished);
    connect(server, &AmpacheServer::errorMessage,      this, &Waver::errorMessage);

    QSettings settings;

    settings.remove("servers");
    settings.beginWriteArray("servers");
    for(int i = 0; i < servers.size(); i++) {
        settings.setArrayIndex(i);
        settings.setValue("urlString", servers.at(i)->getHost().toString());
        settings.setValue("userName", servers.at(i)->getUser());
        settings.setValue("settingsId", servers.at(i)->getSettingsId().toString());
    }
    settings.endArray();

    addToLog(id, tr("Server added"), server->formattedName());
}


void Waver::addToLog(QString id, QString info, QString error)
{
    LogItem item = { QDateTime::currentDateTime(), id, info, error };
    log.append(item);

    while (log.size() > 250) {
        log.removeFirst();
    }

    emit logUpdate(QString("%1: <%2>, '%3', '%4'\n").arg(item.dateTime.toString(), item.id, item.error, item.info));
}


QChar Waver::alphabetFromName(QString name)
{
    name.replace(QRegExp("^(The|An|A|Die|Das|Ein|Eine|Les|Le|La)\\s+"), "").at(0);

    QChar returnValue = name.at(0).toUpper();

    if (!returnValue.isLetter()) {
        returnValue = '#';
    }

    return returnValue;
}


void Waver::clearTrackUISignals()
{
    emit uiSetTrackData("", "", "", "", "");
    emit uiSetTrackBusy(false);
    emit uiSetTrackLength("");
    emit uiSetTrackPosition("", 0, 0);
    emit uiSetTrackTags("");
    emit uiSetTrackBufferData("");
    emit uiSetTrackReplayGain("", "");
    emit uiSetImage("qrc:/images/waver.png");
    emit uiSetStatusText(tr("Stopped"));
}


void Waver::connectTrackSignals(Track *track, bool newConnect)
{
    if (newConnect) {
        connect(track, &Track::playPosition,      this, &Waver::trackPlayPosition);
        connect(track, &Track::decoded,           this, &Waver::trackDecoded);
        connect(track, &Track::bufferInfo,        this, &Waver::trackBufferInfo);
        connect(track, &Track::networkConnecting, this, &Waver::trackNetworkConnecting);
        connect(track, &Track::replayGainInfo,    this, &Waver::trackReplayGainInfo);
        connect(track, &Track::finished,          this, &Waver::trackFinished);
        connect(track, &Track::fadeoutStarted,    this, &Waver::trackFadeoutStarted);
        connect(track, &Track::trackInfoUpdated,  this, &Waver::trackInfoUpdated);
        connect(track, &Track::error,             this, &Waver::trackError);
        connect(track, &Track::info,              this, &Waver::trackInfo);
        connect(track, &Track::sessionExpired,   this, &Waver::trackSessionExpired);
        connect(track, &Track::statusChanged,     this, &Waver::trackStatusChanged);

        connect(this,  &Waver::requestTrackBufferReplayGainInfo, track, &Track::requestForBufferReplayGainInfo);

        return;
    }

    disconnect(track, &Track::playPosition,      this, &Waver::trackPlayPosition);
    disconnect(track, &Track::decoded,           this, &Waver::trackDecoded);
    disconnect(track, &Track::bufferInfo,        this, &Waver::trackBufferInfo);
    disconnect(track, &Track::networkConnecting, this, &Waver::trackNetworkConnecting);
    disconnect(track, &Track::replayGainInfo,    this, &Waver::trackReplayGainInfo);
    disconnect(track, &Track::finished,          this, &Waver::trackFinished);
    disconnect(track, &Track::fadeoutStarted,    this, &Waver::trackFadeoutStarted);
    disconnect(track, &Track::trackInfoUpdated,  this, &Waver::trackInfoUpdated);
    disconnect(track, &Track::error,             this, &Waver::trackError);
    disconnect(track, &Track::info,              this, &Waver::trackInfo);
    disconnect(track, &Track::sessionExpired,   this, &Waver::trackSessionExpired);
    disconnect(track, &Track::statusChanged,     this, &Waver::trackStatusChanged);

    disconnect(this,  &Waver::requestTrackBufferReplayGainInfo, track, &Track::requestForBufferReplayGainInfo);
}


void Waver::deleteServer(QString id)
{
    int index = serverIndex(id);
    if (index < 0) {
        return;
    }

    emit explorerRemoveItem(servers.at(index)->getId());

    QUuid settingsID = servers.at(index)->getSettingsId();

    QString formattedName = servers.at(index)->formattedName();

    delete servers.at(index);
    servers.removeAt(index);

    QSettings settings;

    settings.remove(settingsID.toString());

    settings.remove("servers");
    settings.beginWriteArray("servers");
    for(int i = 0; i < servers.size(); i++) {
        settings.setArrayIndex(i);
        settings.setValue("urlString", servers.at(i)->getHost().toString());
        settings.setValue("userName", servers.at(i)->getUser());
        settings.setValue("settingsId", servers.at(i)->getSettingsId().toString());
    }
    settings.endArray();

    addToLog(id, tr("Server deleted"), formattedName);
}


void Waver::errorMessage(QString id, QString info, QString error)
{
    #ifdef QT_DEBUG
        qDebug() << id << info << error;
    #endif

    addToLog(id, info, error);

    emit uiSetStatusTempText(info);
}


void Waver::explorerItemClicked(QString id, int action, QString extraJSON)
{
    QVariantMap extra = QJsonDocument::fromJson(extraJSON.toUtf8()).toVariant().toMap();

    if (extra.contains("art") && (action != globalConstant("action_collapse"))) {
        emit uiSetTempImage(extra.value("art"));
    }

    if (id.startsWith(UI_ID_PREFIX_LOCALDIR) || id.startsWith(UI_ID_PREFIX_LOCALDIR_SUBDIR) || id.startsWith(UI_ID_PREFIX_LOCALDIR_FILE)) {
        itemActionLocal(id, action, extra);
        return;
    }
    if (id.startsWith(UI_ID_PREFIX_SERVER)) {
        itemActionServer(id, action, extra);
        return;
    }
    itemActionServerItem(id, action, extra);
}


void Waver::explorerNetworkingUISignals(QString id, bool networking)
{
    QString statusText = networking ? tr("Networking") : tr("Stopped");

    if (!networking && (currentTrack != nullptr)) {
        statusText = currentTrack->getStatusText();
    }

    emit explorerSetBusy(id, networking);
    emit uiSetStatusText(statusText);
}


void Waver::favoriteButton(bool fav)
{
    if (currentTrack == nullptr) {
        return;
    }

    if (fav) {
        currentTrack->attributeAdd("flag", "true");
    }
    else {
        currentTrack->attributeRemove("flag");
    }

    emit explorerSetFlagExtra(getCurrentTrackInfo().id, fav);

    QStringList idParts = getCurrentTrackInfo().id.split("|");

    QString serverId = idParts.last();
    int     srvIndex = serverIndex(serverId);
    if ((srvIndex >= servers.size()) || (srvIndex < 0)) {
        errorMessage(serverId, tr("Server ID can not be found"), serverId);
        return;
    }

    servers.at(srvIndex)->startOperation(AmpacheServer::SetFlag, { { "song_id", idParts.first().mid(1) }, { "flag", fav ? "1" : "0" } });

    addToLog(getCurrentTrackInfo().id, getCurrentTrackInfo().title, fav ? tr("Favorite flag added") : tr("Favorite flag removed"));
}


void Waver::fileScanFinished()
{
    FileScanner *fileScanner = (FileScanner*) QObject::sender();

    disconnect(fileScanner, &FileScanner::finished, this, &Waver::fileScanFinished);

    QRandomGenerator *randomGenerator = QRandomGenerator::global();

    int counter = 0;
    foreach (QFileInfo dir, fileScanner->getDirs()) {
        counter++;
        emit explorerAddItem(QString("%1%2").arg(UI_ID_PREFIX_LOCALDIR_SUBDIR).arg(randomGenerator->bounded(std::numeric_limits<quint32>::max())), fileScanner->getUiData(), dir.baseName(), "qrc:/icons/browse.ico", QVariantMap({{ "path", dir.absoluteFilePath() }}), true, false, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
    }
    foreach (QFileInfo file, fileScanner->getFiles()) {
        counter++;
        emit explorerAddItem(QString("%1%2").arg(UI_ID_PREFIX_LOCALDIR_FILE).arg(randomGenerator->bounded(std::numeric_limits<quint32>::max())), fileScanner->getUiData(), file.fileName(), "qrc:/icons/audio_file.ico", QVariantMap({{ "path", file.absoluteFilePath() }}), false, true, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
    }

    emit explorerSetBusy(fileScanner->getUiData(), false);

    if (!counter) {
        errorMessage("local", tr("No audio files found in directory"), fileScanner->getUiData());
    }

    delete fileScanner;
    fileScanners.removeAll(fileScanner);
}


QString Waver::formatFrequencyValue(double hertz)
{
    if (hertz >= 1000) {
        return QString("%1KHz").arg(static_cast<double>(hertz) / 1000, 0, 'f', 1);
    }
    return QString("%1Hz").arg(static_cast<double>(hertz), 0, 'f', 0);
}


QString Waver::formatMemoryValue(unsigned long bytes)
{
    if (bytes > (1024 * 1024)) {
        return QString("%1MB").arg(static_cast<double>(bytes) / (1024 * 1024), 0, 'f', 2);
    }

    if (bytes > 1024) {
        return QString("%1KB").arg(static_cast<double>(bytes) / 1024, 0, 'f', 2);
    }

    return QString("%1B").arg(static_cast<double>(bytes), 0, 'f', 0);
}


Track::TrackInfo Waver::getCurrentTrackInfo()
{
    if (currentTrack == nullptr) {
        Track::TrackInfo emptyTrackInfo;
        emptyTrackInfo.track = 0;
        emptyTrackInfo.year  = 0;
        return emptyTrackInfo;
    }
    return currentTrack->getTrackInfo();
}


Track::Status Waver::getCurrentTrackStatus()
{
    if (currentTrack == nullptr) {
        return Track::Idle;
    }
    return currentTrack->getStatus();
}


long Waver::getLastPositionMilliseconds()
{
    return lastPositionMilliseconds;
}


QVariant Waver::globalConstant(QString constName)
{
    QVariant returnValue = globalConstants->property(constName.toLatin1());
    assert (returnValue.isValid());
    return returnValue;
}


bool Waver::isCrossfade(Track *track1, Track *track2)
{
    if ((track1 == nullptr) || (track2 == nullptr)) {
        return false;
    }

    bool crossfade1 = false;
    bool crossfade2 = false;
    foreach(QString crossfadeTag, crossfadeTags) {
        if (track1->getTrackInfo().tags.contains(crossfadeTag, Qt::CaseInsensitive)) {
            crossfade1 = true;
        }
        if (track2->getTrackInfo().tags.contains(crossfadeTag, Qt::CaseInsensitive)) {
            crossfade2 = true;
        }
    }

    return (crossfade1 && crossfade2);
}


void Waver::itemActionLocal(QString id, int action, QVariantMap extra)
{
    if ((action == globalConstant("action_expand")) || (action == globalConstant("action_refresh"))) {
        stopShuffleCountdown();

        emit explorerSetBusy(id, true);
        emit explorerRemoveChildren(id);

        FileScanner *fileScanner = new FileScanner(extra.value("path").toString(), id);
        connect(fileScanner, &FileScanner::finished, this, &Waver::fileScanFinished);
        fileScanners.append(fileScanner);

        fileScanner->start();
    }
    else if (action == globalConstant("action_play")) {
        stopShuffleCountdown();

        playlist.clear();
        Track::TrackInfo trackInfo = trackInfoFromFilePath(extra.value("path").toString());
        actionPlay(trackInfo);
    }
    else if ((action == globalConstant("action_playnext")) || (action == globalConstant("action_enqueue"))) {
        stopShuffleCountdown();

        Track::TrackInfo trackInfo = trackInfoFromFilePath(extra.value("path").toString());

        Track *track = new Track(trackInfo, peakCallbackInfo);
        connectTrackSignals(track);

        if (action == globalConstant("action_enqueue")) {
            playlist.append(track);
        }
        else {
            playlist.prepend(track);
        }
        playlistUpdateUISignals();
    }
    else if (action == globalConstant("action_collapse")) {
        emit explorerRemoveChildren(id);
    }
}


void Waver::itemActionServer(QString id, int action, QVariantMap extra)
{
    Q_UNUSED(extra);

    if ((action == globalConstant("action_expand")) || (action == globalConstant("action_refresh"))) {
        stopShuffleCountdown();

        emit explorerRemoveChildren(id);

        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SEARCH), id),              id, tr("Search"),          "qrc:/icons/search.ico",        QVariant::fromValue(nullptr), true, false, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_BROWSE), id),              id, tr("Browse"),          "qrc:/icons/browse.ico",        QVariant::fromValue(nullptr), true, false, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_PLAYLISTS), id),           id, tr("Playlists"),       "qrc:/icons/playlist.ico",      QVariant::fromValue(nullptr), true, false, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SMARTPLAYLISTS), id),      id, tr("Smart Playlists"), "qrc:/icons/playlist.ico",      QVariant::fromValue(nullptr), true, false, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_RADIOSTATIONS), id),       id, tr("Radio Stations"),  "qrc:/icons/radio_station.ico", QVariant::fromValue(nullptr), true, false, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SHUFFLE), id),             id, tr("Shuffle"),         "qrc:/icons/shuffle.ico",       QVariant::fromValue(nullptr), true, true,  QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SHUFFLE_FAVORITES), id),   id, tr("Favorites"),        "qrc:/icons/shuffle.ico",       QVariant::fromValue(nullptr), false, true, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SHUFFLE_NEVERPLAYED), id), id, tr("Never Played"),    "qrc:/icons/shuffle.ico",       QVariant::fromValue(nullptr), false, true, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));

        emit explorerDisableQueueable(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SHUFFLE), id));
        emit explorerDisableQueueable(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SHUFFLE_FAVORITES), id));
        emit explorerDisableQueueable(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SHUFFLE_NEVERPLAYED), id));
    }
    else if (action == globalConstant("action_collapse")) {
        emit explorerRemoveChildren(id);
    }
}


void Waver::itemActionServerItem(QString id, int action, QVariantMap extra)
{
    stopShuffleCountdown();

    QStringList idParts = id.split("|");

    QString serverId = idParts.last();
    int     srvIndex = serverIndex(serverId);
    if ((srvIndex >= servers.size()) || (srvIndex < 0)) {
        errorMessage(serverId, tr("Server ID can not be found"), serverId);
        return;
    }

    QSettings settings;

    if (action == globalConstant("action_expand")) {
        if ((id.startsWith(UI_ID_PREFIX_SERVER_BROWSE) || id.startsWith(UI_ID_PREFIX_SERVER_BROWSEALPHABET))) {
            QString browseCacheKey = QString("%1/browse").arg(servers.at(srvIndex)->getSettingsId().toString());

            if (settings.contains(QString("%1/1/id").arg(browseCacheKey))) {
                QRandomGenerator *randomGenerator = QRandomGenerator::global();
                QChar             currentChar;

                int browseSize = settings.beginReadArray(browseCacheKey);
                for (int i = 0; i < browseSize; i++) {
                    settings.setArrayIndex(i);

                    QString name = settings.value("name").toString();

                    if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSE) && (currentChar != alphabetFromName(name))) {
                        currentChar = alphabetFromName(name);
                        QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_BROWSEALPHABET).arg(randomGenerator->bounded(std::numeric_limits<quint32>::max())).arg(serverId);
                        emit explorerAddItem(newId, id, currentChar, "qrc:/icons/browse.ico", QVariantMap({{ "alphabet", currentChar }}), true, false, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
                    }
                    if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEALPHABET) && extra.value("alphabet").toString().startsWith(alphabetFromName(name))) {
                        QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_BROWSEARTIST, settings.value("id").toString(), serverId);
                        emit explorerAddItem(newId, id, name, settings.value("art"), QVariantMap({{ "art", settings.value("art") }}), true, true, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
                    }
                }
                settings.endArray();
                return;
            }
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_PLAYLISTS) || id.startsWith(UI_ID_PREFIX_SERVER_SMARTPLAYLISTS)) {
            QString playlistsCacheKey = QString("%1/playlists").arg(servers.at(srvIndex)->getSettingsId().toString());
            bool    hideDotPlaylists  = settings.value("options/hide_dot_playlists", DEFAULT_HIDE_DOT_PLAYLIST).toBool();

            if (settings.contains(QString("%1/1/id").arg(playlistsCacheKey))) {
                int playlistsSize = settings.beginReadArray(playlistsCacheKey);
                for (int i = 0; i < playlistsSize; i++) {
                    settings.setArrayIndex(i);

                    QString playlistId   = settings.value("id").toString();
                    QString playlistName = settings.value("name").toString();

                    if (id.startsWith(UI_ID_PREFIX_SERVER_PLAYLISTS) && !playlistId.startsWith("smart", Qt::CaseInsensitive) && (!playlistName.startsWith(".") || !hideDotPlaylists)) {
                        QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_PLAYLIST, playlistId, serverId);
                        emit explorerAddItem(newId, id, playlistName, "qrc:/icons/playlist.ico", QVariantMap({{ "group", playlistName }}), false, true, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
                    }
                    if (id.startsWith(UI_ID_PREFIX_SERVER_SMARTPLAYLISTS) && playlistId.startsWith("smart", Qt::CaseInsensitive) && (!playlistName.startsWith(".") || !hideDotPlaylists)) {
                        QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_SMARTPLAYLIST, playlistId, serverId);
                        emit explorerAddItem(newId, id, playlistName, "qrc:/icons/playlist.ico", QVariantMap({{ "group", playlistName }}), false, true, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
                    }
                }
                settings.endArray();
                return;
            }
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_RADIOSTATIONS)) {
            QString radioStationsCacheKey = QString("%1/radiostations").arg(servers.at(srvIndex)->getSettingsId().toString());

            if (settings.contains(QString("%1/1/id").arg(radioStationsCacheKey))) {
                int radiosSize = settings.beginReadArray(radioStationsCacheKey);
                for (int i = 0; i < radiosSize; i++) {
                    settings.setArrayIndex(i);

                    QString newId         = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_RADIOSTATION, settings.value("id").toString(), serverId);
                    QString unescapedName = QTextDocumentFragment::fromHtml(settings.value("name").toString()).toPlainText();

                    emit explorerAddItem(newId, id, unescapedName, "qrc:/icons/radio_station.ico", QVariantMap({ { "name", unescapedName }, { "url", settings.value("url") } }), false, true, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
                    emit explorerDisableQueueable(newId);
                }
                settings.endArray();
                return;
            }
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_SHUFFLE)) {
            QString tagsCacheKey = QString("%1/tags").arg(servers.at(srvIndex)->getSettingsId().toString());

            if (settings.contains(QString("%1/1/id").arg(tagsCacheKey))) {
                int tagsSize = settings.beginReadArray(tagsCacheKey);
                for (int i = 0; i < tagsSize; i++) {
                    settings.setArrayIndex(i);

                    QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_SHUFFLETAG, settings.value("id").toString(), serverId);

                    emit explorerAddItem(newId, id, settings.value("name"), "qrc:/icons/tag.ico", QVariant::fromValue(nullptr), false, false, true, servers.at(srvIndex)->isShuffleTagSelected(settings.value("id").toInt()));
                }
                settings.endArray();
                return;
            }
        }

        action = globalConstant("action_refresh").toInt();
    }

    if (action == globalConstant("action_collapse")) {
        if (!id.startsWith(UI_ID_PREFIX_SERVER_SEARCH)) {
            emit explorerRemoveChildren(id);
            return;
        }

        action = globalConstant("action_refresh").toInt();
    }

    if (action == globalConstant("action_refresh")) {
        if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEALPHABET)) {
            id = QString("%1|%2").arg(QString(serverId).replace(0, 1, UI_ID_PREFIX_SERVER_BROWSE), serverId);
        }

        explorerNetworkingUISignals(id, true);
        emit explorerRemoveChildren(id);

        if (id.startsWith(UI_ID_PREFIX_SERVER_SEARCH)) {
            servers.at(srvIndex)->startOperation(AmpacheServer::Search, {{ "criteria", extra.value("criteria").toString() }});
            addToLog("waver", tr("Starting operation - Search"), "");
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSE)) {
            servers.at(srvIndex)->startOperation(AmpacheServer::BrowseRoot, AmpacheServer::OpData());
            addToLog("waver", tr("Starting operation - Browse Root"), "");
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEARTIST)) {
            servers.at(srvIndex)->startOperation(AmpacheServer::BrowseArtist, {{ "artist", QString(idParts.first()).remove(0, 1) }});
            addToLog("waver", tr("Starting operation - Browse Artist"), "");
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEALBUM)) {
            servers.at(srvIndex)->startOperation(AmpacheServer::BrowseAlbum, {{ "album", QString(idParts.first()).remove(0, 1) }});
            addToLog("waver", tr("Starting operation - Browse Album"), "");
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_PLAYLISTS) || id.startsWith(UI_ID_PREFIX_SERVER_SMARTPLAYLISTS)) {
            QString playlistsId     = QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_PLAYLISTS);
            QString smartPlaylistId = QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SMARTPLAYLISTS);

            explorerNetworkingUISignals(playlistsId, true);
            explorerNetworkingUISignals(smartPlaylistId, true);
            emit explorerRemoveChildren(playlistsId);
            emit explorerRemoveChildren(smartPlaylistId);

            servers.at(srvIndex)->startOperation(AmpacheServer::PlaylistRoot, AmpacheServer::OpData());
            addToLog("waver", tr("Starting operation - Playlist Root"), "");
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_RADIOSTATIONS)) {
            servers.at(srvIndex)->startOperation(AmpacheServer::RadioStations, AmpacheServer::OpData());
            addToLog("waver", tr("Starting operation - Radio Stations"), "");
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_SHUFFLE)) {
            servers.at(srvIndex)->startOperation(AmpacheServer::Tags, AmpacheServer::OpData());
            addToLog("waver", tr("Starting operation - Tags"), "");
        }
        return;
    }

    if (action == globalConstant("action_play")) {
        if (id.startsWith(UI_ID_PREFIX_SERVER_SEARCHRESULT)) {
            playlist.clear();
            Track::TrackInfo trackInfo = trackInfoFromIdExtra(id, extra);
            actionPlay(trackInfo);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEARTIST)) {
            bool OK = false;
            int  artistId = QString(idParts.first()).remove(0, 1).toInt(&OK);

            if (OK) {
                startShuffleBatch(srvIndex, artistId);
            }
            return;
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEALBUM)) {
            explorerNetworkingUISignals(id, true);
            emit playlistBigBusy(true);

            QObject *opExtra = new QObject();
            opExtra->setProperty("original_action", "action_play");
            opExtra->setProperty("group", extra.value("group").toString());

            servers.at(srvIndex)->startOperation(AmpacheServer::BrowseAlbum, {{ "album", QString(idParts.first()).remove(0, 1) }}, opExtra);
            addToLog("waver", tr("Starting operation - Browse Album"), "");
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSESONG)) {
            playlist.clear();
            Track::TrackInfo trackInfo = trackInfoFromIdExtra(id, extra);
            actionPlay(trackInfo);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_PLAYLIST) || id.startsWith(UI_ID_PREFIX_SERVER_SMARTPLAYLIST)) {
            explorerNetworkingUISignals(id, true);
            emit playlistBigBusy(true);

            QObject *opExtra = new QObject();
            opExtra->setProperty("original_action", "action_play");
            opExtra->setProperty("group", extra.value("group").toString());

            servers.at(srvIndex)->startOperation(AmpacheServer::PlaylistSongs, {{ "playlist", QString(idParts.first()).remove(0, 1) }}, opExtra);
            addToLog("waver", tr("Starting operation - Playlist Songs"), "");
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_RADIOSTATION)) {
            playlist.clear();

            extra.insert("artist", extra.value("name"));
            extra.insert("album", tr("Radio Station"));
            extra.insert("art", "qrc:/icons/radio_station.ico");
            Track::TrackInfo trackInfo = trackInfoFromIdExtra(id, extra);
            trackInfo.attributes.insert("radio_station", "radio_station");

            actionPlay(trackInfo);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_SHUFFLE)) {
            startShuffleBatch(srvIndex);
            return;
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_SHUFFLE_FAVORITES)) {
            startShuffleBatch(srvIndex, 0, Favorite);
            return;
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_SHUFFLE_NEVERPLAYED)) {
            startShuffleBatch(srvIndex, 0, NeverPlayed);
            return;
        }
        return;
    }

    if ((action == globalConstant("action_playnext")) || (action == globalConstant("action_enqueue"))) {
        if (id.startsWith(UI_ID_PREFIX_SERVER_SEARCHRESULT)) {
            Track *track = new Track(trackInfoFromIdExtra(id, extra), peakCallbackInfo);
            connectTrackSignals(track);

            if (action == globalConstant("action_enqueue")) {
                playlist.append(track);
            }
            else {
                playlist.prepend(track);
            }

            playlistUpdateUISignals();
            playlistFirstGroupSave();
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEARTIST)) {
            bool OK = false;
            int  artistId = QString(idParts.first()).remove(0, 1).toInt(&OK);

            if (OK) {
                startShuffleBatch(srvIndex, artistId, None, action == globalConstant("action_playnext") ? "action_playnext" : "action_enqueue");
            }
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEALBUM)) {
            explorerNetworkingUISignals(id, true);
            emit playlistBigBusy(true);

            QObject *opExtra = new QObject();
            opExtra->setProperty("original_action", action == globalConstant("action_playnext") ? "action_playnext" : "action_enqueue");
            opExtra->setProperty("group", extra.value("group").toString());

            servers.at(srvIndex)->startOperation(AmpacheServer::BrowseAlbum, {{ "album", QString(idParts.first()).remove(0, 1) }}, opExtra);
            addToLog("waver", tr("Starting operation - Browse Album"), "");
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSESONG)) {
            Track *track = new Track(trackInfoFromIdExtra(id, extra), peakCallbackInfo);
            connectTrackSignals(track);

            if (action == globalConstant("action_enqueue")) {
                playlist.append(track);
            }
            else {
                playlist.prepend(track);
            }

            playlistUpdateUISignals();
            playlistFirstGroupSave();
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_PLAYLIST) || id.startsWith(UI_ID_PREFIX_SERVER_SMARTPLAYLIST)) {
            explorerNetworkingUISignals(id, true);
            emit playlistBigBusy(true);

            QObject *opExtra = new QObject();
            opExtra->setProperty("original_action", action == globalConstant("action_playnext") ? "action_playnext" : "action_enqueue");
            opExtra->setProperty("group", extra.value("group").toString());

            servers.at(srvIndex)->startOperation(AmpacheServer::PlaylistSongs, {{ "playlist", QString(idParts.first()).remove(0, 1) }}, opExtra);
            addToLog("waver", tr("Starting operation - Playlist Songs"), "");
        }
        return;
    }

    if (action == globalConstant("action_select")) {
        if (id.startsWith(UI_ID_PREFIX_SERVER_SHUFFLETAG)) {
            int tagId            = QString(idParts.first()).remove(0, 1).toInt();
            bool currentSelected = servers.at(srvIndex)->isShuffleTagSelected(tagId);

            servers.at(srvIndex)->setShuffleTag(tagId, !currentSelected);
            emit explorerSetSelected(id, !currentSelected);
        }
        return;
    }
}


bool Waver::isShutdownCompleted()
{
    bool returnValue;

    shutdownMutex.lock();
    returnValue = shutdownCompleted;
    shutdownMutex.unlock();

    return returnValue;
}


void Waver::killPreviousTrack()
{
    if (previousTrack != nullptr) {
        connectTrackSignals(previousTrack, false);
        previousTrack->setStatus(Track::Paused);
        previousTrack->setStatus(Track::Idle);

        addToLog(previousTrack->getTrackInfo().id, previousTrack->getTrackInfo().title, tr("Track killed"));

        delete previousTrack;
        previousTrack = nullptr;
    }
}


void Waver::nextButton()
{
    addToLog("waver", tr("Next button pressed"), "");

    if (playlist.size() > 0) {
        Track *track = playlist.first();
        playlist.removeFirst();

        actionPlay(track);
    }
}


void Waver::pauseButton()
{
    addToLog("waver", tr("Pause button pressed"), "");

    if (currentTrack != nullptr) {
        currentTrack->setStatus(Track::Paused);
        emit notify(PlaybackStatus);
    }
}


void Waver::peakCallback(double lPeak, double rPeak, qint64 delayMicroseconds, void *trackPointer)
{
    Track *track = currentTrack;
    if (crossfadeInProgress) {
        track = previousTrack;
    }

    if (track != trackPointer) {
        return;
    }
    if (track == nullptr) {
        emit uiSetPeakMeter(1, 1, QDateTime::currentMSecsSinceEpoch());
        return;
    }

    peakCallbackCount++;

    qint64 delayMilliseconds = delayMicroseconds / 1000 + (peakDelayOn ? peakDelayMilliseconds : 0);
    qint64 scheduledTime     = QDateTime::currentMSecsSinceEpoch() + delayMilliseconds;

    QTimer::singleShot(delayMilliseconds, this, [this, lPeak, rPeak, scheduledTime] () {
        emit uiSetPeakMeter(1 - lPeak, 1 - rPeak, scheduledTime);
    });

    if (peakCallbackCount % 10 == 0) {
        if (peakUILagCount > 0)  {
            peakLagCount++;

            peakFPSMutex.lock();
            if (peakFPS > 3) {
                peakFPS -= 2;
                emit uiSetPeakFPS(QString("%1FPS").arg(peakFPS));
            }
            peakFPSMutex.unlock();

            peakLagIgnoreEnd     = QDateTime::currentMSecsSinceEpoch() + 2500;
            peakFPSIncreaseStart = QDateTime::currentMSecsSinceEpoch() + qMin(peakLagCount * 100, static_cast<qint64>(30000));
            peakUILagCount       = 0;
        }
        else if ((peakFPS < peakFPSMax) && (QDateTime::currentMSecsSinceEpoch() >= peakFPSIncreaseStart)) {
            peakFPSMutex.lock();
            peakFPS++;
            peakFPSMutex.unlock();

            emit uiSetPeakFPS(QString("%1FPS").arg(peakFPS));
        }
        else if ((peakLagCount > 0) && (QDateTime::currentMSecsSinceEpoch() >= peakFPSIncreaseStart + 300000)) {
            peakLagCount = 0;
        }
    }
}


void Waver::peakUILag()
{
    if (QDateTime::currentMSecsSinceEpoch() > peakLagIgnoreEnd) {
        peakUILagCount++;
    }
}


void Waver::playButton()
{
    addToLog("waver", tr("Play button pressed"), "");

    if (currentTrack != nullptr) {
        currentTrack->setStatus(Track::Playing);
        emit notify(PlaybackStatus);
    }
}


int Waver::playlistFirstGroupLoad()
{
    playlistFirstGroupSongIds.clear();
    playlistFirstGroupTracks.clear();

    QSettings settings;

    if (!settings.contains("playlist_first_group/server_settings_id")) {
        return 0;
    }
    QUuid serverSettingsId(settings.value("playlist_first_group/server_settings_id").toString());

    AmpacheServer *groupServer = nullptr;
    foreach(AmpacheServer *server, servers) {
        if (server->getSettingsId() == serverSettingsId) {
            groupServer = server;
            break;
        }
    }
    if (groupServer == nullptr) {
        return 0;
    }

    int tracksSize = settings.beginReadArray("playlist_first_group/tracks");

    if (tracksSize > 0) {
        emit uiSetStatusText(tr("Networking"));
        addToLog("waver", tr("Playlist First Group loaded").append(" - %1").arg(tracksSize), "");
    }

    for (int i = 0; i < tracksSize; i++) {
        settings.setArrayIndex(i);

        QObject *opExtra = new QObject();
        opExtra->setProperty("group", settings.value("group"));

        playlistFirstGroupSongIds.append(settings.value("track_id").toString());

        groupServer->startOperation(AmpacheServer::Song, {{ "song_id", playlistFirstGroupSongIds.last() }}, opExtra);
        addToLog("waver", tr("Starting operation - Song"), "");
    }
    settings.endArray();

    return tracksSize;
}


void Waver::playlistFirstGroupSave()
{
    // local files never have group, no need to support them here

    QSettings settings;
    settings.remove("playlist_first_group");

    if (currentTrack == nullptr) {
        return;
    }
    if (!getCurrentTrackInfo().attributes.contains("group")) {
        return;
    }

    QStringList idParts  = getCurrentTrackInfo().id.split("|");
    QString     serverId = idParts.last();
    int         srvIndex = serverIndex(serverId);
    if ((srvIndex >= servers.size()) || (srvIndex < 0)) {
        errorMessage(serverId, tr("Server ID can not be found"), serverId);
        return;
    }

    QString groupId = getCurrentTrackInfo().attributes.value("group_id").toString();

    settings.setValue("playlist_first_group/server_settings_id", servers.at(srvIndex)->getSettingsId().toString());

    settings.beginWriteArray("playlist_first_group/tracks");

    settings.setArrayIndex(0);
    settings.setValue("track_id", idParts.at(0).mid(1));
    settings.setValue("group", getCurrentTrackInfo().attributes.value("group"));

    int i = 0;
    while ((i < playlist.count()) && playlist.at(i)->getTrackInfo().attributes.contains("group_id") && !groupId.compare(playlist.at(i)->getTrackInfo().attributes.value("group_id").toString())) {
        settings.setArrayIndex(i + 1);

        Track::TrackInfo trackInfo = playlist.at(i)->getTrackInfo();

        settings.setValue("track_id", trackInfo.id.split("|").at(0).mid(1));
        settings.setValue("group", trackInfo.attributes.value("group"));

        i++;
    }

    settings.endArray();
    settings.sync();
}


void Waver::playlistItemClicked(int index, int action)
{
    if (action == globalConstant("action_play")) {
        addToLog("waver", tr("Playlist item action: Play").append(" - %1").arg(index), "");

        Track *track = playlist.at(index);
        playlist.removeAt(index);
        actionPlay(track);
        return;
    }

    if (action == globalConstant("action_move_to_top")) {
        addToLog("waver", tr("Playlist item action: Move To Top").append(" - %1").arg(index), "");

        if (playlist.at(index)->getTrackInfo().attributes.contains("playlist_selected")) {
            int topIndex = 0;
            for (int i = 0; i < playlist.size(); i++) {
                if (playlist.at(i)->getTrackInfo().attributes.contains("playlist_selected")) {
                    Track *track = playlist.at(i);
                    playlist.removeAt(i);
                    playlist.insert(topIndex, track);
                    topIndex++;
                }
            }
        }
        else {
            Track *track = playlist.at(index);
            playlist.removeAt(index);
            playlist.insert(0, track);
        }
        playlistUpdateUISignals();
        playlistFirstGroupSave();
        return;
    }

    if (action == globalConstant("action_remove")) {
        addToLog("waver", tr("Playlist item action: Remove").append(" - %1").arg(index), "");

        if (playlist.at(index)->getTrackInfo().attributes.contains("playlist_selected")) {
            int i = 0;
            while (i < playlist.size()) {
                if (playlist.at(i)->getTrackInfo().attributes.contains("playlist_selected")) {
                    playlist.removeAt(i);
                }
                else {
                    i++;
                }
            }
        }
        else {
            playlist.removeAt(index);
        }
        playlistUpdateUISignals();
        playlistFirstGroupSave();
        return;
    }

    if (action == globalConstant("action_select")) {
        addToLog("waver", tr("Playlist item action: Select").append(" - %1").arg(index), "");

        if (playlist.at(index)->getTrackInfo().attributes.contains("playlist_selected")) {
            playlist.at(index)->attributeRemove("playlist_selected");
            emit playlistSelected(index, false);
        }
        else {
            playlist.at(index)->attributeAdd("playlist_selected", "playlist_selected");
            emit playlistSelected(index, true);
        }
    }

    if (action == globalConstant("action_select_group")) {
        addToLog("waver", tr("Playlist item action: Select Group").append(" - %1").arg(index), "");

        if (!playlist.at(index)->getTrackInfo().attributes.contains("group_id")) {
            return;
        }

        QString groupId = playlist.at(index)->getTrackInfo().attributes.value("group_id").toString();
        for (int i = 0; i < playlist.size(); i++) {
            if (playlist.at(i)->getTrackInfo().attributes.contains("group_id") && (playlist.at(i)->getTrackInfo().attributes.value("group_id").toString().compare(groupId) == 0)) {
                playlist.at(i)->attributeAdd("playlist_selected", "playlist_selected");
                emit playlistSelected(i, true);
            }
        }
    }

    if ((action == globalConstant("action_select_all")) || (action == globalConstant("action_deselect_all"))) {
        addToLog("waver", tr("Playlist item action: Select All or Deselect All").append(" - %1").arg(index), "");

        for (int i = 0; i < playlist.size(); i++) {
            if (action == globalConstant("action_select_all")) {
                playlist.at(i)->attributeAdd("playlist_selected", "playlist_selected");
            }
            else {
                playlist.at(i)->attributeRemove("playlist_selected");
            }
            emit playlistSelected(i, action == globalConstant("action_select_all"));
        }
    }
}


void Waver::playlistItemDragDropped(int index, int destinationIndex)
{
    addToLog("waver", tr("Playlist item moved").append(" - %1 to %2").arg(index).arg(destinationIndex), "");

    Track *track = playlist.at(index);
    playlist.removeAt(index);
    playlist.insert(destinationIndex, track);
    playlistUpdateUISignals();
    playlistFirstGroupSave();
}


void Waver::playlistUpdateUISignals()
{
    qint64 totalMilliSeconds = 0;
    bool   totalIsEstimate   = false;

    emit playlistBigBusy(false);

    emit playlistClearItems();
    foreach (Track *track, playlist) {
        Track::TrackInfo trackInfo = track->getTrackInfo();
        emit playlistAddItem(trackInfo.title, trackInfo.artist, trackInfo.attributes.contains("group") ? trackInfo.attributes.value("group") : "", trackInfo.arts.first().toString(), trackInfo.attributes.contains("playlist_selected"));

        qint64 milliseconds = track->getLengthMilliseconds();
        if (milliseconds > 0) {
            totalMilliSeconds += milliseconds;
        }
        else {
            totalIsEstimate = true;
        }
    }

    if (totalMilliSeconds <= 0) {
        emit playlistTotalTime("");
    }
    else {
        emit playlistTotalTime(QDateTime::fromMSecsSinceEpoch(totalMilliSeconds).toUTC().toString("hh:mm:ss").replace(QRegExp("^00:"), "").prepend(totalIsEstimate ? "~" : ""));
    }
}


void Waver::positioned(double percent)
{
    addToLog("waver", tr("Positioner used").append(" - %1").arg(percent), "");

    if (currentTrack == nullptr) {
        return;
    }
    currentTrack->setPosition(percent);
}


void Waver::previousButton(int index)
{
    addToLog("waver", tr("Previous button pressed"), "");

    if (history.count() <= index) {
        return;
    }

    actionPlay(history.at(index));
}


void Waver::raiseButton()
{
    addToLog("waver", tr("Raise button pressed"), "");

    emit uiRaise();
}


void Waver::requestLog()
{
    QString logStr;
    foreach (LogItem item, log) {
        logStr.append(QString("%1: <%2>, '%3', '%4'\n").arg(item.dateTime.toString(), item.id, item.error, item.info));
    }

    emit logAsRequested(logStr);
}


void Waver::requestOptions()
{
    QVariantMap     optionsObj;
    QSettings       settings;

    if (currentTrack == nullptr) {
        optionsObj.insert("eq_disable", 1);
    }
    else {
        optionsObj.insert("eq_disable", 0);
        optionsObj.insert("eq_on", settings.value("eq/on", DEFAULT_EQON).toBool());

        QVector<double> eqCenterFrequencies = currentTrack->getEqualizerBandCenterFrequencies();

        optionsObj.insert("pre_amp", settings.value("eq/pre_amp", DEFAULT_PREAMP));
        optionsObj.insert("eq1Label", formatFrequencyValue(eqCenterFrequencies.at(0)));
        optionsObj.insert("eq1", settings.value("eq/eq1", DEFAULT_EQ1));
        optionsObj.insert("eq2Label", formatFrequencyValue(eqCenterFrequencies.at(1)));
        optionsObj.insert("eq2", settings.value("eq/eq2", DEFAULT_EQ2));
        optionsObj.insert("eq3Label", formatFrequencyValue(eqCenterFrequencies.at(2)));
        optionsObj.insert("eq3", settings.value("eq/eq3", DEFAULT_EQ3));
        optionsObj.insert("eq4Label", formatFrequencyValue(eqCenterFrequencies.at(3)));
        optionsObj.insert("eq4", settings.value("eq/eq4", DEFAULT_EQ4));
        optionsObj.insert("eq5Label", formatFrequencyValue(eqCenterFrequencies.at(4)));
        optionsObj.insert("eq5", settings.value("eq/eq5", DEFAULT_EQ5));
        optionsObj.insert("eq6Label", formatFrequencyValue(eqCenterFrequencies.at(5)));
        optionsObj.insert("eq6", settings.value("eq/eq6", DEFAULT_EQ6));
        optionsObj.insert("eq7Label", formatFrequencyValue(eqCenterFrequencies.at(6)));
        optionsObj.insert("eq7", settings.value("eq/eq7", DEFAULT_EQ7));
        optionsObj.insert("eq8Label", formatFrequencyValue(eqCenterFrequencies.at(7)));
        optionsObj.insert("eq8", settings.value("eq/eq8", DEFAULT_EQ8));
        optionsObj.insert("eq9Label", formatFrequencyValue(eqCenterFrequencies.at(8)));
        optionsObj.insert("eq9", settings.value("eq/eq9", DEFAULT_EQ9));
        optionsObj.insert("eq10Label", formatFrequencyValue(eqCenterFrequencies.at(9)));
        optionsObj.insert("eq10", settings.value("eq/eq10", DEFAULT_EQ10));
    }

    optionsObj.insert("shuffle_autostart", settings.value("options/shuffle_autostart", DEFAULT_SHUFFLE_AUTOSTART).toBool());
    optionsObj.insert("shuffle_delay_seconds", settings.value("options/shuffle_delay_seconds", DEFAULT_SHUFFLE_DELAY_SECONDS));
    optionsObj.insert("shuffle_count", settings.value("options/shuffle_count", DEFAULT_SHUFFLE_COUNT));
    optionsObj.insert("random_lists_count", settings.value("options/random_lists_count", DEFAULT_RANDOM_LISTS_COUNT));
    optionsObj.insert("shuffle_favorite_frequency", settings.value("options/shuffle_favorite_frequency", DEFAULT_SHUFFLE_FAVORITE_FREQUENCY));
    optionsObj.insert("shuffle_operator", settings.value("options/shuffle_operator", DEFAULT_SHUFFLE_OPERATOR));

    optionsObj.insert("hide_dot_playlists", settings.value("options/hide_dot_playlists", DEFAULT_HIDE_DOT_PLAYLIST).toBool());
    optionsObj.insert("fade_tags", settings.value("options/fade_tags", DEFAULT_FADE_TAGS));
    optionsObj.insert("crossfade_tags", settings.value("options/crossfade_tags", DEFAULT_CROSSFADE_TAGS));

    optionsObj.insert("max_peak_fps", settings.value("options/max_peak_fps", DEFAULT_MAX_PEAK_FPS));
    optionsObj.insert("peak_delay_on", settings.value("options/peak_delay_on", DEFAULT_PEAK_DELAY_ON).toBool());
    optionsObj.insert("peak_delay_ms", settings.value("options/peak_delay_ms", DEFAULT_PEAK_DELAY_MS));

    emit optionsAsRequested(optionsObj);
}


void Waver::run()
{
    addToLog("waver", "Starting", "Entering run method");

    QSettings settings;

    shuffleCountdownTimer = new QTimer();
    shuffleCountdownTimer->setInterval(1000);
    connect(shuffleCountdownTimer, &QTimer::timeout, this, &Waver::shuffleCountdown);


    // local dirs

    int localDirsSize = settings.beginReadArray("localDirs");
    for (int i = 0; i < localDirsSize; i++) {
        settings.setArrayIndex(i);
        localDirs.append(settings.value("dir").toString());
    }
    settings.endArray();

    if (!localDirs.size()) {
        localDirs.append(QStandardPaths::standardLocations(QStandardPaths::MusicLocation));
    }

    localDirs.sort(Qt::CaseInsensitive);

    for(int i = 0; i < localDirs.size(); i++) {
        QString id   = QString("%1%2").arg(UI_ID_PREFIX_LOCALDIR).arg(i);
        QString path = QFileInfo(localDirs.at(i)).absoluteFilePath();
        emit explorerAddItem(id, QVariant::fromValue(nullptr), QFileInfo(localDirs.at(i)).baseName(), "qrc:/icons/local.ico", QVariantMap({{ "path", path }}), true, false, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
        addToLog(id, tr("Folder added"), path);
    }


    // servers

    int serversSize = settings.beginReadArray("servers");
    for (int i = 0; i < serversSize; i++) {
        settings.setArrayIndex(i);
        servers.append(new AmpacheServer(QUrl(settings.value("urlString").toString()), settings.value("userName").toString()));
        servers.last()->setSettingsId(QUuid(settings.value("settingsId").toString()));
    }
    settings.endArray();

    std::sort(servers.begin(), servers.end(), [](AmpacheServer *a, AmpacheServer *b) {
        return a->formattedName().compare(b->formattedName(), Qt::CaseInsensitive) < 0;
    });

    for(int i = 0; i < servers.size(); i++) {
        QString id = QString("%1%2").arg(UI_ID_PREFIX_SERVER).arg(i);

        servers.at(i)->setId(id);
        servers.at(i)->retreivePassword();

        connect(servers.at(i), &AmpacheServer::operationFinished,           this,          &Waver::serverOperationFinished);
        connect(servers.at(i), &AmpacheServer::errorMessage,                this,          &Waver::errorMessage);

        emit explorerAddItem(id, QVariant::fromValue(nullptr), servers.at(i)->formattedName(), "qrc:/icons/remote.ico", QVariant::fromValue(nullptr), true, false, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));

        addToLog(id, tr("Server added"), servers.at(i)->formattedName());
    }

    if (servers.count()) {
        startShuffleCountdown();
    }
}


int Waver::serverIndex(QString id)
{
    for(int i = 0; i < servers.size(); i++) {
        if (servers.at(i)->getId().compare(id) == 0) {
            return i;
        }
    }

    return -1;
}


void Waver::serverOperationFinished(AmpacheServer::OpCode opCode, AmpacheServer::OpData opData, AmpacheServer::OpResults opResults)
{
    stopShuffleCountdown();

    int srvIndex = serverIndex(opData.value("serverId"));
    if ((srvIndex >= servers.size()) || (srvIndex < 0)) {
        return;
    }

    QSettings settings;
    QString   cacheKey;
    QString   parentId;

    QString originalAction = opData.contains("original_action") ? opData.value("original_action") : "";
    QString group          = opData.contains("group")           ? opData.value("group")           : "";
    QUuid   groupId        = QUuid::createUuid();

    if (opResults.count() <= 0) {
        errorMessage(servers.at(srvIndex)->getId(), tr("The Ampache server returned an empty result set"), tr("No error occured, but the results are empty"));
    }

    if (opCode == AmpacheServer::Search) {
        addToLog("waver", tr("Operation Finished - Search"), "");

        parentId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_SEARCH, QString(opData.value("serverId")).remove(0, 1), opData.value("serverId"));

        std::sort(opResults.begin(), opResults.end(), [](AmpacheServer::OpData a, AmpacheServer::OpData b) {
            return a.value("name").compare(b.value("name"), Qt::CaseInsensitive) < 0;
        });
    }
    else if (opCode == AmpacheServer::BrowseRoot) {
        addToLog("waver", tr("Operation Finished - Browse Root"), "");

        cacheKey = QString("%1/browse").arg(servers.at(srvIndex)->getSettingsId().toString());

        settings.remove(cacheKey);
        settings.beginWriteArray(cacheKey);
    }
    else if (opCode == AmpacheServer::BrowseArtist) {
        addToLog("waver", tr("Operation Finished - Browse Artist"), "");

        parentId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_BROWSEARTIST, opData.value("artist"), opData.value("serverId"));

        std::sort(opResults.begin(), opResults.end(), [](AmpacheServer::OpData a, AmpacheServer::OpData b) {
            return (a.value("year").toInt() < b.value("year").toInt()) || ((a.value("year").toInt() == b.value("year").toInt()) && (a.value("name").compare(b.value("name"), Qt::CaseInsensitive) < 0));
        });
    }
    else if (opCode == AmpacheServer::BrowseAlbum) {
        addToLog("waver", tr("Operation Finished - Browse Album"), "");

        parentId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_BROWSEALBUM, opData.value("album"), opData.value("serverId"));

        std::sort(opResults.begin(), opResults.end(), [](AmpacheServer::OpData a, AmpacheServer::OpData b) {
            return (a.value("track").toInt() < b.value("track").toInt()) || ((a.value("track").toInt() == b.value("track").toInt()) && (a.value("title").compare(b.value("title"), Qt::CaseInsensitive) < 0));
        });
    }
    else if (opCode == AmpacheServer::CountFlagged) {
        addToLog("waver", tr("Operation Finished - Count Flagged"), "");
    }
    else if (opCode == AmpacheServer::PlaylistRoot) {
        addToLog("waver", tr("Operation Finished - Playlist Root"), "");

        cacheKey = QString("%1/playlists").arg(servers.at(srvIndex)->getSettingsId().toString());

        settings.remove(cacheKey);
        settings.beginWriteArray(cacheKey);
    }
    else if (opCode == AmpacheServer::PlaylistSongs) {
        addToLog("waver", tr("Operation Finished - Playlist Songs"), "");

        parentId = QString("%1%2|%3").arg(opData.value("playlist").startsWith("smart", Qt::CaseInsensitive) ? UI_ID_PREFIX_SERVER_SMARTPLAYLIST : UI_ID_PREFIX_SERVER_PLAYLIST, opData.value("playlist"), opData.value("serverId"));
    }
    else if (opCode == AmpacheServer::RadioStations) {
        addToLog("waver", tr("Operation Finished - Radio Stations"), "");

        cacheKey = QString("%1/radiostations").arg(servers.at(srvIndex)->getSettingsId().toString());

        settings.remove(cacheKey);
        settings.beginWriteArray(cacheKey);
    }
    else if (opCode == AmpacheServer::SetFlag) {
        addToLog("waver", tr("Operation Finished - Set Flag"), "");
    }
    else if (opCode == AmpacheServer::Shuffle) {
        addToLog("waver", tr("Operation Finished - Shuffle"), "");

        parentId = QString("%1|%2").arg(QString(opData.value("serverId")).replace(0, 1, UI_ID_PREFIX_SERVER_SHUFFLE), opData.value("serverId"));
    }
    else if (opCode == AmpacheServer::Song) {
        addToLog("waver", tr("Operation Finished - Song"), "");
    }
    else if (opCode == AmpacheServer::Tags) {
        addToLog("waver", tr("Operation Finished - Tags"), "");

        cacheKey = QString("%1/tags").arg(servers.at(srvIndex)->getSettingsId().toString());

        settings.remove(cacheKey);
        settings.beginWriteArray(cacheKey);
    }

    for(int i = 0; i < opResults.size(); i++) {
        AmpacheServer::OpData result = opResults.at(i);

        if (opCode == AmpacheServer::Search) {
            QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_SEARCHRESULT, result.value("id"), opData.value("serverId"));

            QVariantMap trackInfoMap;
            QStringList keys = result.keys();
            foreach(QString key, keys) {
                trackInfoMap.insert(key, result.value(key));
            }

            emit explorerAddItem(newId, parentId, result.value("name"), result.value("art"), trackInfoMap, false, true, false, false);
        }
        else if (opCode == AmpacheServer::BrowseRoot) {
           settings.setArrayIndex(i);
           settings.setValue("id", result.value("id"));
           settings.setValue("name", result.value("name"));
           settings.setValue("art", result.value("art"));
        }
        else if (opCode == AmpacheServer::BrowseArtist) {
            QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_BROWSEALBUM, result.value("id"), opData.value("serverId"));

            QString displayName = result.value("name");
            if (result.value("year").toInt()) {
                displayName = displayName.prepend("%1. ").arg(result.value("year"));
            }

            emit explorerAddItem(newId, parentId, displayName, result.value("art"), QVariantMap({{ "art", result.value("art") }, { "group", result.value("name") }}), true, true, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
        }
        else if (opCode == AmpacheServer::BrowseAlbum) {
            QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_BROWSESONG, result.value("id"), opData.value("serverId"));

            QVariantMap trackInfoMap;
            QStringList keys = result.keys();
            foreach(QString key, keys) {
                trackInfoMap.insert(key, result.value(key));
            }

            if ((originalAction.compare("action_play") == 0) || (originalAction.compare("action_playnext") == 0) || (originalAction.compare("action_enqueue") == 0)) {
                Track::TrackInfo trackInfo = trackInfoFromIdExtra(newId, trackInfoMap);
                trackInfo.attributes.insert("group_id", groupId.toString());
                trackInfo.attributes.insert("group", QString("%1 %2.").arg(group).arg(i + 1));

                if ((i == 0) && (originalAction.compare("action_play") == 0)) {
                    playlist.clear();
                    actionPlay(trackInfo);
                }
                else {
                    Track *track = new Track(trackInfo, peakCallbackInfo);
                    connectTrackSignals(track);

                    if (originalAction.compare("action_enqueue") == 0) {
                        playlist.append(track);
                    }
                    else {
                        playlist.insert(originalAction.compare("action_play") == 0 ? i - 1 : i, track);
                    }
                }
            }
            else {
                QString displayTitle = result.value("title");
                if (result.value("track").toInt()) {
                    displayTitle = displayTitle.prepend("%1. ").arg(result.value("track"));
                }

                emit explorerAddItem(newId, parentId, displayTitle, "qrc:/icons/audio_file.ico", trackInfoMap, false, true, QVariant::fromValue(nullptr), QVariant::fromValue(nullptr));
            }
        }
        else if ((opCode == AmpacheServer::PlaylistRoot) || (opCode == AmpacheServer::Tags)) {
            settings.setArrayIndex(i);
            settings.setValue("id", result.value("id"));
            settings.setValue("name", result.value("name"));
        }
        else if ((opCode == AmpacheServer::PlaylistSongs) || (opCode == AmpacheServer::Shuffle)) {
            QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_PLAYLIST_ITEM, result.value("id"), opData.value("serverId"));

            QVariantMap trackInfoMap;
            QStringList keys = result.keys();
            foreach(QString key, keys) {
                trackInfoMap.insert(key, result.value(key));
            }

            if ((originalAction.compare("action_play") == 0) || (originalAction.compare("action_playnext") == 0) || (originalAction.compare("action_enqueue") == 0)) {
                Track::TrackInfo trackInfo = trackInfoFromIdExtra(newId, trackInfoMap);

                if (opCode == AmpacheServer::PlaylistSongs) {
                    trackInfo.attributes.insert("group_id", groupId.toString());
                    trackInfo.attributes.insert("group", QString("%1 %2.").arg(group).arg(i + 1));
                }

                if ((i == 0) && (originalAction.compare("action_play") == 0)) {
                    playlist.clear();
                    actionPlay(trackInfo);
                }
                else {
                    Track *track = new Track(trackInfo, peakCallbackInfo);
                    connectTrackSignals(track);

                    if (originalAction.compare("action_enqueue") == 0) {
                        playlist.append(track);
                    }
                    else {
                        playlist.insert(originalAction.compare("action_play") == 0 ? i - 1 : i, track);
                    }
                }
            }
        }
        else if (opCode == AmpacheServer::RadioStations) {
            settings.setArrayIndex(i);
            settings.setValue("id", result.value("id"));
            settings.setValue("url", result.value("url"));
            settings.setValue("name", result.value("name"));
        }
        else if (opCode == AmpacheServer::Song) {
            QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_PLAYLIST_ITEM, result.value("id"), opData.value("serverId"));

            QVariantMap trackInfoMap;
            QStringList keys = result.keys();
            foreach(QString key, keys) {
                trackInfoMap.insert(key, result.value(key));
            }

            Track::TrackInfo trackInfo = trackInfoFromIdExtra(newId, trackInfoMap);

            if (opData.contains("session_expired")) {
                if (opData.value("session_expired").compare("current_track") == 0) {
                    if (currentTrack != nullptr) {
                        connectTrackSignals(currentTrack, false);
                        currentTrack->setStatus(Track::Paused);
                        currentTrack->setStatus(Track::Idle);
                        delete currentTrack;
                        currentTrack = nullptr;
                    }
                    actionPlay(trackInfo);
                }
                else {
                    QList<Track*> toBeDeleted;

                    for (int i = 0; i < playlist.size(); i++) {
                        if (playlist.at(i)->getTrackInfo().id.compare(newId) == 0) {
                            toBeDeleted.append(playlist.at(i));

                            if (opData.contains("group")) {
                                trackInfo.attributes.insert("group_id", "sessionExpiredRestored");
                                trackInfo.attributes.insert("group", opData.value("group"));
                                trackInfo.attributes.insert("playlist_index", i);
                            }

                            Track *track = new Track(trackInfo, peakCallbackInfo);
                            connectTrackSignals(track);
                            playlist.replace(i, track);
                        }
                    }
                    playlistUpdateUISignals();
                    playlistFirstGroupSave();

                    foreach (Track *track, toBeDeleted) {
                        connectTrackSignals(track, false);
                        track->setStatus(Track::Idle);
                        delete track;
                    }
                }
            }
            else {
                int playlistIndex = playlistFirstGroupSongIds.indexOf(result.value("id"));

                trackInfo.attributes.insert("group_id", "playlistFirstGroup");
                trackInfo.attributes.insert("group", group);
                trackInfo.attributes.insert("playlist_index", playlistIndex);

                playlistFirstGroupTracks.append(trackInfo);
                playlistFirstGroupSongIds.replace(playlistIndex, "");
            }
        }
    }

    if (opCode == AmpacheServer::Search) {
        explorerNetworkingUISignals(parentId, false);
    }
    else if (opCode == AmpacheServer::BrowseRoot) {
        settings.endArray();
        settings.sync();

        QString browseId = QString("%1|%2").arg(QString(opData.value("serverId")).replace(0, 1, UI_ID_PREFIX_SERVER_BROWSE), opData.value("serverId"));
        itemActionServerItem(browseId, globalConstant("action_expand").toInt(), QVariantMap());
        explorerNetworkingUISignals(browseId, false);
    }
    else if (opCode == AmpacheServer::BrowseArtist) {
        explorerNetworkingUISignals(parentId, false);
    }
    else if (opCode == AmpacheServer::BrowseAlbum) {
        explorerNetworkingUISignals(parentId, false);

        if ((originalAction.compare("action_play") == 0) || (originalAction.compare("action_playnext") == 0) || (originalAction.compare("action_enqueue") == 0)) {
            playlistUpdateUISignals();
            playlistFirstGroupSave();
        }
    }
    else if (opCode == AmpacheServer::PlaylistRoot) {
        settings.endArray();
        settings.sync();

        QString playlistId = QString("%1|%2").arg(QString(opData.value("serverId")).replace(0, 1, UI_ID_PREFIX_SERVER_PLAYLISTS), opData.value("serverId"));
        itemActionServerItem(playlistId, globalConstant("action_expand").toInt(), QVariantMap());
        explorerNetworkingUISignals(playlistId, false);

        playlistId = QString("%1|%2").arg(QString(opData.value("serverId")).replace(0, 1, UI_ID_PREFIX_SERVER_SMARTPLAYLISTS), opData.value("serverId"));
        itemActionServerItem(playlistId, globalConstant("action_expand").toInt(), QVariantMap());
        explorerNetworkingUISignals(playlistId, false);
    }
    else if ((opCode == AmpacheServer::PlaylistSongs) || (opCode == AmpacheServer::Shuffle)) {
        explorerNetworkingUISignals(parentId, false);

        if ((originalAction.compare("action_play") == 0) || (originalAction.compare("action_playnext") == 0) || (originalAction.compare("action_enqueue") == 0)) {
            playlistUpdateUISignals();
            playlistFirstGroupSave();
        }
    }
    else if (opCode == AmpacheServer::RadioStations) {
        settings.endArray();
        settings.sync();

        QString radiosId = QString("%1|%2").arg(QString(opData.value("serverId")).replace(0, 1, UI_ID_PREFIX_SERVER_RADIOSTATIONS), opData.value("serverId"));
        itemActionServerItem(radiosId, globalConstant("action_expand").toInt(), QVariantMap());
        explorerNetworkingUISignals(radiosId, false);
    }
    else if (opCode == AmpacheServer::Tags) {
        settings.endArray();
        settings.sync();

        QString shuffleId = QString("%1|%2").arg(QString(opData.value("serverId")).replace(0, 1, UI_ID_PREFIX_SERVER_SHUFFLE), opData.value("serverId"));
        itemActionServerItem(shuffleId, globalConstant("action_expand").toInt(), QVariantMap());
        explorerNetworkingUISignals(shuffleId, false);
    }
    else if (opCode == AmpacheServer::Song) {
        if (!opData.contains("session_expired")) {
            bool finished = true;
            foreach(QString songId, playlistFirstGroupSongIds) {
                if (!songId.isEmpty()) {
                    finished = false;
                    break;
                }
            }

            if (finished) {
                std::sort(playlistFirstGroupTracks.begin(), playlistFirstGroupTracks.end(), [](Track::TrackInfo a, Track::TrackInfo b) {
                    return a.attributes.value("playlist_index").toInt() < b.attributes.value("playlist_index").toInt();
                });

                for(int i = 0; i < playlistFirstGroupTracks.count(); i++) {
                    Track::TrackInfo trackInfo = playlistFirstGroupTracks.at(i);

                    if (i == 0) {
                        playlist.clear();
                        actionPlay(trackInfo);
                    }
                    else {
                        Track *track = new Track(trackInfo, peakCallbackInfo);
                        connectTrackSignals(track);
                        playlist.append(track);
                    }
                }

                playlistUpdateUISignals();
                playlistFirstGroupSave();
            }
        }
    }
}


void Waver::shuffleCountdown()
{
    QSettings settings;

    shuffleCountdownPercent -= 1.0 / settings.value("options/shuffle_delay_seconds", DEFAULT_SHUFFLE_DELAY_SECONDS).toDouble();

    if (shuffleCountdownPercent <= 0) {
        stopShuffleCountdown();

        if (shuffleFirstAfterStart) {
            shuffleFirstAfterStart = false;
            if (playlistFirstGroupLoad()) {
                return;
            }
        }
        startShuffleBatch();
        return;
    }

    emit uiSetShuffleCountdown(shuffleCountdownPercent);
}


void Waver::shutdown()
{
    stopByShutdown = true;
    stopButton();

    shutdownMutex.lock();
    shutdownCompleted = true;
    shutdownMutex.unlock();
}


void Waver::startNextTrack()
{
    stopShuffleCountdown();

    if (currentTrack != nullptr) {
        return;
    }
    if (playlist.size() < 1) {
        startShuffleCountdown();
        return;
    }

    currentTrack = playlist.first();
    playlist.removeFirst();

    currentTrack->setStatus(Track::Playing);

    if (isCrossfade(previousTrack, currentTrack)) {
        crossfadeInProgress = true;
        QTimer::singleShot(currentTrack->getFadeDurationSeconds() * 1000 / 2, this, &Waver::startNextTrackUISignals);
    }
    else {
        startNextTrackUISignals();
    }

    addToLog(currentTrack->getTrackInfo().id, currentTrack->getTrackInfo().title, tr("Track started"));
}


void Waver::startNextTrackUISignals()
{
    if (currentTrack == nullptr) {
        return;
    }

    crossfadeInProgress = false;

    Track::TrackInfo trackInfo = getCurrentTrackInfo();

    emit uiSetTrackData(trackInfo.title, trackInfo.artist, trackInfo.album, trackInfo.track, trackInfo.year);
    emit uiSetTrackLength(QDateTime::fromMSecsSinceEpoch(currentTrack->getLengthMilliseconds()).toUTC().toString("hh:mm:ss"));
    emit uiSetTrackTags(trackInfo.tags.join(", "));
    emit uiSetImage(trackInfo.arts.size() ? trackInfo.arts.at(0).toString() : "qrc:/images/waver.png");
    emit uiSetFavorite(trackInfo.attributes.contains("flag"));
    emit uiSetTrackBusy(currentTrack->getNetworkStartingLastState());

    emit requestTrackBufferReplayGainInfo();

    playlistUpdateUISignals();
    playlistFirstGroupSave();

    emit notify(All);
}


void Waver::startShuffleBatch(int srvIndex, int artistId, ShuffleMode mode, QString originalAction)
{
    if (srvIndex < 0) {
        srvIndex = shuffleServerIndex;

        shuffleServerIndex++;
        if (shuffleServerIndex >= servers.count()) {
            shuffleServerIndex = 0;
        }
    }

    if (srvIndex >= servers.size()) {
        return;
    }

    explorerNetworkingUISignals(QString("%1%2|%3%4").arg(UI_ID_PREFIX_SERVER_SHUFFLE).arg(srvIndex).arg(UI_ID_PREFIX_SERVER).arg(srvIndex), true);
    emit playlistBigBusy(true);

    QSettings             settings;
    AmpacheServer::OpData opData;

    if (artistId > 0) {
        opData.insert("artist", QString("%1").arg(artistId));
    }
    else if (mode == Favorite) {
        opData.insert("favorites", "favorites");
    }
    else if (mode == NeverPlayed) {
        opData.insert("never_played", "never_played");
    }

    if (!QStringList({"action_play", "action_playnext", "action_enqueue"}).contains(originalAction)) {
        originalAction = "action_play";
    }

    QObject *opExtra = new QObject();
    opExtra->setProperty("original_action", originalAction);

    servers.at(srvIndex)->startOperation(AmpacheServer::Shuffle, opData, opExtra);
    addToLog("waver", tr("Starting operation - Shuffle"), "");
}


void Waver::startShuffleCountdown()
{
    clearTrackUISignals();

    QSettings settings;
    if (!settings.value("options/shuffle_autostart", DEFAULT_SHUFFLE_AUTOSTART).toBool()) {
        return;
    }
    if (settings.value("options/shuffle_delay_seconds", DEFAULT_SHUFFLE_DELAY_SECONDS).toDouble() <= 0) {
        startShuffleBatch();
        return;
    }

    shuffleCountdownPercent = 1.0;
    emit uiSetShuffleCountdown(shuffleCountdownPercent);
    shuffleCountdownTimer->start(1000);

    addToLog("waver", tr("Shuffle countdown started"), "");
}


void Waver::stopButton()
{
    addToLog("waver", tr("Stop button pressed"), "");

    stopShuffleCountdown();

    killPreviousTrack();
    if (currentTrack != nullptr) {
        connectTrackSignals(currentTrack, false);
        currentTrack->setStatus(Track::Paused);
        currentTrack->setStatus(Track::Idle);
        delete currentTrack;
        currentTrack = nullptr;
    }

    foreach (Track *track, playlist) {
        connectTrackSignals(track, false);
        track->setStatus(Track::Paused);
        track->setStatus(Track::Idle);
        delete track;
    }
    playlist.clear();

    clearTrackUISignals();
    playlistUpdateUISignals();

    if (!stopByShutdown) {
        playlistFirstGroupSave();
    }
}


void Waver::stopShuffleCountdown()
{
    shuffleCountdownTimer->stop();
    emit uiSetShuffleCountdown(0);
}


void Waver::trackBufferInfo(QString id, bool rawIsFile, unsigned long rawSize, bool pmcIsFile, unsigned long pmcSize)
{
    QString memoryUsageText = QString("%1 <i>%2</i> / %3 <i>%4</i>").arg(formatMemoryValue(rawSize)).arg(rawIsFile ? 'F' : 'N').arg(formatMemoryValue(pmcSize)).arg(pmcIsFile ? 'F' : 'M');

    if ((currentTrack != nullptr) && (getCurrentTrackInfo().id.compare(id) == 0)) {
        emit uiSetTrackBufferData(memoryUsageText);
        return;
    }

    for (int i = 0; i < playlist.size(); i++) {
        if (id.compare(playlist.at(i)->getTrackInfo().id) == 0) {
            emit playlistBufferData(i, memoryUsageText);
            return;
        }
    }
}

void Waver::trackDecoded(QString id, qint64 length)
{
    QString logString = tr("Decoding finished, PCM %1 ms").arg(length);

    if ((previousTrack != nullptr) && (id.compare(previousTrack->getTrackInfo().id) == 0)) {
        addToLog(id, previousTrack->getTrackInfo().title, logString);
    }

    if ((currentTrack != nullptr) && (getCurrentTrackInfo().id.compare(id) == 0)) {
        addToLog(id,getCurrentTrackInfo().title, logString);
    }

    foreach (Track *track, playlist) {
        if (id.compare(track->getTrackInfo().id) == 0) {
            addToLog(id, track->getTrackInfo().title, logString);
        }
    }
}


void Waver::trackNetworkConnecting(QString id, bool busy)
{
    QString statusText = busy ? tr("Networking") : tr("Stopped");

    if (!busy && (currentTrack != nullptr)) {
        statusText = currentTrack->getStatusText();
    }

    emit uiSetStatusText(statusText);

    if ((currentTrack != nullptr) && (getCurrentTrackInfo().id.compare(id) == 0)) {
        emit uiSetTrackBusy(busy);
        addToLog(id, getCurrentTrackInfo().title, busy ? tr("Networking - Busy") : tr("Networking - Not busy"));
        return;
    }

    for (int i = 0; i < playlist.size(); i++) {
        if (id.compare(playlist.at(i)->getTrackInfo().id) == 0) {
            emit playlistBusy(i, busy);
            addToLog(id, playlist.at(i)->getTrackInfo().title, busy ? tr("Networking - Busy") : tr("Networking - Not busy"));
            return;
        }
    }
}


void Waver::trackError(QString id, QString info, QString error)
{
    errorMessage(id, info, error);
}


void Waver::trackFadeoutStarted(QString id)
{
    if ((currentTrack == nullptr) || id.compare(getCurrentTrackInfo().id)) {
        return;
    }
    if (playlist.size() < 1) {
        return;
    }

    addToLog(id, getCurrentTrackInfo().title, tr("Fadeout Started"));

    if (isCrossfade(currentTrack, playlist.first())) {
        killPreviousTrack();

        previousTrack = currentTrack;
        currentTrack  = nullptr;
        startNextTrack();
    }
}


void Waver::trackFinished(QString id)
{
    bool shuffleOK = true;

    if ((previousTrack != nullptr) && (id.compare(previousTrack->getTrackInfo().id) == 0)) {
        addToLog(id, previousTrack->getTrackInfo().title, tr("Track Finished"));

        connectTrackSignals(previousTrack, false);

        history.prepend(previousTrack->getTrackInfo());
        emit uiHistoryAdd(previousTrack->getTrackInfo().attributes.contains("radio_station") ? previousTrack->getTrackInfo().artist : previousTrack->getTrackInfo().title);

        previousTrack->setStatus(Track::Paused);
        previousTrack->setStatus(Track::Idle);
        delete previousTrack;
        previousTrack = nullptr;

        shuffleOK = false;
        startNextTrack();
    }

    if ((currentTrack != nullptr) && (id.compare(getCurrentTrackInfo().id) == 0)) {
        addToLog(id, getCurrentTrackInfo().title, tr("Track Finished"));

        if (currentTrack->getPlayedMillseconds() < 1000) {
            errorMessage(id, tr("Unable to start"), getCurrentTrackInfo().url.toString());
        }

        connectTrackSignals(currentTrack, false);

        history.prepend(getCurrentTrackInfo());
        emit uiHistoryAdd(getCurrentTrackInfo().attributes.contains("radio_station") ? getCurrentTrackInfo().artist : getCurrentTrackInfo().title);

        currentTrack->setStatus(Track::Paused);
        currentTrack->setStatus(Track::Idle);
        delete currentTrack;
        currentTrack = nullptr;

        shuffleOK = false;
        startNextTrack();
    }

    QList<Track *> tracksToBeDeleted;
    foreach (Track *track, playlist) {
        if (id.compare(track->getTrackInfo().id) == 0) {
            addToLog(id, track->getTrackInfo().title, tr("Track Finished"));
            tracksToBeDeleted.append(track);
        }
    }
    foreach (Track *track, tracksToBeDeleted) {
        connectTrackSignals(track, false);

        errorMessage(id, tr("Unable to start"), track->getTrackInfo().url.toString());

        playlist.removeAll(track);
        track->setStatus(Track::Paused);
        track->setStatus(Track::Idle);
        delete track;
    }

    playlistUpdateUISignals();
    playlistFirstGroupSave();

    if (shuffleOK && (playlist.size() < 1) && (servers.size() > 0)) {
        startShuffleCountdown();
    }
}


void Waver::trackInfo(QString id, QString info)
{
    errorMessage(id, info, "");
}


Track::TrackInfo Waver::trackInfoFromFilePath(QString filePath)
{
    Track::TrackInfo trackInfo;

    trackInfo.id    = QString("%1%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(QFileInfo(filePath).baseName());
    trackInfo.url   = QUrl::fromLocalFile(filePath);
    trackInfo.track = 0;
    trackInfo.year  = 0;

#ifndef Q_OS_WINRT

    TagLib::FileRef fileRef(QFile::encodeName(filePath).constData());
    if (!fileRef.isNull() && !fileRef.tag()->isEmpty()) {
        trackInfo.title  = TStringToQString(fileRef.tag()->title());
        trackInfo.artist = TStringToQString(fileRef.tag()->artist());
        trackInfo.album  = TStringToQString(fileRef.tag()->album());
        trackInfo.year   = fileRef.tag()->year();
        trackInfo.track  = fileRef.tag()->track();
        trackInfo.attributes.insert("lengthMilliseconds", fileRef.audioProperties()->lengthInMilliseconds());
    }

#endif

    if (trackInfo.title.isEmpty() || trackInfo.artist.isEmpty() || trackInfo.album.isEmpty()) {
        QString trackDirectory;
        foreach (QString directory, QStandardPaths::standardLocations(QStandardPaths::MusicLocation)) {
            if (filePath.startsWith(directory) && (directory.length() > trackDirectory.length())) {
                trackDirectory = directory;
            }
        }

        QStringList trackRelative = QString(filePath).remove(trackDirectory).split("/");
        if (trackRelative.at(0).isEmpty()) {
            trackRelative.removeFirst();
        }
        if (trackRelative.count() > 0) {
            if (trackInfo.title.isEmpty()) {
                trackInfo.title = trackRelative.last().replace(QRegExp("\\..+$"), "");
            }
            trackRelative.removeLast();
        }
        if (trackRelative.count() > 0) {
            if (trackInfo.artist.isEmpty()) {
                trackInfo.artist = trackRelative.first();
            }
            trackRelative.removeFirst();
        }
        if ((trackRelative.count() > 0) && (trackInfo.album.isEmpty())) {
            trackInfo.album = trackRelative.join(" - ");
        }
    }

    QList<QUrl> pictures;
    QFileInfoList entries = QDir(filePath.left(filePath.lastIndexOf("/"))).entryInfoList();
    foreach (QFileInfo entry, entries) {
        if (entry.exists() && entry.isFile() && !entry.isSymLink() && (entry.fileName().endsWith(".jpg", Qt::CaseInsensitive) || entry.fileName().endsWith(".png", Qt::CaseInsensitive))) {
            pictures.append(QUrl::fromLocalFile(entry.absoluteFilePath()));
        }
    }
    trackInfo.arts.append(pictures);

    return trackInfo;
}


Track::TrackInfo Waver::trackInfoFromIdExtra(QString id, QVariantMap extra)
{
    Track::TrackInfo trackInfo;

    trackInfo.id     = id;
    trackInfo.album  = extra.value("album", tr("Unknown album")).toString();
    trackInfo.artist = extra.value("artist", tr("Unknown artist")).toString();
    trackInfo.title  = extra.value("title", tr("Unknown title")).toString();
    trackInfo.track  = extra.value("track", 0).toInt();
    trackInfo.url    = QUrl(extra.value("url", "").toString());
    trackInfo.year   = extra.value("year", 0).toInt();

    trackInfo.tags.append(extra.value("tags", "").toString().split('|'));

    bool OK      = false;
    int  seconds = extra.value("time", 0).toInt(&OK);
    if (OK && (seconds > 0)) {
        trackInfo.attributes.insert("lengthMilliseconds", seconds * 1000);
    }

    if (extra.contains("flag") && extra.value("flag").toString().compare("0")) {
        trackInfo.attributes.insert("flag", "true");
    }

    trackInfo.arts.append(QUrl(extra.value("art").toString()));

    return trackInfo;
}


void Waver::trackInfoUpdated(QString id)
{
    if (!crossfadeInProgress && (currentTrack != nullptr) && (getCurrentTrackInfo().id.compare(id) == 0)) {
        Track::TrackInfo trackInfo = getCurrentTrackInfo();

        emit uiSetTrackData(trackInfo.title, trackInfo.artist, trackInfo.album, trackInfo.track, trackInfo.year);
        emit uiSetTrackLength(QDateTime::fromMSecsSinceEpoch(currentTrack->getLengthMilliseconds()).toUTC().toString("hh:mm:ss"));
        emit uiSetTrackTags(trackInfo.tags.join(", "));
        emit uiSetImage(trackInfo.arts.size() ? trackInfo.arts.at(0).toString() : "qrc:/images/waver.png");
        emit uiSetFavorite(trackInfo.attributes.contains("flag"));
    }
}


void Waver::trackPlayPosition(QString id, bool decoderFinished, long knownDurationMilliseconds, long positionMilliseconds, long decodedMilliseconds)
{
    Q_UNUSED(decoderFinished);

    Track *track = currentTrack;
    if (crossfadeInProgress) {
        track = previousTrack;
    }

    if (track == nullptr) {
        return;
    }
    if (track->getTrackInfo().id.compare(id)) {
        return;
    }

    lastPositionMilliseconds = positionMilliseconds;

    double positionPercent = 0;
    double decodedPercent  = 0;

    if (knownDurationMilliseconds > 0) {
        positionPercent = static_cast<double>(positionMilliseconds) / knownDurationMilliseconds;
        decodedPercent  = static_cast<double>(decodedMilliseconds) / knownDurationMilliseconds;
    }

    emit uiSetTrackPosition(QDateTime::fromMSecsSinceEpoch(positionMilliseconds).toUTC().toString("hh:mm:ss"), positionPercent, decodedPercent);

    if ((track == currentTrack) && (knownDurationMilliseconds > 0) && (knownDurationMilliseconds - positionMilliseconds <= 20000) && (playlist.size() > 0) && (playlist.at(0)->getStatus() == Track::Idle)) {
        playlist.at(0)->setStatus(Track::Decoding);
    }
}


void Waver::trackReplayGainInfo(QString id, double target, double current)
{
    if ((currentTrack != nullptr) && (getCurrentTrackInfo().id.compare(id) == 0)) {
        emit uiSetTrackReplayGain(QString("%1").arg(target, 0, 'f', 2), QString("%1").arg(current, 0, 'f', 2));
    }
}


void Waver::trackSessionExpired(QString trackId)
{
    QStringList idParts = trackId.split("|");

    QString serverId = idParts.last();
    int srvIndex     = serverIndex(serverId);
    if ((srvIndex >= servers.size()) || (srvIndex < 0)) {
        errorMessage(serverId, tr("Server ID can not be found"), serverId);
        return;
    }

    emit uiSetStatusText(tr("Networking"));
    emit uiSetStatusTempText("Session expired, retrying");

    addToLog(trackId, tr("Session expired, retrying"), tr("Session Expired"));

    if ((currentTrack != nullptr) && (getCurrentTrackInfo().id.compare(trackId) == 0)) {
        QObject *opExtra = new QObject();
        opExtra->setProperty("session_expired", "current_track");
        servers.at(srvIndex)->startOperation(AmpacheServer::Song, {{ "song_id", idParts.first().replace(0, 1, "") }}, opExtra);
        addToLog("waver", tr("Starting operation - Song"), "");
    }

    for (int i = 0; i < playlist.size(); i++) {
        Track::TrackInfo trackInfo = playlist.at(i)->getTrackInfo();

        idParts = trackInfo.id.split("|");
        if (idParts.last().compare(serverId) == 0) {
            QObject *opExtra = new QObject();
            opExtra->setProperty("session_expired", "session_expired");
            if (trackInfo.attributes.contains("group")) {
                opExtra->setProperty("group", trackInfo.attributes.value("group").toString());
            }
            servers.at(srvIndex)->startOperation(AmpacheServer::Song, {{ "song_id", idParts.first().replace(0, 1, "") }}, opExtra);
            addToLog("waver", tr("Starting operation - Song"), "");
        }
    }
}


void Waver::trackStatusChanged(QString id, Track::Status status, QString statusString)
{
    QString logString = tr("Status Changed").append(" - %1 %2").arg(status).arg(statusString);

    if ((previousTrack != nullptr) && (id.compare(previousTrack->getTrackInfo().id) == 0)) {
        addToLog(id, previousTrack->getTrackInfo().title, logString);
    }

    if ((currentTrack != nullptr) && (getCurrentTrackInfo().id.compare(id) == 0)) {
        emit uiSetStatusText(statusString);
        addToLog(id,getCurrentTrackInfo().title, logString);
    }

    foreach (Track *track, playlist) {
        if (id.compare(track->getTrackInfo().id) == 0) {
            addToLog(id, track->getTrackInfo().title, logString);
        }
    }
}


void Waver::updatedOptions(QString optionsJSON)
{
    QVariantMap options = QJsonDocument::fromJson(optionsJSON.toUtf8()).toVariant().toMap();
    QSettings   settings;

    peakFPSMax            = options.value("max_peak_fps").toInt();
    peakDelayOn           = options.value("peak_delay_on").toBool();
    peakDelayMilliseconds = options.value("peak_delay_ms").toInt();

    settings.setValue("options/shuffle_autostart", options.value("shuffle_autostart").toBool());
    settings.setValue("options/shuffle_operator", options.value("shuffle_operator").toString());
    settings.setValue("options/shuffle_count", options.value("shuffle_count").toInt());
    settings.setValue("options/random_lists_count", options.value("random_lists_count").toInt());
    settings.setValue("options/shuffle_delay_seconds", options.value("shuffle_delay_seconds").toInt());
    settings.setValue("options/shuffle_favorite_frequency", options.value("shuffle_favorite_frequency").toInt());

    settings.setValue("options/max_peak_fps", peakFPSMax);
    settings.setValue("options/peak_delay_on", peakDelayOn);
    settings.setValue("options/peak_delay_ms", peakDelayMilliseconds);

    settings.setValue("options/fade_tags", options.value("fade_tags").toString());
    settings.setValue("options/crossfade_tags", options.value("crossfade_tags").toString());
    settings.setValue("options/hide_dot_playlists", options.value("hide_dot_playlists").toBool());

    if (!options.value("eq_disable").toBool()) {
        settings.setValue("eq/on",  options.value("eq_on").toBool());
        settings.setValue("eq/eq1", options.value("eq1").toDouble());
        settings.setValue("eq/eq2", options.value("eq2").toDouble());
        settings.setValue("eq/eq3", options.value("eq3").toDouble());
        settings.setValue("eq/eq4", options.value("eq4").toDouble());
        settings.setValue("eq/eq5", options.value("eq5").toDouble());
        settings.setValue("eq/eq6", options.value("eq6").toDouble());
        settings.setValue("eq/eq7", options.value("eq7").toDouble());
        settings.setValue("eq/eq8", options.value("eq8").toDouble());
        settings.setValue("eq/eq9", options.value("eq9").toDouble());
        settings.setValue("eq/eq10", options.value("eq10").toDouble());
        settings.setValue("eq/pre_amp", options.value("pre_amp").toDouble());
    }

    if (peakFPS > peakFPSMax) {
        peakFPSMutex.lock();
        peakFPS = peakFPSMax;
        peakFPSMutex.unlock();
    }

    if (currentTrack != nullptr) {
        currentTrack->optionsUpdated();
    }
    foreach (Track *track, playlist) {
        track->optionsUpdated();
    }
}
