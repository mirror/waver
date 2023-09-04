/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/

#include "waver.h"

const QString Waver::UI_ID_PREFIX_SEARCH                        = "E";
const QString Waver::UI_ID_PREFIX_SEARCHQUERY                   = "e";
const QString Waver::UI_ID_PREFIX_SEARCHRESULT                  = "F";
const QString Waver::UI_ID_PREFIX_SEARCHRESULT_ALBUM            = "f";
const QString Waver::UI_ID_PREFIX_SEARCHRESULT_ARTIST           = "[";
const QString Waver::UI_ID_PREFIX_SEARCHRESULT_PLAYLIST         = "]";
const QString Waver::UI_ID_PREFIX_LOCALDIR                      = "A";
const QString Waver::UI_ID_PREFIX_LOCALDIR_SUBDIR               = "B";
const QString Waver::UI_ID_PREFIX_LOCALDIR_FILE                 = "C";
const QString Waver::UI_ID_PREFIX_SERVER                        = "D";
const QString Waver::UI_ID_PREFIX_SERVER_BROWSE                 = "G";
const QString Waver::UI_ID_PREFIX_SERVER_BROWSEALPHABET         = "H";
const QString Waver::UI_ID_PREFIX_SERVER_BROWSEARTIST           = "I";
const QString Waver::UI_ID_PREFIX_SERVER_BROWSEALBUM            = "J";
const QString Waver::UI_ID_PREFIX_SERVER_BROWSESONG             = "K";
const QString Waver::UI_ID_PREFIX_SERVER_PLAYLISTS              = "L";
const QString Waver::UI_ID_PREFIX_SERVER_PLAYLISTSALPHABET      = "l";
const QString Waver::UI_ID_PREFIX_SERVER_PLAYLIST               = "M";
const QString Waver::UI_ID_PREFIX_SERVER_SMARTPLAYLISTS         = "N";
const QString Waver::UI_ID_PREFIX_SERVER_SMARTPLAYLISTSALPHABET = "n";
const QString Waver::UI_ID_PREFIX_SERVER_SMARTPLAYLIST          = "O";
const QString Waver::UI_ID_PREFIX_SERVER_PLAYLIST_ITEM          = "P";
const QString Waver::UI_ID_PREFIX_SERVER_RADIOSTATIONS          = "Q";
const QString Waver::UI_ID_PREFIX_SERVER_RADIOSTATIONSALPHABET  = "q";
const QString Waver::UI_ID_PREFIX_SERVER_RADIOSTATION           = "R";
const QString Waver::UI_ID_PREFIX_SERVER_GENRES                 = "Y";
const QString Waver::UI_ID_PREFIX_SERVER_GENRESALPHABET         = "y";
const QString Waver::UI_ID_PREFIX_SERVER_GENRE                  = ">";
const QString Waver::UI_ID_PREFIX_SERVER_SHUFFLE                = "S";
const QString Waver::UI_ID_PREFIX_SERVER_SHUFFLE_FAVORITES      = "U";
const QString Waver::UI_ID_PREFIX_SERVER_SHUFFLE_NEVERPLAYED    = "V";
const QString Waver::UI_ID_PREFIX_SERVER_SHUFFLE_RECENTLYADDED  = "X";


Waver::Waver() : QObject()
{
    qRegisterMetaType<Track::Status>("Track::Status");
    qRegisterMetaType<NotificationDataToSend>("NotificationDataToSend");

    globalConstantsView = new QQuickView(QUrl("qrc:/qml/GlobalConstants.qml"));
    globalConstants = (QObject*)globalConstantsView->rootObject();

    shuffleCountdownTimer = nullptr;

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

    peakUILagLastMS     = 0;
    peakCallbackCount   = 0;
    peakLagCount        = 0;
    peakFPSNextIncrease = 0;

    searchQueriesCounter    = 0;
    searchOperationsCounter = 0;
    searchResultsCounter    = 0;

    peakFPSMutex.lock();
    peakFPS           = peakFPSMax;
    peakLagCheckCount = static_cast<int>(round(333.0 / (1000.0 / peakFPS)));
    peakFPSMutex.unlock();

    decodingCallbackInfo.callbackObject = (DecodingCallback *)this;
    decodingCallbackInfo.callbackMethod = (DecodingCallback::DecodingCallbackPointer)&Waver::decodingCallback;
    decodingCallbackInfo.trackPointer   = nullptr;

    autoRefresh       = false;
    stopByShutdown    = false;
    shutdownCompleted = false;
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
    foreach (FileSearcher *fileSearcher, fileSearchers) {
        disconnect(fileSearcher, &FileSearcher::finished, this, &Waver::fileSearchFinished);
        fileSearcher->requestInterruption();
        fileSearcher->wait();
        delete fileSearcher;
    }

    foreach(AmpacheServer *server, servers) {
        delete server;
    }

    globalConstantsView->deleteLater();
}


void Waver::actionPlay(Track::TrackInfo trackInfo, bool allowCrossfade)
{
    Track *track = new Track(trackInfo, peakCallbackInfo, decodingCallbackInfo);
    connectTrackSignals(track);

    actionPlay(track, allowCrossfade);
}


void Waver::actionPlay(Track *track, bool allowCrossfade)
{
    killPreviousTrack();

    playlist.prepend(track);
    if (currentTrack == nullptr) {
        startNextTrack();
    }
    else {
        CrossfadeMode crossfade = isCrossfade(currentTrack, track);

        previousTrack = currentTrack;
        currentTrack  = nullptr;

        if (!allowCrossfade || (crossfade == PlayNormal)) {
            previousTrack->setStatus(Track::Paused);
        }
        previousTrack->setStatus(Track::Idle);
        if (allowCrossfade && (crossfade != PlayNormal)) {
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
    emit explorerAddItem(id, QVariantMap({}), server->formattedName(), "qrc:/icons/remote.ico", QVariantMap({}), true, false, false, false);

    connect(server, &AmpacheServer::operationFinished, this, &Waver::serverOperationFinished);
    connect(server, &AmpacheServer::passwordNeeded,    this, &Waver::serverPasswordNeeded);
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


void Waver::autoRefreshNext()
{
    if (autoRefreshLastServerIndex >= servers.count()) {
        return;
    }

    itemActionServer(QString("%1%2").arg(UI_ID_PREFIX_SERVER).arg(autoRefreshLastServerIndex), globalConstant("action_collapse").toInt(), QVariantMap());
    itemActionServer(QString("%1%2").arg(UI_ID_PREFIX_SERVER).arg(autoRefreshLastServerIndex), globalConstant("action_expand").toInt(), QVariantMap());

    QString id;
    AmpacheServer::OpCode opCode = AmpacheServer::Unknown;
    if (autoRefreshLastItem == AUTO_REFRESH_BROWSE) {
        id     = QString("%1%2|%3%4").arg(UI_ID_PREFIX_SERVER_BROWSE).arg(autoRefreshLastServerIndex).arg(UI_ID_PREFIX_SERVER).arg(autoRefreshLastServerIndex);
        opCode = AmpacheServer::BrowseRoot;
    }
    else if (autoRefreshLastItem == AUTO_REFRESH_PLAYLISTS) {
        opCode = AmpacheServer::PlaylistRoot;

        QString playlistsId     = QString("%1%2|%3%4").arg(UI_ID_PREFIX_SERVER_PLAYLISTS).arg(autoRefreshLastServerIndex).arg(UI_ID_PREFIX_SERVER).arg(autoRefreshLastServerIndex);
        QString smartPlaylistId = QString("%1%2|%3%4").arg(UI_ID_PREFIX_SERVER_SMARTPLAYLISTS).arg(autoRefreshLastServerIndex).arg(UI_ID_PREFIX_SERVER).arg(autoRefreshLastServerIndex);

        explorerNetworkingUISignals(playlistsId, true);
        explorerNetworkingUISignals(smartPlaylistId, true);
        emit explorerRemoveChildren(playlistsId);
        emit explorerRemoveChildren(smartPlaylistId);
    }
    else if (autoRefreshLastItem == AUTO_REFRESH_RAIOSTATIONS) {
        id     = QString("%1%2|%3%4").arg(UI_ID_PREFIX_SERVER_RADIOSTATIONS).arg(autoRefreshLastServerIndex).arg(UI_ID_PREFIX_SERVER).arg(autoRefreshLastServerIndex);
        opCode = AmpacheServer::RadioStations;
    }
    else if (autoRefreshLastItem == AUTO_REFRESH_GENRES) {
        id     = QString("%1%2|%3%4").arg(UI_ID_PREFIX_SERVER_GENRES).arg(autoRefreshLastServerIndex).arg(UI_ID_PREFIX_SERVER).arg(autoRefreshLastServerIndex);
        opCode = AmpacheServer::Tags;
    }

    if (!id.isEmpty()) {
        explorerNetworkingUISignals(id, true);
        emit explorerRemoveChildren(id);
    }
    if (opCode != AmpacheServer::Unknown) {
        QObject *opExtra = new QObject();
        opExtra->setProperty("auto_refresh", "auto_refresh");

        servers.at(autoRefreshLastServerIndex)->startOperation(opCode, AmpacheServer::OpData(), opExtra);

        autoRefreshLastItem++;
        if (autoRefreshLastItem >= 4) {
            autoRefreshLastItem = 0;
            autoRefreshLastServerIndex++;
        }

        QSettings settings;
        settings.setValue("auto_refresh/last_item", autoRefreshLastItem);
        settings.setValue("auto_refresh/last_server_index", autoRefreshLastServerIndex);
    }
}


void Waver::clearTrackUISignals()
{
    emit uiSetTrackData("", "", "", "", "", "");
    emit uiSetTrackBusy(false);
    emit uiSetTrackLength("");
    emit uiSetTrackPosition("", 0);
    emit uiSetTrackTags("");
    emit uiSetTrackAmpacheURL("");
    emit uiSetImage("qrc:/images/waver.png");
    emit uiSetStatusText(tr("Stopped"));
    emit uiSetPeakMeter(0, 0, QDateTime::currentMSecsSinceEpoch() + 100);
    emit uiSetPeakMeterReplayGain(0);
}


void Waver::connectTrackSignals(Track *track, bool newConnect)
{
    if (newConnect) {
        connect(track, &Track::playPosition,      this, &Waver::trackPlayPosition);
        connect(track, &Track::networkConnecting, this, &Waver::trackNetworkConnecting);
        connect(track, &Track::replayGainInfo,    this, &Waver::trackReplayGainInfo);
        connect(track, &Track::finished,          this, &Waver::trackFinished);
        connect(track, &Track::fadeoutStarted,    this, &Waver::trackFadeoutStarted);
        connect(track, &Track::trackInfoUpdated,  this, &Waver::trackInfoUpdated);
        connect(track, &Track::error,             this, &Waver::trackError);
        connect(track, &Track::info,              this, &Waver::trackInfo);
        connect(track, &Track::sessionExpired,    this, &Waver::trackSessionExpired);
        connect(track, &Track::statusChanged,     this, &Waver::trackStatusChanged);

        connect(this,  &Waver::requestTrackBufferReplayGainInfo, track, &Track::requestForBufferReplayGainInfo);

        return;
    }

    disconnect(track, &Track::playPosition,      this, &Waver::trackPlayPosition);
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


void Waver::decodingCallback(double downloadPercent, double PCMPercent, void *trackPointer)
{
    if (currentTrack == trackPointer) {
        emit uiSetTrackDecoding(downloadPercent, PCMPercent);
        return;
    }

    for (int i = 0; i < playlist.size(); i++) {
        if (playlist.at(i) == trackPointer) {
            emit playlistDecoding(i, downloadPercent, PCMPercent);
        }
    }
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
}


void Waver::errorMessage(QString id, QString info, QString error)
{
    #ifdef QT_DEBUG
        qDebug() << id << info << error;
    #endif

    if (error.compare("4704 No Results") == 0) {
        searchOperationsCounter--;
        if (searchOperationsCounter <= 0) {
            foreach(QString key, searchQueries.keys()) {
                explorerNetworkingUISignals(searchQueries.value(key), false);
            }
            searchAction();
        }
    }

    emit uiSetStatusTempText(QString("%1 <i>%2</i>").arg(info, error));
}


void Waver::explorerItemClicked(QString id, int action, QString extraJSON)
{
    QVariantMap extra = QJsonDocument::fromJson(extraJSON.toUtf8()).toVariant().toMap();

    if (extra.contains("art") && (action != globalConstant("action_collapse"))) {
        emit uiSetTempImage(extra.value("art"));
    }

    if (id.startsWith(UI_ID_PREFIX_SEARCH) || id.startsWith(UI_ID_PREFIX_SEARCHQUERY)) {
        itemActionSearch(id, action, extra);
        return;
    }
    if (id.startsWith(UI_ID_PREFIX_SEARCHRESULT) || id.startsWith(UI_ID_PREFIX_SEARCHRESULT_ALBUM) || id.startsWith(UI_ID_PREFIX_SEARCHRESULT_ARTIST) || id.startsWith(UI_ID_PREFIX_SEARCHRESULT_PLAYLIST)) {
        itemActionSearchResult(id, action, extra);
        return;
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
}


void Waver::fileScanFinished()
{
    FileScanner *fileScanner = (FileScanner*) QObject::sender();

    disconnect(fileScanner, &FileScanner::finished, this, &Waver::fileScanFinished);

    QRandomGenerator *randomGenerator = QRandomGenerator::global();

    int counter = 0;
    foreach (QFileInfo dir, fileScanner->getDirs()) {
        counter++;
        emit explorerAddItem(QString("%1%2").arg(UI_ID_PREFIX_LOCALDIR_SUBDIR).arg(randomGenerator->bounded(std::numeric_limits<quint32>::max())), fileScanner->getUiData(), dir.baseName(), "qrc:/icons/browse.ico", QVariantMap({{ "path", dir.absoluteFilePath() }}), true, false, false, false);
    }
    foreach (QFileInfo file, fileScanner->getFiles()) {
        counter++;
        emit explorerAddItem(QString("%1%2").arg(UI_ID_PREFIX_LOCALDIR_FILE).arg(randomGenerator->bounded(std::numeric_limits<quint32>::max())), fileScanner->getUiData(), file.fileName(), "qrc:/icons/audio_file.ico", QVariantMap({{ "path", file.absoluteFilePath() }}), false, true, false, false);
    }

    emit explorerSetBusy(fileScanner->getUiData(), false);

    if (!counter) {
        errorMessage("local", tr("No audio files found in directory"), fileScanner->getUiData());
    }

    delete fileScanner;
    fileScanners.removeAll(fileScanner);
}


void Waver::fileSearchFinished()
{
    FileSearcher *fileSearcher = (FileSearcher*) QObject::sender();

    disconnect(fileSearcher, &FileSearcher::finished, this, &Waver::fileSearchFinished);

    QRandomGenerator *randomGenerator = QRandomGenerator::global();

    QSettings settings;
    int searchMaxCount = settings.value("options/search_count_max", DEFAULT_SEARCH_COUNT_MAX).toInt();

    foreach (QFileInfo file, fileSearcher->getResults()) {
        if ((searchResultsCounter < searchMaxCount) || (searchMaxCount == 0)) {
            emit explorerAddItem(QString("%1%2").arg(UI_ID_PREFIX_LOCALDIR_FILE).arg(randomGenerator->bounded(std::numeric_limits<quint32>::max())), fileSearcher->getUiData(), file.fileName(), "qrc:/icons/audio_file.ico", QVariantMap({{ "path", file.absoluteFilePath() }}), false, true, false, false);
            searchResultsCounter++;
            continue;
        }
        break;
    }

    delete fileSearcher;
    fileSearchers.removeAll(fileSearcher);
}


QString Waver::formatFrequencyValue(double hertz)
{
    if (hertz >= 1000) {
        return QString("%1KHz").arg(static_cast<double>(hertz) / 1000, 0, 'f', 1);
    }
    return QString("%1Hz").arg(static_cast<double>(hertz), 0, 'f', 0);
}


QString Waver::formatMemoryValue(unsigned long bytes, bool padded)
{
    if (bytes > (1024 * 1024)) {
        return QString("%1MB").arg(static_cast<double>(bytes) / (1024 * 1024), padded ? 7 : 0, 'f', 2);
    }

    if (bytes > 1024) {
        return QString("%1KB").arg(static_cast<double>(bytes) / 1024, padded ? 7 : 0, 'f', 2);
    }

    return QString(padded ? "%1B " : "%1B").arg(static_cast<double>(bytes), padded ? 7 : 0, 'f', 0);
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


Waver::CrossfadeMode Waver::isCrossfade(Track *track1, Track *track2)
{
    if ((track1 == nullptr) || (track2 == nullptr)) {
        return PlayNormal;
    }

    if (crossfadeTags.contains("*")) {
        if ((track1->getTrackInfo().albumId == track2->getTrackInfo().albumId) && (track1->getTrackInfo().track == track1->getTrackInfo().track - 1)) {
            return ShortCrossfade;
        }
        return Crossfade;
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

    if (crossfade1 && crossfade2) {
        if ((track1->getTrackInfo().albumId == track2->getTrackInfo().albumId) && (track1->getTrackInfo().track == track2->getTrackInfo().track - 1)) {
            return ShortCrossfade;
        }
        return Crossfade;
    }

    return PlayNormal;
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
        Track::TrackInfo trackInfo = trackInfoFromFilePath(extra.value("path").toString());

        if (extra.contains("search_action_parent_id")) {
            QString explorerId = extra.value("search_action_parent_id").toString();
            if (!searchActionItemsCounter.contains(explorerId)) {
                return;
            }

            if ((searchActionItemsCounter.value(explorerId).filter == globalConstant("search_action_filter_exactmatch")) && trackInfo.title.compare(searchActionItemsCounter.value(explorerId).query, Qt::CaseInsensitive)) {
                searchAction();
                return;
            }
            if ((searchActionItemsCounter.value(explorerId).filter == globalConstant("search_action_filter_startswith")) && !trackInfo.title.startsWith(searchActionItemsCounter.value(explorerId).query, Qt::CaseInsensitive)) {
                searchAction();
                return;
            }

            searchActionItemsCounter[explorerId].doneCounter++;
            searchAction();
        }

        playlist.clear();
        actionPlay(trackInfo);
    }
    else if ((action == globalConstant("action_playnext")) || (action == globalConstant("action_enqueue"))) {
        stopShuffleCountdown();

        Track::TrackInfo trackInfo = trackInfoFromFilePath(extra.value("path").toString());

        if (extra.contains("search_action_parent_id")) {
            QString explorerId = extra.value("search_action_parent_id").toString();
            if (!searchActionItemsCounter.contains(explorerId)) {
                return;
            }

            if ((searchActionItemsCounter.value(explorerId).filter == globalConstant("search_action_filter_exactmatch")) && trackInfo.title.compare(searchActionItemsCounter.value(explorerId).query, Qt::CaseInsensitive)) {
                searchAction();
                return;
            }
            if ((searchActionItemsCounter.value(explorerId).filter == globalConstant("search_action_filter_startswith")) && !trackInfo.title.startsWith(searchActionItemsCounter.value(explorerId).query, Qt::CaseInsensitive)) {
                searchAction();
                return;
            }

            searchActionItemsCounter[explorerId].doneCounter++;
            searchAction();
        }

        Track *track = new Track(trackInfo, peakCallbackInfo, decodingCallbackInfo);
        connectTrackSignals(track);

        if (action == globalConstant("action_enqueue")) {
            playlist.append(track);
        }
        else {
            playlist.prepend(track);
        }
        playlistUpdated();
    }
    else if (action == globalConstant("action_collapse")) {
        emit explorerRemoveChildren(id);
    }
}


void Waver::itemActionSearch(QString id, int action, QVariantMap extra)
{
    Q_UNUSED(action);

    if (searchOperationsCounter > 0) {
        return;
    }

    if (id.startsWith(UI_ID_PREFIX_SEARCH)) {
        emit uiShowSearchCriteria();
        return;
    }

    QString parentId;
    QString criteria;
    foreach (QString key, searchQueries.keys()) {
        emit explorerRemoveChildren(searchQueries.value(key));
        if (searchQueries.value(key).compare(id) == 0) {
            parentId = id;
            criteria = key;
        }
    }
    if (parentId.isEmpty()) {
        if (!extra.contains("criteria")) {
            return;
        }
        criteria = extra.value("criteria").toString();
        if (!searchQueries.contains(criteria)) {
            return;
        }
        parentId = searchQueries.value(criteria);
    }


    searchResultsCounter = 0;
    if(searchActionItemsCounter.contains(parentId)) {
        searchActionItemsCounter[parentId].doneCounter = 0;
    }

    searchServers(parentId, criteria);
    searchLocalDirs(parentId, criteria);
    searchCaches(parentId, criteria);
}


void Waver::itemActionSearchResult(QString id, int action, QVariantMap extra)
{
    QStringList idParts  = id.split("|");
    QString     serverId = idParts.last();
    int         srvIndex = serverIndex(serverId);
    if ((srvIndex >= servers.size()) || (srvIndex < 0)) {
        errorMessage(serverId, tr("Server ID can not be found"), serverId);
        return;
    }

    bool OK = false;
    int destinationIndex = extra.value("destination_index", 0).toInt(&OK);
    if (!OK || (destinationIndex < 0) || (destinationIndex > playlist.count())) {
        destinationIndex = 0;
    }

    QRandomGenerator *randomGenerator = QRandomGenerator::global();

    if (action == globalConstant("action_play")) {
        if (id.startsWith(UI_ID_PREFIX_SEARCHRESULT_ARTIST)) {
            bool OK = false;
            int  artistId = QString(idParts.first()).remove(0, 1).toInt(&OK);

            if (OK) {
                if (extra.contains("search_action_parent_id")) {
                    QString parentId = extra.value("search_action_parent_id").toString();
                    if (extra.contains("group")) {
                        if ((searchActionItemsCounter.value(parentId).filter == globalConstant("search_action_filter_exactmatch")) && extra.value("group").toString().compare(searchActionItemsCounter.value(parentId).query, Qt::CaseInsensitive)) {
                            searchAction();
                            return;
                        }
                        if ((searchActionItemsCounter.value(parentId).filter == globalConstant("search_action_filter_startswith")) && !extra.value("group").toString().startsWith(searchActionItemsCounter.value(parentId).query, Qt::CaseInsensitive)) {
                            searchAction();
                            return;
                        }
                    }
                    startShuffleBatch(srvIndex, extra.value("group").toString(), artistId, None, "action_play", 0, 0, parentId);
                }
                else {
                    startShuffleBatch(srvIndex, extra.value("group").toString(), artistId);
                }
            }
            return;
        }
        else if (id.startsWith(UI_ID_PREFIX_SEARCHRESULT_ALBUM)) {
            QObject *opExtra = new QObject();
            opExtra->setProperty("original_action", "action_play");
            opExtra->setProperty("group", extra.value("group").toString());
            if (extra.contains("from_search")) {
                opExtra->setProperty("from_search", extra.value("from_search").toString());
            }
            if (extra.contains("search_action_parent_id")) {
                QString parentId = extra.value("search_action_parent_id").toString();
                if (extra.contains("group")) {
                    if ((searchActionItemsCounter.value(parentId).filter == globalConstant("search_action_filter_exactmatch")) && extra.value("group").toString().compare(searchActionItemsCounter.value(parentId).query, Qt::CaseInsensitive)) {
                        searchAction();
                        return;
                    }
                    if ((searchActionItemsCounter.value(parentId).filter == globalConstant("search_action_filter_startswith")) && !extra.value("group").toString().startsWith(searchActionItemsCounter.value(parentId).query, Qt::CaseInsensitive)) {
                        searchAction();
                        return;
                    }
                }
                opExtra->setProperty("search_action_parent_id", parentId);
            }

            explorerNetworkingUISignals(id, true);
            emit playlistBigBusy(true);
            servers.at(srvIndex)->startOperation(AmpacheServer::BrowseAlbum, {{ "album", QString(idParts.first()).remove(0, 1) }}, opExtra);
        }
        else if (id.startsWith(UI_ID_PREFIX_SEARCHRESULT)) {
            Track::TrackInfo trackInfo = trackInfoFromIdExtra(id, extra);

            if (extra.contains("search_action_parent_id")) {
                QString explorerId = extra.value("search_action_parent_id").toString();
                if (!searchActionItemsCounter.contains(explorerId)) {
                    return;
                }

                if ((searchActionItemsCounter.value(explorerId).filter == globalConstant("search_action_filter_exactmatch")) && trackInfo.title.compare(searchActionItemsCounter.value(explorerId).query, Qt::CaseInsensitive)) {
                    searchAction();
                    return;
                }
                if ((searchActionItemsCounter.value(explorerId).filter == globalConstant("search_action_filter_startswith")) && !trackInfo.title.startsWith(searchActionItemsCounter.value(explorerId).query, Qt::CaseInsensitive)) {
                    searchAction();
                    return;
                }

                searchActionItemsCounter[explorerId].doneCounter++;
                searchAction();
            }

            actionPlay(trackInfo);
            playlist.clear();
            playlistUpdated();
            playlistSave();
        }
        else if (id.startsWith(UI_ID_PREFIX_SEARCHRESULT_PLAYLIST)) {
            QObject *opExtra = new QObject();
            opExtra->setProperty("original_action", "action_play");
            opExtra->setProperty("group", extra.value("group").toString());
            if (extra.contains("from_search")) {
                opExtra->setProperty("from_search", extra.value("from_search").toString());
            }
            if (extra.contains("search_action_parent_id")) {
                QString parentId = extra.value("search_action_parent_id").toString();
                if (extra.contains("group")) {
                    if ((searchActionItemsCounter.value(parentId).filter == globalConstant("search_action_filter_exactmatch")) && extra.value("group").toString().compare(searchActionItemsCounter.value(parentId).query, Qt::CaseInsensitive)) {
                        searchAction();
                        return;
                    }
                    if ((searchActionItemsCounter.value(parentId).filter == globalConstant("search_action_filter_startswith")) && !extra.value("group").toString().startsWith(searchActionItemsCounter.value(parentId).query, Qt::CaseInsensitive)) {
                        searchAction();
                        return;
                    }
                }
                opExtra->setProperty("search_action_parent_id", parentId);
            }

            explorerNetworkingUISignals(id, true);
            emit playlistBigBusy(true);
            servers.at(srvIndex)->startOperation(AmpacheServer::PlaylistSongs, {{ "playlist", QString(idParts.first()).remove(0, 1) }}, opExtra);
        }
    }

    if ((action == globalConstant("action_playnext")) || (action == globalConstant("action_enqueue")) || (action == globalConstant("action_insert")) || (action == globalConstant("action_enqueueshuffled"))) {
        QString actionStr =
            action == globalConstant("action_playnext")        ? "action_playnext" :
            action == globalConstant("action_enqueue")         ? "action_enqueue"  :
            action == globalConstant("action_insert")          ? "action_insert"   :
                                                                 "action_enqueueshuffled";

        if (actionStr.isEmpty()) {
            return;
        }

        if (id.startsWith(UI_ID_PREFIX_SEARCHRESULT_ARTIST)) {
            bool OK = false;
            int artistId = QString(idParts.first()).remove(0, 1).toInt(&OK);
            if (OK) {
                if (extra.contains("search_action_parent_id")) {
                    QString parentId = extra.value("search_action_parent_id").toString();
                    if (extra.contains("group")) {
                        if ((searchActionItemsCounter.value(parentId).filter == globalConstant("search_action_filter_exactmatch")) && extra.value("group").toString().compare(searchActionItemsCounter.value(parentId).query, Qt::CaseInsensitive)) {
                            searchAction();
                            return;
                        }
                        if ((searchActionItemsCounter.value(parentId).filter == globalConstant("search_action_filter_startswith")) && !extra.value("group").toString().startsWith(searchActionItemsCounter.value(parentId).query, Qt::CaseInsensitive)) {
                            searchAction();
                            return;
                        }
                    }
                    startShuffleBatch(srvIndex, extra.value("group").toString(), artistId, None, actionStr, 0, destinationIndex, parentId);
                }
                else {
                    startShuffleBatch(srvIndex, extra.value("group").toString(), artistId, None, actionStr, 0, destinationIndex);
                }
            }
        }
        else if (id.startsWith(UI_ID_PREFIX_SEARCHRESULT_ALBUM)) {
            QObject *opExtra = new QObject();
            opExtra->setProperty("original_action", actionStr);
            opExtra->setProperty("group", extra.value("group").toString());
            if (extra.contains("from_search")) {
                opExtra->setProperty("from_search", extra.value("from_search").toString());
            }
            if (extra.contains("search_action_parent_id")) {
                QString parentId = extra.value("search_action_parent_id").toString();
                if (extra.contains("group")) {
                    if ((searchActionItemsCounter.value(parentId).filter == globalConstant("search_action_filter_exactmatch")) && extra.value("group").toString().compare(searchActionItemsCounter.value(parentId).query, Qt::CaseInsensitive)) {
                        searchAction();
                        return;
                    }
                    if ((searchActionItemsCounter.value(parentId).filter == globalConstant("search_action_filter_startswith")) && !extra.value("group").toString().startsWith(searchActionItemsCounter.value(parentId).query, Qt::CaseInsensitive)) {
                        searchAction();
                        return;
                    }
                }
                opExtra->setProperty("search_action_parent_id", parentId);
            }
            if (destinationIndex > 0) {
                opExtra->setProperty("destination_index", QString("%1").arg(destinationIndex));
            }

            explorerNetworkingUISignals(id, true);
            emit playlistBigBusy(true);
            servers.at(srvIndex)->startOperation(AmpacheServer::BrowseAlbum, {{ "album", QString(idParts.first()).remove(0, 1) }}, opExtra);
        }
        else if (id.startsWith(UI_ID_PREFIX_SEARCHRESULT)) {
            if (playlistContains(id)) {
                if (extra.contains("search_action_parent_id")) {
                    searchAction();
                }
                return;
            }

            Track::TrackInfo trackInfo = trackInfoFromIdExtra(id, extra);

            if (extra.contains("search_action_parent_id")) {
                QString explorerId = extra.value("search_action_parent_id").toString();
                if (!searchActionItemsCounter.contains(explorerId)) {
                    return;
                }

                if ((searchActionItemsCounter.value(explorerId).filter == globalConstant("search_action_filter_exactmatch")) && trackInfo.title.compare(searchActionItemsCounter.value(explorerId).query, Qt::CaseInsensitive)) {
                    searchAction();
                    return;
                }
                if ((searchActionItemsCounter.value(explorerId).filter == globalConstant("search_action_filter_startswith")) && !trackInfo.title.startsWith(searchActionItemsCounter.value(explorerId).query, Qt::CaseInsensitive)) {
                    searchAction();
                    return;
                }

                searchActionItemsCounter[explorerId].doneCounter++;
                searchAction();
            }

            Track *track = new Track(trackInfo, peakCallbackInfo, decodingCallbackInfo);
            connectTrackSignals(track);

            if (action == globalConstant("action_enqueue")) {
                playlist.append(track);
            }
            else if (action == globalConstant("action_playnext")) {
                playlist.prepend(track);
            }
            else if (action == globalConstant("action_insert")) {
                playlist.insert(destinationIndex, track);
            }
            else if (action == globalConstant("action_enqueueshuffled")) {
                if (playlist.size() < 1) {
                    playlist.append(track);
                }
                else {
                    playlist.insert(randomGenerator->bounded(playlist.size()), track);
                }
            }

            playlistUpdated();
            playlistSave();
        }
        else if (id.startsWith(UI_ID_PREFIX_SEARCHRESULT_PLAYLIST)) {
            QObject *opExtra = new QObject();
            opExtra->setProperty("original_action", actionStr);
            opExtra->setProperty("group", extra.value("group").toString());
            if (extra.contains("from_search")) {
                opExtra->setProperty("from_search", extra.value("from_search").toString());
            }
            if (extra.contains("search_action_parent_id")) {
                QString parentId = extra.value("search_action_parent_id").toString();
                if (extra.contains("group")) {
                    if ((searchActionItemsCounter.value(parentId).filter == globalConstant("search_action_filter_exactmatch")) && extra.value("group").toString().compare(searchActionItemsCounter.value(parentId).query, Qt::CaseInsensitive)) {
                        searchAction();
                        return;
                    }
                    if ((searchActionItemsCounter.value(parentId).filter == globalConstant("search_action_filter_startswith")) && !extra.value("group").toString().startsWith(searchActionItemsCounter.value(parentId).query, Qt::CaseInsensitive)) {
                        searchAction();
                        return;
                    }
                }
                opExtra->setProperty("search_action_parent_id", parentId);
            }
            if (destinationIndex > 0) {
                opExtra->setProperty("destination_index", QString("%1").arg(destinationIndex));
            }

            explorerNetworkingUISignals(id, true);
            emit playlistBigBusy(true);
            servers.at(srvIndex)->startOperation(AmpacheServer::PlaylistSongs, {{ "playlist", QString(idParts.first()).remove(0, 1) }}, opExtra);
        }
    }
}


void Waver::itemActionServer(QString id, int action, QVariantMap extra)
{
    Q_UNUSED(extra);

    autoRefreshLastActionTimestamp = QDateTime::currentMSecsSinceEpoch();

    if ((action == globalConstant("action_expand")) || (action == globalConstant("action_refresh"))) {
        stopShuffleCountdown();

        emit explorerRemoveChildren(id);

        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_BROWSE), id),                id, tr("Browse"),          "qrc:/icons/browse.ico",        QVariantMap({}), true, false, false, false);
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_PLAYLISTS), id),             id, tr("Playlists"),       "qrc:/icons/playlist.ico",      QVariantMap({}), true, false, false, false);
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SMARTPLAYLISTS), id),        id, tr("Smart Playlists"), "qrc:/icons/playlist.ico",      QVariantMap({}), true, false, false, false);
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_RADIOSTATIONS), id),         id, tr("Radio Stations"),  "qrc:/icons/radio_station.ico", QVariantMap({}), true, false, false, false);
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_GENRES), id),                id, tr("Genres"),          "qrc:/icons/genre.ico",         QVariantMap({}), true, false, false, false);
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SHUFFLE), id),               id, tr("Shuffle"),         "qrc:/icons/shuffle.ico",       QVariantMap({}), false, true, false, false);
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SHUFFLE_FAVORITES), id),     id, tr("Favorites"),       "qrc:/icons/shuffle.ico",       QVariantMap({}), false, true, false, false);
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SHUFFLE_NEVERPLAYED), id),   id, tr("Never Played"),    "qrc:/icons/shuffle.ico",       QVariantMap({}), false, true, false, false);
        emit explorerAddItem(QString("%1|%2").arg(QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SHUFFLE_RECENTLYADDED), id), id, tr("Recently Added"),  "qrc:/icons/shuffle.ico",       QVariantMap({}), false, true, false, false);
    }
    else if (action == globalConstant("action_collapse")) {
        emit explorerRemoveChildren(id);
    }
}


void Waver::itemActionServerItem(QString id, int action, QVariantMap extra)
{
    autoRefreshLastActionTimestamp = QDateTime::currentMSecsSinceEpoch();

    stopShuffleCountdown();

    QStringList idParts  = id.split("|");
    QString     serverId = idParts.last();
    int         srvIndex = serverIndex(serverId);
    if ((srvIndex >= servers.size()) || (srvIndex < 0)) {
        errorMessage(serverId, tr("Server ID can not be found"), serverId);
        return;
    }

    QSettings         settings;
    QRandomGenerator *randomGenerator = QRandomGenerator::global();

    int alphabetLimit = settings.value("options/alphabet_limit", DEFAULT_ALPHABET_LIMIT).toInt();

    if (action == globalConstant("action_expand")) {
        if ((id.startsWith(UI_ID_PREFIX_SERVER_BROWSE) || id.startsWith(UI_ID_PREFIX_SERVER_BROWSEALPHABET))) {
            QString browseCacheKey = QString("%1/browse").arg(servers.at(srvIndex)->getSettingsId().toString());

            if (settings.contains(QString("%1/1/id").arg(browseCacheKey))) {
                QChar   currentChar;
                QString added;

                int browseSize = settings.beginReadArray(browseCacheKey);
                for (int i = 0; i < browseSize; i++) {
                    settings.setArrayIndex(i);

                    QString name     = settings.value("name").toString();
                    QChar   alphabet = alphabetFromName(name);

                    if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSE) && (browseSize > alphabetLimit) && (currentChar != alphabet) && !added.contains(alphabet)) {
                        currentChar = alphabet;
                        added.append(alphabet);
                        QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_BROWSEALPHABET).arg(randomGenerator->bounded(std::numeric_limits<quint32>::max())).arg(serverId);
                        emit explorerAddItem(newId, id, currentChar, "qrc:/icons/browse.ico", QVariantMap({{ "alphabet", currentChar }}), true, false, false, false);
                    }
                    if ((id.startsWith(UI_ID_PREFIX_SERVER_BROWSEALPHABET) && extra.value("alphabet", "").toString().startsWith(alphabet)) || (id.startsWith(UI_ID_PREFIX_SERVER_BROWSE) && (browseSize <= alphabetLimit))) {
                        QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_BROWSEARTIST, settings.value("id").toString(), serverId);
                        emit explorerAddItem(newId, id, name, settings.value("art"), QVariantMap({{ "group", name }, { "art", settings.value("art") }}), true, true, false, false);
                    }
                }
                settings.endArray();
                return;
            }
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_PLAYLISTS) || id.startsWith(UI_ID_PREFIX_SERVER_PLAYLISTSALPHABET) || id.startsWith(UI_ID_PREFIX_SERVER_SMARTPLAYLISTS) || id.startsWith(UI_ID_PREFIX_SERVER_SMARTPLAYLISTSALPHABET)) {
            QString playlistsCacheKey = QString("%1/playlists").arg(servers.at(srvIndex)->getSettingsId().toString());
            bool    hideDotPlaylists  = settings.value("options/hide_dot_playlists", DEFAULT_HIDE_DOT_PLAYLIST).toBool();

            if (settings.contains(QString("%1/1/id").arg(playlistsCacheKey))) {
                int playlistsCount  = 0;
                int smartlistsCount = 0;

                int playlistsSize = settings.beginReadArray(playlistsCacheKey);
                for (int i = 0; i < playlistsSize; i++) {
                    settings.setArrayIndex(i);
                    if (!settings.value("name").toString().startsWith(".") || !hideDotPlaylists) {
                        if (settings.value("id").toString().startsWith("smart", Qt::CaseInsensitive)) {
                            smartlistsCount++;
                            continue;
                        }
                        playlistsCount++;
                    }
                }
                settings.endArray();

                QChar   currentChar;
                QString added;

                playlistsSize = settings.beginReadArray(playlistsCacheKey);
                for (int i = 0; i < playlistsSize; i++) {
                    settings.setArrayIndex(i);

                    QString playlistId   = settings.value("id").toString();
                    QString playlistName = settings.value("name").toString();
                    QChar   alphabet     = alphabetFromName(playlistName);

                    if (!playlistId.startsWith("smart", Qt::CaseInsensitive) && (!playlistName.startsWith(".") || !hideDotPlaylists)) {
                        if (id.startsWith(UI_ID_PREFIX_SERVER_PLAYLISTS) && (playlistsCount > alphabetLimit) && (currentChar != alphabet) && !added.contains(alphabet)) {
                            currentChar = alphabet;
                            added.append(alphabet);
                            QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_PLAYLISTSALPHABET).arg(randomGenerator->bounded(std::numeric_limits<quint32>::max())).arg(serverId);
                            emit explorerAddItem(newId, id, currentChar, "qrc:/icons/playlist.ico", QVariantMap({{ "alphabet", currentChar }}), true, false, false, false);
                        }
                        if ((id.startsWith(UI_ID_PREFIX_SERVER_PLAYLISTSALPHABET) && extra.value("alphabet", "").toString().startsWith(alphabet)) || (id.startsWith(UI_ID_PREFIX_SERVER_PLAYLISTS) && (playlistsCount <= alphabetLimit))) {
                            QString art = settings.value("art", "").toString();
                            if (art.isEmpty()) {
                                art ="qrc:/icons/playlist.ico";
                            }

                            QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_PLAYLIST, playlistId, serverId);
                            emit explorerAddItem(newId, id, playlistName, art, QVariantMap({{ "group", playlistName }, { "art", art }}), false, true, false, false);
                        }
                    }
                    if (playlistId.startsWith("smart", Qt::CaseInsensitive) && (!playlistName.startsWith(".") || !hideDotPlaylists)) {
                        if (id.startsWith(UI_ID_PREFIX_SERVER_SMARTPLAYLISTS) && (smartlistsCount > alphabetLimit) && (currentChar != alphabet) && !added.contains(alphabet)) {
                            currentChar = alphabet;
                            added.append(alphabet);
                            QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_SMARTPLAYLISTSALPHABET).arg(randomGenerator->bounded(std::numeric_limits<quint32>::max())).arg(serverId);
                            emit explorerAddItem(newId, id, currentChar, "qrc:/icons/playlist.ico", QVariantMap({{ "alphabet", currentChar }}), true, false, false, false);
                        }
                        if ((id.startsWith(UI_ID_PREFIX_SERVER_SMARTPLAYLISTSALPHABET) && extra.value("alphabet", "").toString().startsWith(alphabet)) || (id.startsWith(UI_ID_PREFIX_SERVER_SMARTPLAYLISTS) && (smartlistsCount <= alphabetLimit))) {
                            QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_SMARTPLAYLIST, playlistId, serverId);
                            emit explorerAddItem(newId, id, playlistName, "qrc:/icons/playlist.ico", QVariantMap({{ "group", playlistName }}), false, true, false, false);
                        }
                    }
                }
                settings.endArray();
                return;
            }
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_RADIOSTATIONS) || id.startsWith(UI_ID_PREFIX_SERVER_RADIOSTATIONSALPHABET)) {
            QString radioStationsCacheKey = QString("%1/radiostations").arg(servers.at(srvIndex)->getSettingsId().toString());

            if (settings.contains(QString("%1/1/id").arg(radioStationsCacheKey))) {
                QChar   currentChar;
                QString added;

                int radiosSize = settings.beginReadArray(radioStationsCacheKey);
                for (int i = 0; i < radiosSize; i++) {
                    settings.setArrayIndex(i);

                    QString unescapedName = QTextDocumentFragment::fromHtml(settings.value("name").toString()).toPlainText();
                    QChar   alphabet      = alphabetFromName(unescapedName);

                    if (id.startsWith(UI_ID_PREFIX_SERVER_RADIOSTATIONS) && (radiosSize > alphabetLimit) && (currentChar != alphabet) && !added.contains(alphabet)) {
                        currentChar = alphabet;
                        added.append(alphabet);
                        QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_RADIOSTATIONSALPHABET).arg(randomGenerator->bounded(std::numeric_limits<quint32>::max())).arg(serverId);
                        emit explorerAddItem(newId, id, currentChar, "qrc:/icons/radio_station.ico", QVariantMap({{ "alphabet", currentChar }}), true, false, false, false);
                    }
                    if ((id.startsWith(UI_ID_PREFIX_SERVER_RADIOSTATIONSALPHABET) && extra.value("alphabet", "").toString().startsWith(alphabet)) || (id.startsWith(UI_ID_PREFIX_SERVER_RADIOSTATIONS) && (radiosSize <= alphabetLimit))) {
                        QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_RADIOSTATION, settings.value("id").toString(), serverId);
                        emit explorerAddItem(newId, id, unescapedName, "qrc:/icons/radio_station.ico", QVariantMap({ { "name", unescapedName }, { "url", settings.value("url") } }), false, true, false, false);
                        emit explorerDisableQueueable(newId);
                    }
                }
                settings.endArray();
                return;
            }
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_GENRES) || id.startsWith(UI_ID_PREFIX_SERVER_GENRESALPHABET)) {
            QString tagsCacheKey = QString("%1/tags").arg(servers.at(srvIndex)->getSettingsId().toString());

            if (settings.contains(QString("%1/1/id").arg(tagsCacheKey))) {
                QChar   currentChar;
                QString added;

                int tagsSize = settings.beginReadArray(tagsCacheKey);
                for (int i = 0; i < tagsSize; i++) {
                    settings.setArrayIndex(i);

                    QString name     = QTextDocumentFragment::fromHtml(settings.value("name").toString()).toPlainText();
                    QChar   alphabet = alphabetFromName(name);

                    if (id.startsWith(UI_ID_PREFIX_SERVER_GENRES) && (tagsSize > alphabetLimit) && (currentChar != alphabet) && !added.contains(alphabet)) {
                        currentChar = alphabet;
                        added.append(alphabet);
                        QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_GENRESALPHABET).arg(randomGenerator->bounded(std::numeric_limits<quint32>::max())).arg(serverId);
                        emit explorerAddItem(newId, id, currentChar, "qrc:/icons/genre.ico", QVariantMap({{ "alphabet", currentChar }}), true, false, false, false);
                    }
                    if ((id.startsWith(UI_ID_PREFIX_SERVER_GENRESALPHABET) && extra.value("alphabet", "").toString().startsWith(alphabet)) || (id.startsWith(UI_ID_PREFIX_SERVER_GENRES) && (tagsSize <= alphabetLimit))) {
                        QString newId    = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_GENRE, settings.value("id").toString(), serverId);
                        emit explorerAddItem(newId, id, settings.value("name"), "qrc:/icons/genre.ico", QVariantMap({{ "group", settings.value("name")}}), false, true, false, false);
                    }
                }
                settings.endArray();
                return;
            }
        }

        action = globalConstant("action_refresh").toInt();
    }

    if (action == globalConstant("action_collapse")) {
        emit explorerRemoveChildren(id);
        return;
    }

    if (action == globalConstant("action_refresh")) {
        if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEALPHABET)) {
            id = QString("%1|%2").arg(QString(serverId).replace(0, 1, UI_ID_PREFIX_SERVER_BROWSE), serverId);
        }

        explorerNetworkingUISignals(id, true);
        emit explorerRemoveChildren(id);

        if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSE)) {
            servers.at(srvIndex)->startOperation(AmpacheServer::BrowseRoot, AmpacheServer::OpData());
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEARTIST)) {
            servers.at(srvIndex)->startOperation(AmpacheServer::BrowseArtist, {{ "artist", QString(idParts.first()).remove(0, 1) }});
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEALBUM)) {
            servers.at(srvIndex)->startOperation(AmpacheServer::BrowseAlbum, {{ "album", QString(idParts.first()).remove(0, 1) }});
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_PLAYLISTS) || id.startsWith(UI_ID_PREFIX_SERVER_SMARTPLAYLISTS)) {
            QString playlistsId     = QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_PLAYLISTS);
            QString smartPlaylistId = QString(id).replace(0, 1, UI_ID_PREFIX_SERVER_SMARTPLAYLISTS);

            explorerNetworkingUISignals(playlistsId, true);
            explorerNetworkingUISignals(smartPlaylistId, true);
            emit explorerRemoveChildren(playlistsId);
            emit explorerRemoveChildren(smartPlaylistId);

            servers.at(srvIndex)->startOperation(AmpacheServer::PlaylistRoot, AmpacheServer::OpData());
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_RADIOSTATIONS)) {
            servers.at(srvIndex)->startOperation(AmpacheServer::RadioStations, AmpacheServer::OpData());
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_GENRES)) {
            servers.at(srvIndex)->startOperation(AmpacheServer::Tags, AmpacheServer::OpData());
        }
        return;
    }

    if (action == globalConstant("action_play")) {
        if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEARTIST)) {
            bool OK = false;
            int  artistId = QString(idParts.first()).remove(0, 1).toInt(&OK);

            if (OK) {
                startShuffleBatch(srvIndex, extra.value("group").toString(), artistId);
            }
            return;
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEALBUM)) {
            explorerNetworkingUISignals(id, true);
            emit playlistBigBusy(true);

            QObject *opExtra = new QObject();
            opExtra->setProperty("original_action", "action_play");
            opExtra->setProperty("group", extra.value("group").toString());
            if (extra.contains("from_search")) {
                opExtra->setProperty("from_search", extra.value("from_search").toString());
            }

            servers.at(srvIndex)->startOperation(AmpacheServer::BrowseAlbum, {{ "album", QString(idParts.first()).remove(0, 1) }}, opExtra);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSESONG)) {
            playlist.clear();
            Track::TrackInfo trackInfo = trackInfoFromIdExtra(id, extra);
            actionPlay(trackInfo);
            playlistUpdated();
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_PLAYLIST) || id.startsWith(UI_ID_PREFIX_SERVER_SMARTPLAYLIST)) {
            explorerNetworkingUISignals(id, true);
            emit playlistBigBusy(true);

            QObject *opExtra = new QObject();
            opExtra->setProperty("original_action", "action_play");
            opExtra->setProperty("group", extra.value("group").toString());
            if (extra.contains("from_search")) {
                opExtra->setProperty("from_search", extra.value("from_search").toString());
            }

            servers.at(srvIndex)->startOperation(AmpacheServer::PlaylistSongs, {{ "playlist", QString(idParts.first()).remove(0, 1) }}, opExtra);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_RADIOSTATION)) {
            playlist.clear();

            extra.insert("artist", extra.value("name"));
            extra.insert("album", tr("Radio Station"));
            extra.insert("art", "qrc:/icons/radio_station.ico");
            Track::TrackInfo trackInfo = trackInfoFromIdExtra(id, extra);
            trackInfo.attributes.insert("radio_station", "radio_station");

            actionPlay(trackInfo);
            playlistUpdated();
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_SHUFFLE)) {
            startShuffleBatch(srvIndex);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_SHUFFLE_FAVORITES)) {
            startShuffleBatch(srvIndex, tr("Favorites"), 0, Favorite);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_SHUFFLE_NEVERPLAYED)) {
            startShuffleBatch(srvIndex, tr("Never Played"), 0, NeverPlayed);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_SHUFFLE_RECENTLYADDED)) {
            startShuffleBatch(srvIndex, tr("Recently Added"), 0, RecentlyAdded);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_GENRE)) {
            bool OK = false;
            int  tagId = QString(idParts.first()).remove(0, 1).toInt(&OK);
            if (OK) {
                startShuffleBatch(srvIndex, extra.value("group").toString(), 0, None, "action_play", tagId);
            }
        }
        return;
    }

    if ((action == globalConstant("action_playnext")) || (action == globalConstant("action_enqueue")) || (action == globalConstant("action_insert")) || (action == globalConstant("action_enqueueshuffled"))) {
        QString actionStr =
                action == globalConstant("action_playnext") ? "action_playnext" :
                action == globalConstant("action_enqueue")  ? "action_enqueue"  :
                action == globalConstant("action_insert")   ? "action_insert"   :
                                                              "action_enqueueshuffled";
        bool OK = false;
        int destinationIndex = extra.value("destination_index", 0).toInt(&OK);
        if (!OK || (destinationIndex < 0) || (destinationIndex > playlist.count())) {
            destinationIndex = 0;
        }

        if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEARTIST)) {
            bool OK = false;
            int  artistId = QString(idParts.first()).remove(0, 1).toInt(&OK);
            if (OK) {
                startShuffleBatch(srvIndex, extra.value("group").toString(), artistId, None, actionStr, 0, destinationIndex);
            }
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSEALBUM)) {
            explorerNetworkingUISignals(id, true);
            emit playlistBigBusy(true);

            QObject *opExtra = new QObject();
            opExtra->setProperty("original_action", actionStr);
            opExtra->setProperty("group", extra.value("group").toString());
            if (extra.contains("from_search")) {
                opExtra->setProperty("from_search", extra.value("from_search").toString());
            }
            if (destinationIndex > 0) {
                opExtra->setProperty("destination_index", QString("%1").arg(destinationIndex));
            }

            servers.at(srvIndex)->startOperation(AmpacheServer::BrowseAlbum, {{ "album", QString(idParts.first()).remove(0, 1) }}, opExtra);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_BROWSESONG)) {
            if (playlistContains(id)) {
                return;
            }

            Track *track = new Track(trackInfoFromIdExtra(id, extra), peakCallbackInfo, decodingCallbackInfo);
            connectTrackSignals(track);

            if (action == globalConstant("action_enqueue")) {
                playlist.append(track);
            }
            else if (action == globalConstant("action_playnext")) {
                playlist.prepend(track);
            }
            else if (action == globalConstant("action_insert")) {
                playlist.insert(destinationIndex, track);
            }
            else if (action == globalConstant("action_enqueueshuffled")) {
                if (playlist.size() < 1) {
                    playlist.append(track);
                }
                else {
                    playlist.insert(randomGenerator->bounded(playlist.size()), track);
                }
            }

            playlistUpdated();
            playlistSave();
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_PLAYLIST) || id.startsWith(UI_ID_PREFIX_SERVER_SMARTPLAYLIST)) {
            explorerNetworkingUISignals(id, true);
            emit playlistBigBusy(true);

            QObject *opExtra = new QObject();
            opExtra->setProperty("original_action", actionStr);
            opExtra->setProperty("group", extra.value("group").toString());
            if (extra.contains("from_search")) {
                opExtra->setProperty("from_search", extra.value("from_search").toString());
            }
            if (destinationIndex > 0) {
                opExtra->setProperty("destination_index", QString("%1").arg(destinationIndex));
            }

            servers.at(srvIndex)->startOperation(AmpacheServer::PlaylistSongs, {{ "playlist", QString(idParts.first()).remove(0, 1) }}, opExtra);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_SHUFFLE)) {
            startShuffleBatch(srvIndex, "", 0, None, actionStr, 0, destinationIndex);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_SHUFFLE_FAVORITES)) {
            startShuffleBatch(srvIndex, tr("Favorites"), 0, Favorite, actionStr, 0, destinationIndex);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_SHUFFLE_NEVERPLAYED)) {
            startShuffleBatch(srvIndex, tr("Never Played"), 0, NeverPlayed, actionStr, 0, destinationIndex);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_SHUFFLE_RECENTLYADDED)) {
            startShuffleBatch(srvIndex, tr("Recently Added"), 0, RecentlyAdded, actionStr, 0, destinationIndex);
        }
        else if (id.startsWith(UI_ID_PREFIX_SERVER_GENRE)) {
            bool OK = false;
            int  tagId = QString(idParts.first()).remove(0, 1).toInt(&OK);
            if (OK) {
                startShuffleBatch(srvIndex, extra.value("group").toString(), 0, None, actionStr, tagId, destinationIndex);
            }
        }
        return;
    }

    if (action == globalConstant("action_select")) {
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

        delete previousTrack;
        previousTrack = nullptr;
    }
}


void Waver::nextButton()
{
    if (playlist.size() > 0) {
        Track *track = playlist.first();
        playlist.removeFirst();

        actionPlay(track);
    }
}


void Waver::pauseButton()
{
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

    if (peakCallbackCount % peakLagCheckCount == 0) {
        if (peakUILagLastMS > 0) {
            peakFPSMutex.lock();
            peakFPS             = qMax(static_cast<qint64>(1), static_cast<qint64>(round(1000.0 / (1000.0 / peakFPS + peakUILagLastMS))));
            peakLagCheckCount   = qMax(static_cast<int>(1), static_cast<int>(round(333.0 / (1000.0 / peakFPS))));
            peakFPSNextIncrease = peakCallbackCount + static_cast<qint64>(round(500.0 / (1000.0 / peakFPS)));
            peakFPSMutex.unlock();

            peakUILagLastMS = 0;
            peakLagCount    = 0;


        }
    }
    if ((peakFPS < peakFPSMax) && (peakCallbackCount >= peakFPSNextIncrease)) {
        peakFPSMutex.lock();
        peakFPS++;
        peakFPSNextIncrease = peakCallbackCount + static_cast<qint64>(round(150.0 / (1000.0 / peakFPS)));
        peakFPSMutex.unlock();
    }
}


void Waver::peakUILag(int lagMS)
{
    if (lagMS > peakUILagLastMS) {
        peakUILagLastMS = lagMS;
    }
}


void Waver::playButton()
{
    if (currentTrack != nullptr) {
        currentTrack->setStatus(Track::Playing);
        emit notify(PlaybackStatus);
    }
}


QString Waver::playlistAttributeLoad(AmpacheServer *server, QString playlistId, QString attribute)
{
    QSettings settings;
    QString   returnValue;

    QString cacheKey = QString("%1/playlists").arg(server->getSettingsId().toString());

    if (settings.contains(QString("%1/1/id").arg(cacheKey))) {
        int playlistsSize = settings.beginReadArray(cacheKey);
        for (int i = 0; i < playlistsSize; i++) {
            settings.setArrayIndex(i);

            QString id = settings.value("id").toString();
            if (id.compare(playlistId) == 0) {
                returnValue = settings.value(attribute).toString();
                break;
            }
        }
        settings.endArray();
    }

    return returnValue;
}


bool Waver::playlistAttributeSave(AmpacheServer *server, QString playlistId, QString attribute, QString value)
{
    QSettings settings;
    bool      returnValue = false;

    QString cacheKey = QString("%1/playlists").arg(server->getSettingsId().toString());

    if (settings.contains(QString("%1/1/id").arg(cacheKey))) {
        int playlistsSize = settings.beginReadArray(cacheKey);
        for (int i = 0; i < playlistsSize; i++) {
            settings.setArrayIndex(i);

            QString id = settings.value("id").toString();
            if (id.compare(playlistId) == 0) {
                settings.setValue(attribute, value);
                returnValue = true;
                break;
            }
        }
        settings.endArray();
        settings.sync();
    }

    return returnValue;
}


bool Waver::playlistContains(QString id)
{
    if ((currentTrack != nullptr) && !currentTrack->getTrackInfo().id.compare(id)) {
        return true;
    }
    foreach (Track *track, playlist) {
        if (!track->getTrackInfo().id.compare(id)) {
            return true;
        }
    }
    return false;
}


int Waver::playlistLoad()
{
    playlistLoadSongIds.clear();
    playlistLoadTracks.clear();

    QSettings settings;

    int tracksSize = settings.beginReadArray("playlist");
    if (tracksSize < 1) {
        return 0;
    }

    emit uiSetStatusText(tr("Networking"));
    emit playlistBigBusy(true);

    int loaded = 0;
    for (int i = 0; i < tracksSize; i++) {
        settings.setArrayIndex(i);

        QUuid serverSettingsId(settings.value("server_settings_id").toString());

        AmpacheServer *ampServer      = nullptr;
        int            ampServerIndex = 0;
        for(int i = 0; i < servers.count(); i++) {
            AmpacheServer *server = servers.at(i);

            if (server->getSettingsId() == serverSettingsId) {
                ampServer      = server;
                ampServerIndex = i;
                break;
            }
        }
        if (ampServer == nullptr) {
            continue;
        }

        QString newId = QString("%1%2|%3%4").arg(UI_ID_PREFIX_SERVER_PLAYLIST_ITEM).arg(settings.value("track_id").toInt()).arg(UI_ID_PREFIX_SERVER).arg(ampServerIndex);

        QString groupId             = settings.value("group_id").toString();
        QString group               = settings.value("group").toString();
        QString serverPlaylistId    = settings.value("server_playlist_id").toString();
        QString serverPlaylistIndex = settings.value("server_playlist_index").toString();

        QObject *opExtra = new QObject();
        if (!groupId.isEmpty() && !group.isEmpty()) {
            opExtra->setProperty("group_id", settings.value("group_id"));
            opExtra->setProperty("group", settings.value("group"));
        }
        if (!serverPlaylistId.isEmpty() && !serverPlaylistIndex.isEmpty()) {
            opExtra->setProperty("server_playlist_id", settings.value("server_playlist_id"));
            opExtra->setProperty("server_playlist_index", settings.value("server_playlist_index"));
        }

        playlistLoadSongIds.append(newId);
        ampServer->startOperation(AmpacheServer::Song, {{ "song_id", settings.value("track_id").toString() }}, opExtra);

        loaded++;
    }
    settings.endArray();

    return loaded;
}


void Waver::playlistSave()
{
    QSettings settings;
    settings.remove("playlist");

    QList<Track *> toBeSaved;

    if (currentTrack != nullptr) {
        toBeSaved.append(currentTrack);
    }
    toBeSaved.append(playlist);

    if (toBeSaved.size() < 1) {
        return;
    }

    settings.beginWriteArray("playlist");
    for (int i = 0; i < toBeSaved.count(); i++) {
        settings.setArrayIndex(i);

        Track::TrackInfo trackInfo = toBeSaved.at(i)->getTrackInfo();

        QStringList idParts  = trackInfo.id.split("|");
        QString     serverId = idParts.last();
        int         srvIndex = serverIndex(serverId);

        if (srvIndex < 0) {
            continue;
        }

        QString settingsId = servers.at(srvIndex)->getSettingsId().toString();
        QString trackId    = idParts.at(0).mid(1);

        settings.setValue("server_settings_id", settingsId);
        settings.setValue("track_id", trackId);
        settings.setValue("group_id", trackInfo.attributes.value("group_id", ""));
        settings.setValue("group", trackInfo.attributes.value("group", ""));
        settings.setValue("server_playlist_id", trackInfo.attributes.value("server_playlist_id", ""));
        settings.setValue("server_playlist_index", trackInfo.attributes.value("server_playlist_index", ""));
    }
    settings.endArray();
    settings.sync();
}


void Waver::playlistItemClicked(int index, int action)
{
    if (action == globalConstant("action_shuffle_playlist")) {
        QRandomGenerator *randomGenerator = QRandomGenerator::global();

        for (int i = 0; i < playlist.count(); i++) {
            int    index = randomGenerator->bounded(playlist.count());
            Track *swap  = playlist.at(index);
            playlist[index] = playlist.at(i);
            playlist[i]     = swap;
        }

        emit playlistClearItems();
        foreach (Track *track, playlist) {
            Track::TrackInfo trackInfo = track->getTrackInfo();
            emit playlistAddItem(trackInfo.title, trackInfo.artist, trackInfo.attributes.contains("group") ? trackInfo.attributes.value("group") : "", trackInfo.arts.first().toString(), trackInfo.attributes.contains("playlist_selected"), trackURL(trackInfo.id).toString());
        }
    }

    if (action == globalConstant("action_play")) {
        Track *track = playlist.at(index);
        playlist.removeAt(index);
        actionPlay(track);
        return;
    }

    if (action == globalConstant("action_move_to_top")) {
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
        playlistUpdated();
        playlistSave();
        return;
    }

    if (action == globalConstant("action_remove")) {
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
        playlistUpdated();
        playlistSave();
        return;
    }

    if (action == globalConstant("action_select")) {
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
    Track *track = playlist.at(index);
    playlist.removeAt(index);
    playlist.insert(destinationIndex, track);
    playlistUpdated();
    playlistSave();
}


void Waver::playlistExplorerItemDragDropped(QString id, QString extraJSON, int destinationIndex)
{
    bool OK = false;
    int actionInsert = globalConstant("action_insert").toInt(&OK);

    if (OK) {
        QVariantMap extra = QJsonDocument::fromJson(extraJSON.toUtf8()).toVariant().toMap();
        extra.insert("destination_index", destinationIndex);
        itemActionServerItem(id, actionInsert, extra);
    }
}


void Waver::playlistUpdated()
{
    qint64 totalMilliSeconds = 0;
    bool   totalIsEstimate   = false;

    emit playlistBigBusy(false);
    emit playlistClearItems();

    for (int i = 0; i < playlist.count(); i++) {
        CrossfadeMode  crossfadeMode = PlayNormal;
        Track         *compareTo     = currentTrack;
        if (i > 0) {
            compareTo = playlist.at(i - 1);
        }
        crossfadeMode = isCrossfade(compareTo, playlist.at(i));
        compareTo->setShortFadeEnd(crossfadeMode == ShortCrossfade);
        playlist.at(i)->setShortFadeBeginning(crossfadeMode == ShortCrossfade);

        Track::TrackInfo trackInfo = playlist.at(i)->getTrackInfo();
        emit playlistAddItem(trackInfo.title, trackInfo.artist, trackInfo.attributes.contains("group") ? trackInfo.attributes.value("group") : "", trackInfo.arts.first().toString(), trackInfo.attributes.contains("playlist_selected"), trackURL(trackInfo.id).toString());

        qint64 milliseconds = playlist.at(i)->getLengthMilliseconds();
        if (milliseconds > 0) {
            totalMilliSeconds += milliseconds;
        }
        else {
            totalIsEstimate = true;
        }

        playlist.at(i)->requestDecodingCallback();
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
    if (currentTrack == nullptr) {
        return;
    }
    currentTrack->setPosition(percent);
}


void Waver::ppButton()
{
    Track::Status status = getCurrentTrackStatus();

    if (status == Track::Playing) {
        pauseButton();
    }
    if (status == Track::Paused) {
        playButton();
    }
}


void Waver::previousButton(int index)
{
    if (history.count() <= index) {
        return;
    }

    actionPlay(history.at(index));
    index++;

    int i = 0;
    while ((i < index) && (i < history.count())) {
        Track *track = new Track(history.at(i), peakCallbackInfo, decodingCallbackInfo);
        connectTrackSignals(track);
        playlist.prepend(track);
        i++;
    }
    playlistUpdated();
    playlistSave();

    history.remove(0, index + 1);
    emit uiHistoryRemove(index + 1);
}


void Waver::raiseButton()
{
    emit uiRaise();
}


void Waver::requestEQ(int eqChooser)
{
    QSettings settings;

    QString eqChooserPrefix = "eq";
    if (eqChooser != Track::EQ_CHOOSER_COMMON) {
        eqChooserPrefix = "";
        if (currentTrack != nullptr) {
            Track::TrackInfo currentTrackInfo = getCurrentTrackInfo();

            QStringList idParts  = currentTrackInfo.id.split("|");
            QString     serverId = idParts.last();
            int         srvIndex = serverIndex(serverId);

            QString settingsId = servers.at(srvIndex)->getSettingsId().toString();
            QString trackId    = idParts.at(0).mid(1);

            if ((eqChooser == Track::EQ_CHOOSER_TRACK) && settings.contains(QString("%1/track/%2/eq/pre_amp").arg(settingsId, trackId))) {
                eqChooserPrefix = QString("%1/track/%2/eq").arg(settingsId, trackId);
            }
            else if ((eqChooser == Track::EQ_CHOOSER_ALBUM) && settings.contains(QString("%1/album/%2/eq/pre_amp").arg(settingsId, currentTrackInfo.albumId))) {
                eqChooserPrefix = QString("%1/album/%2/eq").arg(settingsId, currentTrackInfo.albumId);
            }
        }
    }
    if (eqChooserPrefix.length() == 0) {
        return;
    }

    QVariantMap eqObj;
    eqObj.insert("pre_amp", settings.value(eqChooserPrefix + "/pre_amp", DEFAULT_PREAMP));
    eqObj.insert("eq1", settings.value(eqChooserPrefix + "/eq1", DEFAULT_EQ1));
    eqObj.insert("eq2", settings.value(eqChooserPrefix + "/eq2", DEFAULT_EQ2));
    eqObj.insert("eq3", settings.value(eqChooserPrefix + "/eq3", DEFAULT_EQ3));
    eqObj.insert("eq4", settings.value(eqChooserPrefix + "/eq4", DEFAULT_EQ4));
    eqObj.insert("eq5", settings.value(eqChooserPrefix + "/eq5", DEFAULT_EQ5));
    eqObj.insert("eq6", settings.value(eqChooserPrefix + "/eq6", DEFAULT_EQ6));
    eqObj.insert("eq7", settings.value(eqChooserPrefix + "/eq7", DEFAULT_EQ7));
    eqObj.insert("eq8", settings.value(eqChooserPrefix + "/eq8", DEFAULT_EQ8));
    eqObj.insert("eq9", settings.value(eqChooserPrefix + "/eq9", DEFAULT_EQ9));
    eqObj.insert("eq10", settings.value(eqChooserPrefix + "/eq10", DEFAULT_EQ10));

    emit eqAsRequested(eqObj);
}


void Waver::requestOptions()
{
    QVariantMap optionsObj;
    QSettings   settings;

    if (currentTrack == nullptr) {
        optionsObj.insert("eq_disable", 1);
    }
    else {
        optionsObj.insert("eq_disable", 0);

        QVector<double> eqCenterFrequencies = currentTrack->getEqualizerBandCenterFrequencies();

        int     eqChooser       = Track::EQ_CHOOSER_COMMON;
        QString eqChooserPrefix = "eq";
        if (currentTrack != nullptr) {
            Track::TrackInfo currentTrackInfo = getCurrentTrackInfo();

            QStringList idParts  = currentTrackInfo.id.split("|");
            QString     serverId = idParts.last();
            int         srvIndex = serverIndex(serverId);

            QString settingsId = servers.at(srvIndex)->getSettingsId().toString();
            QString trackId    = idParts.at(0).mid(1);

            if (settings.contains(QString("%1/track/%2/eq/pre_amp").arg(settingsId, trackId))) {
                eqChooser       = Track::EQ_CHOOSER_TRACK;
                eqChooserPrefix = QString("%1/track/%2/eq").arg(settingsId, trackId);
            }
            else if (settings.contains(QString("%1/album/%2/eq/pre_amp").arg(settingsId, currentTrackInfo.albumId))) {
                eqChooser       = Track::EQ_CHOOSER_ALBUM;
                eqChooserPrefix = QString("%1/album/%2/eq").arg(settingsId, currentTrackInfo.albumId);
            }
        }
        optionsObj.insert("eq_chooser", eqChooser);
        optionsObj.insert("eq_on", settings.value("eq/on", DEFAULT_EQON).toBool());

        optionsObj.insert("pre_amp", settings.value(eqChooserPrefix + "/pre_amp", DEFAULT_PREAMP));
        optionsObj.insert("eq1Label", formatFrequencyValue(eqCenterFrequencies.at(0)));
        optionsObj.insert("eq1", settings.value(eqChooserPrefix + "/eq1", DEFAULT_EQ1));
        optionsObj.insert("eq2Label", formatFrequencyValue(eqCenterFrequencies.at(1)));
        optionsObj.insert("eq2", settings.value(eqChooserPrefix + "/eq2", DEFAULT_EQ2));
        optionsObj.insert("eq3Label", formatFrequencyValue(eqCenterFrequencies.at(2)));
        optionsObj.insert("eq3", settings.value(eqChooserPrefix + "/eq3", DEFAULT_EQ3));
        optionsObj.insert("eq4Label", formatFrequencyValue(eqCenterFrequencies.at(3)));
        optionsObj.insert("eq4", settings.value(eqChooserPrefix + "/eq4", DEFAULT_EQ4));
        optionsObj.insert("eq5Label", formatFrequencyValue(eqCenterFrequencies.at(4)));
        optionsObj.insert("eq5", settings.value(eqChooserPrefix + "/eq5", DEFAULT_EQ5));
        optionsObj.insert("eq6Label", formatFrequencyValue(eqCenterFrequencies.at(5)));
        optionsObj.insert("eq6", settings.value(eqChooserPrefix + "/eq6", DEFAULT_EQ6));
        optionsObj.insert("eq7Label", formatFrequencyValue(eqCenterFrequencies.at(6)));
        optionsObj.insert("eq7", settings.value(eqChooserPrefix + "/eq7", DEFAULT_EQ7));
        optionsObj.insert("eq8Label", formatFrequencyValue(eqCenterFrequencies.at(7)));
        optionsObj.insert("eq8", settings.value(eqChooserPrefix + "/eq8", DEFAULT_EQ8));
        optionsObj.insert("eq9Label", formatFrequencyValue(eqCenterFrequencies.at(8)));
        optionsObj.insert("eq9", settings.value(eqChooserPrefix + "/eq9", DEFAULT_EQ9));
        optionsObj.insert("eq10Label", formatFrequencyValue(eqCenterFrequencies.at(9)));
        optionsObj.insert("eq10", settings.value(eqChooserPrefix + "/eq10", DEFAULT_EQ10));
    }

    optionsObj.insert("shuffle_autostart", settings.value("options/shuffle_autostart", DEFAULT_SHUFFLE_AUTOSTART).toBool());
    optionsObj.insert("shuffle_delay_seconds", settings.value("options/shuffle_delay_seconds", DEFAULT_SHUFFLE_DELAY_SECONDS));
    optionsObj.insert("shuffle_count", settings.value("options/shuffle_count", DEFAULT_SHUFFLE_COUNT));
    optionsObj.insert("random_lists_count", settings.value("options/random_lists_count", DEFAULT_RANDOM_LISTS_COUNT));
    optionsObj.insert("recently_added_count", settings.value("options/recently_added_count", DEFAULT_RECENTLY_ADDED_COUNT));
    optionsObj.insert("recently_added_days", settings.value("options/recently_added_days", DEFAULT_RECENTLY_ADDED_DAYS));
    optionsObj.insert("shuffle_favorite_frequency", settings.value("options/shuffle_favorite_frequency", DEFAULT_SHUFFLE_FAVORITE_FREQUENCY));
    optionsObj.insert("shuffle_recently_added_frequency", settings.value("options/shuffle_recently_added_frequency", DEFAULT_SHUFFLE_RECENT_FREQUENCY));
    optionsObj.insert("shuffle_operator", settings.value("options/shuffle_operator", DEFAULT_SHUFFLE_OPERATOR));

    optionsObj.insert("search_count_max", settings.value("options/search_count_max", DEFAULT_SEARCH_COUNT_MAX));
    optionsObj.insert("search_action", settings.value("options/search_action", DEFAULT_SEARCH_ACTION));
    optionsObj.insert("search_action_filter", settings.value("options/search_action_filter", DEFAULT_SEARCH_ACTION_FILTER));
    optionsObj.insert("search_action_count_max", settings.value("options/search_action_count_max", DEFAULT_SEARCH_ACTION_COUNT_MAX));

    optionsObj.insert("auto_refresh", settings.value("options/auto_refresh", DEFAULT_AUTO_REFRESH).toBool());
    optionsObj.insert("hide_dot_playlists", settings.value("options/hide_dot_playlists", DEFAULT_HIDE_DOT_PLAYLIST).toBool());
    optionsObj.insert("title_curly_special", settings.value("options/title_curly_special", DEFAULT_TITLE_CURLY_SPECIAL).toBool());
    optionsObj.insert("starting_index_apply", settings.value("options/starting_index_apply", DEFAULT_STARTING_INDEX_APPLY).toBool());
    optionsObj.insert("starting_index_days", settings.value("options/starting_index_days", DEFAULT_STARTING_INDEX_DAYS));
    optionsObj.insert("alphabet_limit", settings.value("options/alphabet_limit", DEFAULT_ALPHABET_LIMIT));
    optionsObj.insert("font_size", settings.value("options/font_size", DEFAULT_FONT_SIZE));
    optionsObj.insert("wide_stereo", settings.value("options/wide_stereo_delay_millisec", DEFAULT_WIDE_STEREO_DELAY_MILLISEC));
    optionsObj.insert("skip_long_silence", settings.value("options/skip_long_silence", DEFAULT_SKIP_LONG_SILENCE).toBool());
    optionsObj.insert("skip_long_silence_seconds", settings.value("options/skip_long_silence_seconds", DEFAULT_SKIP_LONG_SILENCE_SECONDS));

    optionsObj.insert("fade_tags", settings.value("options/fade_tags", DEFAULT_FADE_TAGS));
    optionsObj.insert("crossfade_tags", settings.value("options/crossfade_tags", DEFAULT_CROSSFADE_TAGS));
    optionsObj.insert("fade_seconds", settings.value("options/fade_seconds", DEFAULT_FADE_SECONDS).toInt());

    optionsObj.insert("max_peak_fps", settings.value("options/max_peak_fps", DEFAULT_MAX_PEAK_FPS));
    optionsObj.insert("peak_delay_on", settings.value("options/peak_delay_on", DEFAULT_PEAK_DELAY_ON).toBool());
    optionsObj.insert("peak_delay_ms", settings.value("options/peak_delay_ms", DEFAULT_PEAK_DELAY_MS));

    QVariantList genres;
    QStringList  added;
    for (int srvIndex = 0; srvIndex < servers.size(); srvIndex++) {
        QString tagsCacheKey = QString("%1/tags").arg(servers.at(srvIndex)->getSettingsId().toString());

        if (settings.contains(QString("%1/1/id").arg(tagsCacheKey))) {
            int tagsSize = settings.beginReadArray(tagsCacheKey);
            for (int i = 0; i < tagsSize; i++) {
                settings.setArrayIndex(i);

                int     id   = settings.value("id").toInt();
                QString name = QTextDocumentFragment::fromHtml(settings.value("name").toString()).toPlainText();

                if (added.contains(name)) {
                    continue;
                }

                added.append(name);
                genres.append(QVariantMap({{ "title", name }, { "selected", servers.at(srvIndex)->isShuffleTagSelected(id) }}));
            }
            settings.endArray();
        }
    }
    std::sort(genres.begin(), genres.end(), [](QVariant a, QVariant b) {
        return a.toMap().value("title", "").toString().compare(b.toMap().value("title", "").toString(), Qt::CaseInsensitive) < 0;
    });

    optionsObj.insert("genres", genres);

    emit optionsAsRequested(optionsObj);
}


void Waver::run()
{
    QSettings settings;

    emit uiSetIsSnap(QGuiApplication::instance()->applicationDirPath().contains("snap", Qt::CaseInsensitive));
    emit uiSetFontSize(settings.value("options/font_size", DEFAULT_FONT_SIZE));
    emit uiSetTitleCurlySpecial(settings.value("options/title_curly_special", DEFAULT_TITLE_CURLY_SPECIAL).toBool());

    shuffleCountdownTimer = new QTimer();
    shuffleCountdownTimer->setInterval(1000);
    connect(shuffleCountdownTimer, &QTimer::timeout, this, &Waver::shuffleCountdown);

    emit explorerAddItem(QString("%1A").arg(UI_ID_PREFIX_SEARCH), QVariant::fromValue(nullptr), tr("Search"), "qrc:/icons/search.ico", QVariantMap({}), false, false, false, false);


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
        emit explorerAddItem(id, QVariant::fromValue(nullptr), QFileInfo(localDirs.at(i)).baseName(), "qrc:/icons/local.ico", QVariantMap({{ "path", path }}), true, false, false, false);
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

        connect(servers.at(i), &AmpacheServer::operationFinished, this, &Waver::serverOperationFinished);
        connect(servers.at(i), &AmpacheServer::passwordNeeded,    this, &Waver::serverPasswordNeeded);
        connect(servers.at(i), &AmpacheServer::errorMessage,      this, &Waver::errorMessage);

        emit explorerAddItem(id, QVariant::fromValue(nullptr), servers.at(i)->formattedName(), "qrc:/icons/remote.ico", QVariantMap({}), true, false, false, false);
    }

    autoRefreshLastActionTimestamp = QDateTime::currentMSecsSinceEpoch() + settings.value("options/shuffle_delay_seconds", DEFAULT_SHUFFLE_DELAY_SECONDS).toInt() * 1000 + 5000;
    autoRefreshLastDay             = settings.value("auto_refresh/last_day", 0).toInt();
    autoRefreshLastServerIndex     = settings.value("auto_refresh/last_server_index", 0).toInt();
    autoRefreshLastItem            = settings.value("auto_refresh/last_item", 0).toInt();
    if (autoRefreshLastItem > AUTO_REFRESH_GENRES) {
        autoRefreshLastItem = 0;
        autoRefreshLastServerIndex++;
    }
    if (autoRefreshLastServerIndex >= servers.count()) {
        autoRefreshLastServerIndex = 0;
    }
    autoRefresh = settings.value("options/auto_refresh", DEFAULT_AUTO_REFRESH).toBool();

    if (servers.count() > 0) {
        startShuffleCountdown();
    }
}


void Waver::searchAction()
{
    QSettings settings;

    if (settings.value("options/search_action", DEFAULT_SEARCH_ACTION).toInt() == globalConstant("search_action_none").toInt()) {
        return;
    }
    int searchActionCountMax = settings.value("options/search_action_count_max", DEFAULT_SEARCH_ACTION_COUNT_MAX).toInt();
    int searchActionFilter   = settings.value("options/search_action_filter", DEFAULT_SEARCH_ACTION_FILTER).toInt();

    foreach (QString query, searchQueries.keys()) {
        QString explorerId = searchQueries.value(query);
        if (!searchActionItemsCounter.contains(explorerId)) {
            searchActionItemsCounter.insert(explorerId, { query, 0, 0, searchActionFilter == globalConstant("search_action_filter_reductive").toInt() ? globalConstant("search_action_filter_exactmatch").toInt() : searchActionFilter, SEARCH_TARGET_SONGS });
        }
        if ((searchActionItemsCounter.value(explorerId).doneCounter >= 0) && ((searchActionItemsCounter.value(explorerId).doneCounter < searchActionCountMax) || (searchActionCountMax == 0))) {
            int index = searchActionItemsCounter.value(explorerId).nextExplorerResultIndex;
            searchActionItemsCounter[explorerId].nextExplorerResultIndex++;
            emit explorerGetSearchResult(explorerId, index);
        }
    }
}


void Waver::searchCaches(QString parent, QString criteria)
{
    QSettings settings;

    foreach (AmpacheServer *server, servers) {
        QString browseCacheKey    = QString("%1/browse").arg(server->getSettingsId().toString());
        QString playlistsCacheKey = QString("%1/playlists").arg(server->getSettingsId().toString());
        bool    hideDotPlaylists  = settings.value("options/hide_dot_playlists", DEFAULT_HIDE_DOT_PLAYLIST).toBool();

        if (settings.contains(QString("%1/1/id").arg(browseCacheKey))) {
            int browseSize = settings.beginReadArray(browseCacheKey);
            for (int i = 0; i < browseSize; i++) {
                settings.setArrayIndex(i);

                QString name = settings.value("name").toString();

                if (name.contains(criteria, Qt::CaseInsensitive)) {
                    QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SEARCHRESULT_ARTIST, settings.value("id").toString(), server->getId());
                    emit explorerAddItem(newId, parent, QString("<i>%1</i>").arg(name), settings.value("art").toString(), QVariantMap({{ "art", settings.value("art").toString() }, { "group", name }, { "from_search", "from_search" }}), false, true, false, false);
                }
            }
            settings.endArray();
        }

        if (settings.contains(QString("%1/1/id").arg(playlistsCacheKey))) {
            int playlistsSize = settings.beginReadArray(playlistsCacheKey);
            for (int i = 0; i < playlistsSize; i++) {
                settings.setArrayIndex(i);

                QString playlistName = settings.value("name").toString();

                if (playlistName.startsWith(".") && hideDotPlaylists) {
                    continue;
                }

                if (playlistName.contains(criteria, Qt::CaseInsensitive)) {

                    QString art = settings.value("art", "").toString();
                    if (art.isEmpty() || settings.value("id").toString().startsWith("smart")) {
                        art ="qrc:/icons/playlist.ico";
                    }

                    QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SEARCHRESULT_PLAYLIST, settings.value("id").toString(), server->getId());
                    emit explorerAddItem(newId, parent, QString("<i>%1</i>").arg(playlistName), art, QVariantMap({{ "group", playlistName }, { "from_search", "from_search" }, { "art", art }}), false, true, false, false);
                }
            }
            settings.endArray();
        }
    }
}


void Waver::searchCriteriaEntered(QString criteria)
{
    foreach (QString key, searchQueries.keys()) {
        emit explorerRemoveChildren(searchQueries.value(key));
    }

    QString parentId;
    if (searchQueries.contains(criteria)) {
        parentId = searchQueries.value(criteria);
    }
    else {
        searchQueriesCounter++;
        parentId = QString("%1%2").arg(UI_ID_PREFIX_SEARCHQUERY).arg(searchQueriesCounter);

        QList<QString> keys = searchQueries.keys();
        keys.sort();
        while ((searchQueries.size() >= SEARCH_MAX_QUERIES) && keys.size()) {
            emit explorerRemoveItem(searchQueries.value(keys.first()));
            searchQueries.remove(keys.first());
            keys.removeFirst();
        }
        searchQueries.insert(criteria, parentId);

        emit explorerAddItem(parentId, QString("%1A").arg(UI_ID_PREFIX_SEARCH), criteria, "qrc:/icons/search.ico", QVariantMap({{ "criteria", criteria }}), false, false, false, false);
        emit explorerSortChildren(QString("%1A").arg(UI_ID_PREFIX_SEARCH));
    }

    searchResultsCounter = 0;

    searchServers(parentId, criteria);
    searchLocalDirs(parentId, criteria);
    searchCaches(parentId, criteria);
}


void Waver::searchLocalDirs(QString parent, QString criteria)
{
    QSettings settings;

    foreach(QDir localDir, localDirs) {
        FileSearcher *fileSearcher = new FileSearcher(localDir.absolutePath(), criteria, parent);
        connect(fileSearcher, &FileSearcher::finished, this, &Waver::fileSearchFinished);
        fileSearchers.append(fileSearcher);

        fileSearcher->start();
    }
}


void Waver::searchResult(QString parentId, QString id, QString extraJSON)
{
    QSettings settings;

    if (id.isEmpty()) {
        int searchActionFilter = settings.value("options/search_action_filter", DEFAULT_SEARCH_ACTION_FILTER).toInt();
        if (searchActionFilter == globalConstant("search_action_filter_reductive")) {
            if (searchActionItemsCounter.value(parentId).filter == globalConstant("search_action_filter_exactmatch")) {
                searchActionItemsCounter[parentId].filter = globalConstant("search_action_filter_startswith").toInt();
                searchActionItemsCounter[parentId].nextExplorerResultIndex = 0;
                searchAction();
                return;
            }
            if (searchActionItemsCounter.value(parentId).filter == globalConstant("search_action_filter_startswith")) {
                searchActionItemsCounter[parentId].filter = globalConstant("search_action_filter_none").toInt();
                searchActionItemsCounter[parentId].nextExplorerResultIndex = 0;
                searchAction();
                return;
            }
        }

        if (searchActionItemsCounter.value(parentId).target == SEARCH_TARGET_SONGS) {
            searchActionItemsCounter[parentId].target                  = SEARCH_TARGET_REST;
            searchActionItemsCounter[parentId].nextExplorerResultIndex = 0;
            searchAction();
            return;
        }

        searchActionItemsCounter[parentId].doneCounter = -1;
        return;
    }

    if ((searchActionItemsCounter.value(parentId).target == SEARCH_TARGET_SONGS) && !id.startsWith(UI_ID_PREFIX_LOCALDIR_FILE) && !id.startsWith(UI_ID_PREFIX_SEARCHRESULT)) {
        searchAction();
        return;
    }
    if ((searchActionItemsCounter.value(parentId).target == SEARCH_TARGET_REST) && (id.startsWith(UI_ID_PREFIX_LOCALDIR_FILE) || id.startsWith(UI_ID_PREFIX_SEARCHRESULT))) {
        searchAction();
        return;
    }

    int searchActionAction = settings.value("options/search_action", DEFAULT_SEARCH_ACTION).toInt();
    if (searchActionAction == globalConstant("search_action_none").toInt()) {
        return;
    }

    int action = 0;
    if (searchActionAction == globalConstant("search_action_play").toInt()) {
        action = globalConstant("action_play").toInt();
    }
    else if (searchActionAction == globalConstant("search_action_playnext").toInt()) {
        action = globalConstant("action_playnext").toInt();
    }
    else if (searchActionAction == globalConstant("search_action_enqueue").toInt()) {
        action = globalConstant("action_enqueue").toInt();
    }
    else if (searchActionAction == globalConstant("search_action_randomize").toInt()) {
        action = globalConstant("action_enqueueshuffled").toInt();
    }
    assert(action);

    if (searchActionItemsCounter.contains(parentId)) {
        if (!searchActionItemsCounter.value(parentId).doneCounter && (currentTrack == nullptr) && (action != globalConstant("action_play").toInt())) {
            action = globalConstant("action_play").toInt();
        }
        if (searchActionItemsCounter.value(parentId).doneCounter && (action == globalConstant("action_play").toInt())) {
            action = globalConstant("search_action_playnext").toInt();
        }
    }

    QVariantMap extra = QJsonDocument::fromJson(extraJSON.toUtf8()).toVariant().toMap();
    extra.insert("search_action_parent_id", parentId);

    explorerItemClicked(id, action, QJsonDocument::fromVariant(extra).toJson());
}


void Waver::searchServers(QString parent, QString criteria)
{
    explorerNetworkingUISignals(parent, true);
    searchOperationsCounter = 0;
    foreach (AmpacheServer *server, servers) {
        searchOperationsCounter += 2;

        QObject *opExtra = new QObject();
        opExtra->setProperty("parent", QString(parent));
        server->startOperation(AmpacheServer::Search, {{ "criteria", criteria }}, opExtra);
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
    int       startingIndex = 0;

    int searchMaxCount       = settings.value("options/search_count_max", DEFAULT_SEARCH_COUNT_MAX).toInt();
    int searchActionCountMax = settings.value("options/search_action_count_max", DEFAULT_SEARCH_ACTION_COUNT_MAX).toInt();

    QString originalAction = opData.contains("original_action") ? opData.value("original_action") : "";
    QString group          = opData.contains("group")           ? opData.value("group")           : "";
    QUuid   groupId        = QUuid::createUuid();

    bool OK = false;
    int destinationIndex = opData.value("destination_index", 0).toInt(&OK);
    if (!OK || (destinationIndex < 0) || (destinationIndex > playlist.count())) {
        destinationIndex = 0;
    }

    int i = 0;
    while (i < opResults.size() - 1) {
        int j = i + 1;
        while (j < opResults.size()) {
            if (opResults.value(i).value("id", "-").compare(opResults.value(j).value("id", "+")) == 0) {
                opResults.removeAt(j);
                continue;
            }
            j++;
        }
        i++;
    }

    bool allTheSameArtist = true;
    if (opResults.size() > 1) {
        QString allTheSameArtistName = opResults.at(0).value("artist", "");
        for(int i = 1; i < opResults.size(); i++) {
            if (opResults.at(i).value("artist", "").compare(allTheSameArtistName)) {
                allTheSameArtist = false;
                break;
            }
        }
    }

    QRandomGenerator *randomGenerator = QRandomGenerator::global();

    if (opResults.count() <= 0) {
        if (opData.contains("count_flagged")) {
            if (servers.at(srvIndex)->getServerVersion() == 5000000) {
                errorMessage(servers.at(srvIndex)->getId(), tr("No favorite tracks can be found. Favorites do not work with Ampache server version 5.0.0, update recommended."), "");
            }
            else {
                errorMessage(servers.at(srvIndex)->getId(), tr("No favorite tracks can be found"), "");
            }
        }
        else {
            errorMessage(servers.at(srvIndex)->getId(), tr("The Ampache server [%1] returned an empty result set").arg(servers.at(srvIndex)->formattedName()), tr("No error occured, but the results are empty"));
        }
    }

    if ((opCode == AmpacheServer::Search) || (opCode == AmpacheServer::SearchAlbums)) {
        parentId = opData.value("parent");

        std::sort(opResults.begin(), opResults.end(), [](AmpacheServer::OpData a, AmpacheServer::OpData b) {
            return a.value("name").compare(b.value("name"), Qt::CaseInsensitive) < 0;
        });
    }
    else if (opCode == AmpacheServer::BrowseRoot) {
        cacheKey = QString("%1/browse").arg(servers.at(srvIndex)->getSettingsId().toString());

        settings.remove(cacheKey);
        settings.beginWriteArray(cacheKey);
    }
    else if (opCode == AmpacheServer::BrowseArtist) {
        parentId = QString("%1%2|%3").arg(opData.contains("from_search") ? UI_ID_PREFIX_SEARCHRESULT_ARTIST : UI_ID_PREFIX_SERVER_BROWSEARTIST, opData.value("artist"), opData.value("serverId"));

        std::sort(opResults.begin(), opResults.end(), [](AmpacheServer::OpData a, AmpacheServer::OpData b) {
            return (a.value("year").toInt() < b.value("year").toInt()) || ((a.value("year").toInt() == b.value("year").toInt()) && (a.value("name").compare(b.value("name"), Qt::CaseInsensitive) < 0));
        });
    }
    else if (opCode == AmpacheServer::BrowseAlbum) {
        parentId = QString("%1%2|%3").arg(opData.contains("from_search") ? UI_ID_PREFIX_SEARCHRESULT_ALBUM : UI_ID_PREFIX_SERVER_BROWSEALBUM, opData.value("album"), opData.value("serverId"));

        std::sort(opResults.begin(), opResults.end(), [](AmpacheServer::OpData a, AmpacheServer::OpData b) {
            return (a.value("track").toInt() < b.value("track").toInt()) || ((a.value("track").toInt() == b.value("track").toInt()) && (a.value("title").compare(b.value("title"), Qt::CaseInsensitive) < 0));
        });
    }
    else if (opCode == AmpacheServer::PlaylistRoot) {
        cacheKey = QString("%1/playlists").arg(servers.at(srvIndex)->getSettingsId().toString());

        settings.remove(cacheKey);
        settings.beginWriteArray(cacheKey);
    }
    else if (opCode == AmpacheServer::PlaylistSongs) {
        parentId      = QString("%1%2|%3").arg(opData.contains("from_search") ? UI_ID_PREFIX_SEARCHRESULT_PLAYLIST : opData.value("playlist").startsWith("smart", Qt::CaseInsensitive) ? UI_ID_PREFIX_SERVER_SMARTPLAYLIST : UI_ID_PREFIX_SERVER_PLAYLIST, opData.value("playlist"), opData.value("serverId"));

        if (settings.value("options/starting_index_apply", DEFAULT_STARTING_INDEX_APPLY).toBool()) {
            bool OK;

            qint64 startingIndexDays = settings.value("options/starting_index_days", DEFAULT_STARTING_INDEX_DAYS).toLongLong(&OK);
            if (OK & (startingIndexDays >= 0)) {
                int lastPlayedIndex = playlistAttributeLoad(servers.at(srvIndex), opData.value("playlist"), "last_played_index").toInt(&OK);
                if (OK) {
                    lastPlayedIndex++;
                    if ((lastPlayedIndex >= 0) && (lastPlayedIndex < opResults.size())) {
                        qint64 last_played_time = playlistAttributeLoad(servers.at(srvIndex), opData.value("playlist"), "last_played_time").toLongLong(&OK);
                        if (OK && (QDateTime::currentMSecsSinceEpoch() - last_played_time < startingIndexDays * 24 * 60 * 60 * 1000)) {
                            startingIndex = lastPlayedIndex;
                            playlistAttributeSave(servers.at(srvIndex), opData.value("playlist"), "last_played_time", "0");
                        }
                    }
                }
            }
        }
    }
    else if (opCode == AmpacheServer::RadioStations) {
        cacheKey = QString("%1/radiostations").arg(servers.at(srvIndex)->getSettingsId().toString());

        settings.remove(cacheKey);
        settings.beginWriteArray(cacheKey);
    }
    else if (opCode == AmpacheServer::Shuffle) {
        parentId = QString("%1|%2").arg(QString(opData.value("serverId")).replace(0, 1, UI_ID_PREFIX_SERVER_SHUFFLE), opData.value("serverId"));
    }
    else if (opCode == AmpacheServer::Tags) {
        cacheKey = QString("%1/tags").arg(servers.at(srvIndex)->getSettingsId().toString());

        settings.remove(cacheKey);
        settings.beginWriteArray(cacheKey);
    }

    for(int i = 0; i < opResults.size(); i++) {
        AmpacheServer::OpData result = opResults.at(i);

        if (opData.contains("search_action_parent_id") && searchActionItemsCounter.contains(opData.value("search_action_parent_id"))) {
            searchActionItemsCounter[opData.value("search_action_parent_id")].doneCounter++;
            if ((searchActionCountMax > 0) && (searchActionItemsCounter.value(opData.value("search_action_parent_id")).doneCounter > searchActionCountMax)) {
                break;
            }
        }

        if (opCode == AmpacheServer::Search) {
            if ((searchResultsCounter < searchMaxCount) || (searchMaxCount == 0)) {
                QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SEARCHRESULT, result.value("id"), opData.value("serverId"));

                QVariantMap trackInfoMap;
                QStringList keys = result.keys();
                foreach(QString key, keys) {
                    trackInfoMap.insert(key, result.value(key));
                }
                trackInfoMap.insert("from_search", "from_search");

                emit explorerAddItem(newId, parentId, QString("<b>%1</b>").arg(result.value("name")), result.value("art"), trackInfoMap, false, true, false, false);
                searchResultsCounter++;
            }
        }
        else if (opCode == AmpacheServer::SearchAlbums) {
            if ((searchResultsCounter < searchMaxCount) || (searchMaxCount == 0)) {
                QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SEARCHRESULT_ALBUM, result.value("id"), opData.value("serverId"));

                emit explorerAddItem(newId, parentId, result.value("name"), result.value("art"), QVariantMap({{ "art", result.value("art") }, { "group", result.value("name") }, { "from_search", "from_search" }}), false, true, false, false);
                searchResultsCounter++;
            }
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

            emit explorerAddItem(newId, parentId, displayName, result.value("art"), QVariantMap({{ "art", result.value("art") }, { "group", result.value("name") }}), true, true, false, false);
        }
        else if (opCode == AmpacheServer::BrowseAlbum) {
            QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_BROWSESONG, result.value("id"), opData.value("serverId"));

            QVariantMap trackInfoMap;
            QStringList keys = result.keys();
            foreach(QString key, keys) {
                trackInfoMap.insert(key, result.value(key));
            }

            if (
                    (originalAction.compare("action_play") == 0)            ||
                    (originalAction.compare("action_playnext") == 0)        ||
                    (originalAction.compare("action_enqueue") == 0)         ||
                    (originalAction.compare("action_insert") == 0)          ||
                    (originalAction.compare("action_enqueueshuffled") == 0)
               ) {
                Track::TrackInfo trackInfo = trackInfoFromIdExtra(newId, trackInfoMap);
                trackInfo.attributes.insert("group_id", groupId.toString());
                trackInfo.attributes.insert("group", QString("%1 %2.").arg(group).arg(i + 1));

                if ((i == 0) && (originalAction.compare("action_play") == 0)) {
                    playlist.clear();
                    actionPlay(trackInfo);
                }
                else {
                    if (!playlistContains(newId)) {
                        Track *track = new Track(trackInfo, peakCallbackInfo, decodingCallbackInfo);
                        connectTrackSignals(track);

                        if (originalAction.compare("action_enqueue") == 0) {
                            playlist.append(track);
                        }
                        else if (originalAction.compare("action_insert") == 0) {
                            playlist.insert(destinationIndex + i, track);
                        }
                        else if (originalAction.compare("action_enqueueshuffled") == 0) {
                            if (playlist.size() < 1) {
                                playlist.append(track);
                            }
                            else {
                                playlist.insert(randomGenerator->bounded(playlist.size()), track);
                            }
                        }
                        else {
                            playlist.insert(originalAction.compare("action_play") == 0 ? i - 1 : i, track);
                        }
                    }
                }
            }
            else {
                QString displayTitle = QString("<b>%1</b>").arg(result.value("title"));
                if (result.value("track").toInt()) {
                    displayTitle = displayTitle.prepend("%1. ").arg(result.value("track"));
                }
                if (!allTheSameArtist) {
                    displayTitle = displayTitle.append(" %1").arg(result.value("artist"));
                }

                emit explorerAddItem(newId, parentId, displayTitle, "qrc:/icons/audio_file.ico", trackInfoMap, false, true, false, false);
            }
        }
        else if ((opCode == AmpacheServer::PlaylistRoot) || (opCode == AmpacheServer::Tags)) {
            settings.setArrayIndex(i);
            settings.setValue("id", result.value("id"));
            settings.setValue("name", result.value("name"));
            settings.setValue("art", result.value("art"));
        }
        else if ((opCode == AmpacheServer::PlaylistSongs) || (opCode == AmpacheServer::Shuffle)) {
            if (!((opCode == AmpacheServer::PlaylistSongs) && (i < startingIndex))) {
                QString newId = QString("%1%2|%3").arg(UI_ID_PREFIX_SERVER_PLAYLIST_ITEM, result.value("id"), opData.value("serverId"));

                QVariantMap trackInfoMap;
                QStringList keys = result.keys();
                foreach(QString key, keys) {
                    trackInfoMap.insert(key, result.value(key));
                }

                if (
                        (originalAction.compare("action_play") == 0)          ||
                        (originalAction.compare("action_playnext") == 0)      ||
                        (originalAction.compare("action_enqueue") == 0)       ||
                        (originalAction.compare("action_insert") == 0)        ||
                        (originalAction.compare("action_enqueueshuffled") == 0)
                   ) {
                    Track::TrackInfo trackInfo = trackInfoFromIdExtra(newId, trackInfoMap);

                    if (!group.isEmpty()) {
                        trackInfo.attributes.insert("group_id", groupId.toString());
                        trackInfo.attributes.insert("group", QString("%1 %2.").arg(group).arg(i + 1));
                    }
                    if ((opCode == AmpacheServer::PlaylistSongs) && !opData.value("playlist").startsWith("smart", Qt::CaseInsensitive)) {
                        trackInfo.attributes.insert("server_playlist_id", opData.value("playlist"));
                        trackInfo.attributes.insert("server_playlist_index", QString("%1").arg(i));
                    }

                    if ((i == startingIndex) && (originalAction.compare("action_play") == 0)) {
                        playlist.clear();
                        actionPlay(trackInfo);
                    }
                    else {
                        if (!playlistContains(newId)) {
                            Track *track = new Track(trackInfo, peakCallbackInfo, decodingCallbackInfo);
                            connectTrackSignals(track);

                            if (originalAction.compare("action_enqueue") == 0) {
                                playlist.append(track);
                            }
                            else if (originalAction.compare("action_insert") == 0) {
                                playlist.insert(destinationIndex + i, track);
                            }
                            else if (originalAction.compare("action_enqueueshuffled") == 0) {
                                if (playlist.size() < 1) {
                                    playlist.append(track);
                                }
                                else {
                                    playlist.insert(randomGenerator->bounded(playlist.size()), track);
                                }
                            }
                            else {
                                playlist.insert(originalAction.compare("action_play") == 0 ? i - 1 : i, track);
                            }
                        }
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
                            if (opData.contains("server_playlist_id") && opData.contains("server_playlist_index")) {
                                trackInfo.attributes.insert("server_playlist_id", opData.value("server_playlist_id"));
                                trackInfo.attributes.insert("server_playlist_index", opData.value("server_playlist_index"));
                            }

                            Track *track = new Track(trackInfo, peakCallbackInfo, decodingCallbackInfo);
                            connectTrackSignals(track);
                            playlist.replace(i, track);
                        }
                    }
                    playlistUpdated();
                    playlistSave();

                    foreach (Track *track, toBeDeleted) {
                        connectTrackSignals(track, false);
                        track->setStatus(Track::Idle);
                        delete track;
                    }
                }
            }
            else {
                int playlistIndex = playlistLoadSongIds.indexOf(newId);

                trackInfo.attributes.insert("playlist_index", playlistIndex);
                if (opData.contains("group")) {
                    trackInfo.attributes.insert("group", group);
                }
                if (opData.contains("group_id")) {
                    trackInfo.attributes.insert("group_id", opData.value("group_id"));
                }
                if (opData.contains("server_playlist_id") && opData.contains("server_playlist_index")) {
                    trackInfo.attributes.insert("server_playlist_id", opData.value("server_playlist_id"));
                    trackInfo.attributes.insert("server_playlist_index", opData.value("server_playlist_index"));
                }

                playlistLoadTracks.append(trackInfo);
                playlistLoadSongIds.replace(playlistIndex, "");
            }
        }
    }

    if ((opCode == AmpacheServer::Search) || (opCode == AmpacheServer::SearchAlbums)) {
        searchOperationsCounter--;
        if (searchOperationsCounter <= 0) {
            explorerNetworkingUISignals(parentId, false);
            searchAction();
        }
    }
    else if (opCode == AmpacheServer::BrowseRoot) {
        settings.endArray();
        settings.sync();

        QString browseId = QString("%1|%2").arg(QString(opData.value("serverId")).replace(0, 1, UI_ID_PREFIX_SERVER_BROWSE), opData.value("serverId"));
        explorerNetworkingUISignals(browseId, false);
        if (opResults.count() > 0) {
            itemActionServerItem(browseId, globalConstant("action_expand").toInt(), QVariantMap());
        }
    }
    else if (opCode == AmpacheServer::BrowseArtist) {
        explorerNetworkingUISignals(parentId, false);
    }
    else if (opCode == AmpacheServer::BrowseAlbum) {
        explorerNetworkingUISignals(parentId, false);

        if (
                (originalAction.compare("action_play") == 0)          ||
                (originalAction.compare("action_playnext") == 0)      ||
                (originalAction.compare("action_enqueue") == 0)       ||
                (originalAction.compare("action_insert") == 0)        ||
                (originalAction.compare("action_enqueueshuffled") == 0)
           ) {
            playlistUpdated();
            playlistSave();
        }
    }
    else if (opCode == AmpacheServer::PlaylistRoot) {
        settings.endArray();
        settings.sync();

        QString playlistId = QString("%1|%2").arg(QString(opData.value("serverId")).replace(0, 1, UI_ID_PREFIX_SERVER_PLAYLISTS), opData.value("serverId"));
        explorerNetworkingUISignals(playlistId, false);
        if (opResults.count() > 0) {
            itemActionServerItem(playlistId, globalConstant("action_expand").toInt(), QVariantMap());
        }

        playlistId = QString("%1|%2").arg(QString(opData.value("serverId")).replace(0, 1, UI_ID_PREFIX_SERVER_SMARTPLAYLISTS), opData.value("serverId"));
        explorerNetworkingUISignals(playlistId, false);
        if (opResults.count() > 0) {
            itemActionServerItem(playlistId, globalConstant("action_expand").toInt(), QVariantMap());
        }
    }
    else if ((opCode == AmpacheServer::PlaylistSongs) || (opCode == AmpacheServer::Shuffle)) {
        explorerNetworkingUISignals(parentId, false);

        if (
                (originalAction.compare("action_play") == 0)          ||
                (originalAction.compare("action_playnext") == 0)      ||
                (originalAction.compare("action_enqueue") == 0)       ||
                (originalAction.compare("action_insert") == 0)        ||
                (originalAction.compare("action_enqueueshuffled") == 0)
           ) {
            playlistUpdated();
            playlistSave();
        }
    }
    else if (opCode == AmpacheServer::RadioStations) {
        settings.endArray();
        settings.sync();

        QString radiosId = QString("%1|%2").arg(QString(opData.value("serverId")).replace(0, 1, UI_ID_PREFIX_SERVER_RADIOSTATIONS), opData.value("serverId"));
        explorerNetworkingUISignals(radiosId, false);
        if (opResults.count() > 0) {
            itemActionServerItem(radiosId, globalConstant("action_expand").toInt(), QVariantMap());
        }
    }
    else if (opCode == AmpacheServer::Tags) {
        settings.endArray();
        settings.sync();

        QString genresId = QString("%1|%2").arg(QString(opData.value("serverId")).replace(0, 1, UI_ID_PREFIX_SERVER_GENRES), opData.value("serverId"));
        explorerNetworkingUISignals(genresId, false);
        if (opResults.count() > 0) {
            itemActionServerItem(genresId, globalConstant("action_expand").toInt(), QVariantMap());
        }
    }
    else if (opCode == AmpacheServer::Song) {
        if (!opData.contains("session_expired")) {
            bool finished = true;
            foreach(QString songId, playlistLoadSongIds) {
                if (!songId.isEmpty()) {
                    finished = false;
                    break;
                }
            }

            if (finished) {
                std::sort(playlistLoadTracks.begin(), playlistLoadTracks.end(), [](Track::TrackInfo a, Track::TrackInfo b) {
                    return a.attributes.value("playlist_index").toInt() < b.attributes.value("playlist_index").toInt();
                });

                for(int i = 0; i < playlistLoadTracks.count(); i++) {
                    Track::TrackInfo trackInfo = playlistLoadTracks.at(i);

                    if (i == 0) {
                        playlist.clear();
                        actionPlay(trackInfo);
                    }
                    else {
                        Track *track = new Track(trackInfo, peakCallbackInfo, decodingCallbackInfo);
                        connectTrackSignals(track);
                        playlist.append(track);
                    }
                }

                playlistUpdated();
                playlistSave();
            }
        }
    }
    else if (opCode == AmpacheServer::Artist) {
        if (opResults.count() > 0) {
            if ((currentTrack != nullptr) && (currentTrack->getTrackInfo().artistId.compare(opResults.at(0).value("id")) == 0)) {
                currentTrack->artistInfoAdd(opResults.at(0).value("summary"), opResults.at(0).value("art"));
            }
            foreach (Track *track, playlist) {
                if (track->getTrackInfo().artistId.compare(opResults.at(0).value("id")) == 0) {
                    track->artistInfoAdd(opResults.at(0).value("summary"), opResults.at(0).value("art"));
                }
            }

        }
    }

    if (opData.contains("search_action_parent_id")) {
        searchAction();
    }
    if (opData.contains("auto_refresh")) {
        QTimer::singleShot(2500, this, &Waver::autoRefreshNext);
    }
}


void Waver::serverPasswordNeeded(QString id)
{
    int srvIndex = serverIndex(id);
    if ((srvIndex >= servers.size()) || (srvIndex < 0)) {
        errorMessage(id, tr("Server ID can not be found"), id);
        return;
    }

    stopShuffleCountdown();

    emit uiPromptServerPsw(id, servers.at(srvIndex)->formattedName());
}

void Waver::setServerPassword(QString id, QString psw)
{
    int srvIndex = serverIndex(id);
    if ((srvIndex >= servers.size()) || (srvIndex < 0)) {
        errorMessage(id, tr("Server ID can not be found"), id);
        return;
    }

    servers.at(srvIndex)->setPassword(psw);

    if ((currentTrack == nullptr) && (playlist.size() == 0)) {
        startShuffleCountdown();
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
            if (playlistLoad()) {
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

    if (isCrossfade(previousTrack, currentTrack) != PlayNormal) {
        crossfadeInProgress = true;
        QTimer::singleShot(currentTrack->getFadeDurationSeconds(Track::FadeDirectionOut) * 1000 / 2, this, &Waver::startNextTrackUISignals);
    }
    else {
        startNextTrackUISignals();
    }
}


void Waver::startNextTrackUISignals()
{
    if (currentTrack == nullptr) {
        return;
    }

    crossfadeInProgress = false;

    Track::TrackInfo trackInfo = getCurrentTrackInfo();

    emit uiSetTrackData(trackInfo.title, trackInfo.artist, trackInfo.album, trackInfo.track, trackInfo.year, trackInfo.artistSummary);
    emit uiSetTrackLength(QDateTime::fromMSecsSinceEpoch(currentTrack->getLengthMilliseconds()).toUTC().toString("hh:mm:ss"));
    emit uiSetTrackPosition("00:00", 0);
    emit uiSetTrackTags(trackInfo.tags.join(", "));
    emit uiSetImage(trackInfo.arts.size() ? trackInfo.arts.at(0).toString() : "qrc:/images/waver.png");
    emit uiSetFavorite(trackInfo.attributes.contains("flag"));
    emit uiSetTrackBusy(currentTrack->getNetworkStartingLastState());
    if (!trackInfo.url.isLocalFile()) {
        emit uiSetTrackAmpacheURL(trackURL(trackInfo.id).toString(QUrl::FullyEncoded));
    }

    if (trackInfo.artistSummary.isEmpty()) {
        int srvIndex = serverIndex(trackInfo.id.split("|").last());
        if (srvIndex >= 0) {
            servers.at(srvIndex)->startOperation(AmpacheServer::Artist, {{ "artist_id", trackInfo.artistId }}, nullptr);
        }
    }

    emit requestTrackBufferReplayGainInfo();

    playlistUpdated();
    playlistSave();

    emit notify(All);
}


void Waver::startShuffleBatch(int srvIndex, QString group, int artistId, ShuffleMode mode, QString originalAction, int shuffleTag, int insertDestinationindex, QString searchActionParentId)
{
    if (srvIndex < 0) {
        bool shuffleTagsExist = false;
        int i = 0;
        while (!shuffleTagsExist && (i < servers.count())) {
            shuffleTagsExist = servers.at(i)->isShuffleTagsSelected();
            i++;
        }
        while (shuffleTagsExist && !servers.at(shuffleServerIndex)->isShuffleTagsSelected()) {
            shuffleServerIndex++;
            if (shuffleServerIndex >= servers.count()) {
                shuffleServerIndex = 0;
            }
        }

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
    else if (mode == RecentlyAdded) {
        opData.insert("recently_added", "recently_added");
    }
    if (shuffleTag != 0) {
        opData.insert("shuffle_tag", QString("%1").arg(shuffleTag));
    }

    if (!QStringList({"action_play", "action_playnext", "action_enqueue", "action_insert", "action_enqueueshuffled"}).contains(originalAction)) {
        originalAction = "action_play";
    }

    QObject *opExtra = new QObject();
    opExtra->setProperty("original_action", originalAction);
    if (!group.isEmpty()) {
        opExtra->setProperty("group", group);
    }
    if (insertDestinationindex > 0) {
        opExtra->setProperty("destination_index", QString("%1").arg(insertDestinationindex));
    }
    if (!searchActionParentId.isEmpty()) {
        opExtra->setProperty("search_action_parent_id", searchActionParentId);
    }

    servers.at(srvIndex)->startOperation(AmpacheServer::Shuffle, opData, opExtra);
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
}


void Waver::stopButton()
{
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
    playlistUpdated();

    if (!stopByShutdown) {
        playlistSave();
    }
}


void Waver::stopShuffleCountdown()
{
    shuffleCountdownTimer->stop();
    emit uiSetShuffleCountdown(0);
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
        return;
    }

    for (int i = 0; i < playlist.size(); i++) {
        if (id.compare(playlist.at(i)->getTrackInfo().id) == 0) {
            emit playlistBusy(i, busy);
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

    if (isCrossfade(currentTrack, playlist.first()) != PlayNormal) {
        killPreviousTrack();

        previousTrack = currentTrack;
        currentTrack  = nullptr;
        startNextTrack();
    }
}


void Waver::trackFinished(QString id)
{
    bool shuffleOK = true;

    QStringList idParts  = id.split("|");
    QString     serverId = idParts.last();
    int         srvIndex = serverIndex(serverId);

    if ((previousTrack != nullptr) && (id.compare(previousTrack->getTrackInfo().id) == 0)) {
        connectTrackSignals(previousTrack, false);

        Track::TrackInfo trackInfo    = previousTrack->getTrackInfo();
        QString          displayTitle = trackInfo.attributes.contains("radio_station") ? QString("<b>%1</b>").arg(trackInfo.artist) : QString("<b>%1</b> %2").arg(trackInfo.title, trackInfo.artist);
        history.prepend(trackInfo);
        emit uiHistoryAdd(displayTitle);

        if ((srvIndex >= 0) && (srvIndex < servers.size()) && trackInfo.attributes.contains("server_playlist_id") && trackInfo.attributes.contains("server_playlist_index")) {
            playlistAttributeSave(servers.at(srvIndex), trackInfo.attributes.value("server_playlist_id").toString(), "last_played_index", trackInfo.attributes.value("server_playlist_index").toString());
            playlistAttributeSave(servers.at(srvIndex), trackInfo.attributes.value("server_playlist_id").toString(), "last_played_time", QString("%1").arg(QDateTime::currentMSecsSinceEpoch()));
        }

        previousTrack->setStatus(Track::Paused);
        previousTrack->setStatus(Track::Idle);
        delete previousTrack;
        previousTrack = nullptr;

        shuffleOK = false;
        startNextTrack();
    }

    if ((currentTrack != nullptr) && (id.compare(getCurrentTrackInfo().id) == 0)) {
        if (currentTrack->getPlayedMillseconds() < 1000) {
            errorMessage(id, tr("Unable to start"), getCurrentTrackInfo().url.toString());
        }

        connectTrackSignals(currentTrack, false);

        Track::TrackInfo trackInfo    = getCurrentTrackInfo();
        QString          displayTitle = trackInfo.attributes.contains("radio_station") ? QString("<b>%1</b>").arg(trackInfo.artist) : QString("<b>%1</b> %2").arg(trackInfo.title, trackInfo.artist);
        history.prepend(trackInfo);
        emit uiHistoryAdd(displayTitle);

        if ((srvIndex >= 0) && (srvIndex < servers.size()) && trackInfo.attributes.contains("server_playlist_id") && trackInfo.attributes.contains("server_playlist_index")) {
            playlistAttributeSave(servers.at(srvIndex), trackInfo.attributes.value("server_playlist_id").toString(), "last_played_index", trackInfo.attributes.value("server_playlist_index").toString());
            playlistAttributeSave(servers.at(srvIndex), trackInfo.attributes.value("server_playlist_id").toString(), "last_played_time", QString("%1").arg(QDateTime::currentMSecsSinceEpoch()));
        }

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

    playlistUpdated();
    playlistSave();

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

    trackInfo.id       = QString("%1%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(QFileInfo(filePath).baseName());
    trackInfo.artistId = "0000";
    trackInfo.albumId  = "0000";
    trackInfo.url      = QUrl::fromLocalFile(filePath);
    trackInfo.track    = 0;
    trackInfo.year     = 0;

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

    trackInfo.id       = id;
    trackInfo.albumId  = extra.value("album_id", "0000").toString();
    trackInfo.album    = extra.value("album", tr("Unknown album")).toString();
    trackInfo.artistId = extra.value("artist_id", "0000").toString();
    trackInfo.artist   = extra.value("artist", tr("Unknown artist")).toString();
    trackInfo.title    = extra.value("title", tr("Unknown title")).toString();
    trackInfo.track    = extra.value("track", 0).toInt();
    trackInfo.url      = QUrl(extra.value("url", "").toString());
    trackInfo.year     = extra.value("year", 0).toInt();

    trackInfo.tags.append(extra.value("tags", "").toString().split('|'));

    bool OK      = false;
    int  seconds = extra.value("time", 0).toInt(&OK);
    if (OK && (seconds > 0)) {
        trackInfo.attributes.insert("lengthMilliseconds", seconds * 1000);
    }

    if (extra.contains("flag") && extra.value("flag").toString().compare("0")) {
        trackInfo.attributes.insert("flag", "true");
    }

    int srvIndex = serverIndex(id.split("|").last());
    if (srvIndex >= 0) {
        trackInfo.attributes.insert("serverSettingsId", servers.at(srvIndex)->getSettingsId().toString());
    }

    trackInfo.arts.append(QUrl(extra.value("art").toString()));

    return trackInfo;
}


void Waver::trackInfoUpdated(QString id)
{
    if (!crossfadeInProgress && (currentTrack != nullptr) && (getCurrentTrackInfo().id.compare(id) == 0)) {
        Track::TrackInfo trackInfo = getCurrentTrackInfo();

        emit uiSetTrackData(trackInfo.title, trackInfo.artist, trackInfo.album, trackInfo.track, trackInfo.year, trackInfo.artistSummary);
        emit uiSetTrackLength(QDateTime::fromMSecsSinceEpoch(currentTrack->getLengthMilliseconds()).toUTC().toString("hh:mm:ss"));
        emit uiSetTrackTags(trackInfo.tags.join(", "));
        emit uiSetImage(trackInfo.arts.size() ? trackInfo.arts.at(0).toString() : "qrc:/images/waver.png");
        emit uiSetFavorite(trackInfo.attributes.contains("flag"));
        if (!trackInfo.url.isLocalFile()) {
            emit uiSetTrackAmpacheURL(trackURL(trackInfo.id).toString(QUrl::FullyEncoded));
        }
    }
}


void Waver::trackPlayPosition(QString id, bool decoderFinished, long knownDurationMilliseconds, long positionMilliseconds)
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
    if (knownDurationMilliseconds > 0) {
        positionPercent = static_cast<double>(positionMilliseconds) / knownDurationMilliseconds;
    }

    emit uiSetTrackPosition(QDateTime::fromMSecsSinceEpoch(positionMilliseconds).toUTC().toString("hh:mm:ss"), positionPercent);

    if ((track == currentTrack) && (knownDurationMilliseconds > 0) && (playlist.size() > 0) && (playlist.at(0)->getStatus() == Track::Idle) && (knownDurationMilliseconds - positionMilliseconds <= 20000 + playlist.at(0)->getFadeDurationSeconds(Track::FadeDirectionIn) * 1000)) {
        playlist.at(0)->setStatus(Track::Decoding);
    }

    if (autoRefresh && (autoRefreshLastDay != QDateTime::currentDateTime().date().day()) && (QDateTime::currentMSecsSinceEpoch() >= autoRefreshLastActionTimestamp + AUTO_REFRESH_NO_ACTION_DELAY_MILLISEC)) {
        autoRefreshLastDay = QDateTime::currentDateTime().date().day();

        QSettings settings;
        settings.setValue("auto_refresh/last_day", autoRefreshLastDay);

        autoRefreshNext();
    }
}


void Waver::trackReplayGainInfo(QString id, double current)
{
    if ((currentTrack != nullptr) && (getCurrentTrackInfo().id.compare(id) == 0)) {
        if (current < -12) {
            current = -12;
        }
        if (current > 6) {
            current = 6;
        }
        emit uiSetPeakMeterReplayGain((current + 12) / 18);
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

    if ((currentTrack != nullptr) && (getCurrentTrackInfo().id.compare(trackId) == 0)) {
        QObject *opExtra = new QObject();
        opExtra->setProperty("session_expired", "current_track");
        servers.at(srvIndex)->startOperation(AmpacheServer::Song, {{ "song_id", idParts.first().replace(0, 1, "") }}, opExtra);
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
        }
    }
}


void Waver::trackStatusChanged(QString id, Track::Status status, QString statusString)
{
    Q_UNUSED(status);

    if ((currentTrack != nullptr) && (getCurrentTrackInfo().id.compare(id) == 0)) {
        emit uiSetStatusText(statusString);
    }
}


QUrl Waver::trackURL(QString id)
{
    QStringList idParts  = id.split("|");
    QString     serverId = idParts.last();
    int         srvIndex = serverIndex(serverId);

    QUrl returnValue = servers.at(srvIndex)->getHost();
    returnValue.setPath("/song.php");
    returnValue.setQuery("action=show_song&song_id=" + idParts.first().mid(1));

    return returnValue;
}


void Waver::updatedOptions(QString optionsJSON)
{
    QVariantMap options = QJsonDocument::fromJson(optionsJSON.toUtf8()).toVariant().toMap();
    QSettings   settings;

    QStringList genres = options.value("genres").toStringList();

    foreach (AmpacheServer *server, servers) {
        server->setShuffleTags(genres);
    }

    peakFPSMax            = options.value("max_peak_fps").toInt();
    peakDelayOn           = options.value("peak_delay_on").toBool();
    peakDelayMilliseconds = options.value("peak_delay_ms").toInt();
    autoRefresh           = options.value("auto_refresh").toBool();

    crossfadeTags.clear();
    crossfadeTags.append(options.value("crossfade_tags").toString().split(","));

    emit uiSetFontSize(options.value("font_size").toInt());
    emit uiSetTitleCurlySpecial(options.value("title_curly_special").toBool());
    if (currentTrack != nullptr) {
        Track::TrackInfo trackInfo = getCurrentTrackInfo();
        emit uiSetTrackData(trackInfo.title, trackInfo.artist, trackInfo.album, trackInfo.track, trackInfo.year, trackInfo.artistSummary);
    }

    settings.setValue("options/shuffle_autostart", options.value("shuffle_autostart").toBool());
    settings.setValue("options/shuffle_operator", options.value("shuffle_operator").toString());
    settings.setValue("options/shuffle_count", options.value("shuffle_count").toInt());
    settings.setValue("options/random_lists_count", options.value("random_lists_count").toInt());
    settings.setValue("options/recently_added_count", options.value("recently_added_count").toInt());
    settings.setValue("options/recently_added_days", options.value("recently_added_days").toInt());
    settings.setValue("options/shuffle_delay_seconds", options.value("shuffle_delay_seconds").toInt());
    settings.setValue("options/shuffle_favorite_frequency", options.value("shuffle_favorite_frequency").toInt());
    settings.setValue("options/shuffle_recently_added_frequency", options.value("shuffle_recently_added_frequency").toInt());

    settings.setValue("options/search_count_max", options.value("search_count_max").toInt());
    settings.setValue("options/search_action", options.value("search_action").toInt());
    settings.setValue("options/search_action_filter", options.value("search_action_filter").toInt());
    settings.setValue("options/search_action_count_max", options.value("search_action_count_max").toInt());

    settings.setValue("options/max_peak_fps", peakFPSMax);
    settings.setValue("options/peak_delay_on", peakDelayOn);
    settings.setValue("options/peak_delay_ms", peakDelayMilliseconds);

    settings.setValue("options/fade_tags", options.value("fade_tags").toString());
    settings.setValue("options/crossfade_tags", options.value("crossfade_tags").toString());
    settings.setValue("options/fade_seconds", options.value("fade_seconds").toInt());
    settings.setValue("options/starting_index_apply", options.value("starting_index_apply").toBool());
    settings.setValue("options/starting_index_days", options.value("starting_index_days").toLongLong());
    settings.setValue("options/auto_refresh", options.value("auto_refresh").toBool());
    settings.setValue("options/hide_dot_playlists", options.value("hide_dot_playlists").toBool());
    settings.setValue("options/title_curly_special", options.value("title_curly_special").toBool());
    settings.setValue("options/alphabet_limit", options.value("alphabet_limit").toInt());
    settings.setValue("options/font_size", options.value("font_size").toInt());
    settings.setValue("options/wide_stereo_delay_millisec", options.value("wide_stereo").toInt());
    settings.setValue("options/skip_long_silence", options.value("skip_long_silence").toBool());
    settings.setValue("options/skip_long_silence_seconds", options.value("skip_long_silence_seconds").toInt());

    if (!options.value("eq_disable").toBool()) {
        settings.setValue("eq/on",  options.value("eq_on").toBool());

        Track::TrackInfo currentTrackInfo = getCurrentTrackInfo();

        QString settingsId;
        QString trackId;
        if (currentTrack != nullptr) {
            QStringList idParts  = currentTrackInfo.id.split("|");
            QString     serverId = idParts.last();
            int         srvIndex = serverIndex(serverId);

            settingsId = servers.at(srvIndex)->getSettingsId().toString();
            trackId    = idParts.at(0).mid(1);
        }

        int eqChooser = options.value("eq_chooser").toInt();
        if (eqChooser == 1) {
            if (settingsId.length() >= 0) {
                settings.remove(QString("%1/track/%2/eq").arg(settingsId, trackId));

                settings.setValue(QString("%1/album/%2/eq/eq1").arg(settingsId, currentTrackInfo.albumId), options.value("eq1").toDouble());
                settings.setValue(QString("%1/album/%2/eq/eq2").arg(settingsId, currentTrackInfo.albumId), options.value("eq2").toDouble());
                settings.setValue(QString("%1/album/%2/eq/eq3").arg(settingsId, currentTrackInfo.albumId), options.value("eq3").toDouble());
                settings.setValue(QString("%1/album/%2/eq/eq4").arg(settingsId, currentTrackInfo.albumId), options.value("eq4").toDouble());
                settings.setValue(QString("%1/album/%2/eq/eq5").arg(settingsId, currentTrackInfo.albumId), options.value("eq5").toDouble());
                settings.setValue(QString("%1/album/%2/eq/eq6").arg(settingsId, currentTrackInfo.albumId), options.value("eq6").toDouble());
                settings.setValue(QString("%1/album/%2/eq/eq7").arg(settingsId, currentTrackInfo.albumId), options.value("eq7").toDouble());
                settings.setValue(QString("%1/album/%2/eq/eq8").arg(settingsId, currentTrackInfo.albumId), options.value("eq8").toDouble());
                settings.setValue(QString("%1/album/%2/eq/eq9").arg(settingsId, currentTrackInfo.albumId), options.value("eq9").toDouble());
                settings.setValue(QString("%1/album/%2/eq/eq10").arg(settingsId, currentTrackInfo.albumId), options.value("eq10").toDouble());
                settings.setValue(QString("%1/album/%2/eq/pre_amp").arg(settingsId, currentTrackInfo.albumId), options.value("pre_amp").toDouble());
            }
        }
        else if (eqChooser == 2) {
            if (settingsId.length() >= 0) {
                settings.setValue(QString("%1/track/%2/eq/eq1").arg(settingsId, trackId), options.value("eq1").toDouble());
                settings.setValue(QString("%1/track/%2/eq/eq2").arg(settingsId, trackId), options.value("eq2").toDouble());
                settings.setValue(QString("%1/track/%2/eq/eq3").arg(settingsId, trackId), options.value("eq3").toDouble());
                settings.setValue(QString("%1/track/%2/eq/eq4").arg(settingsId, trackId), options.value("eq4").toDouble());
                settings.setValue(QString("%1/track/%2/eq/eq5").arg(settingsId, trackId), options.value("eq5").toDouble());
                settings.setValue(QString("%1/track/%2/eq/eq6").arg(settingsId, trackId), options.value("eq6").toDouble());
                settings.setValue(QString("%1/track/%2/eq/eq7").arg(settingsId, trackId), options.value("eq7").toDouble());
                settings.setValue(QString("%1/track/%2/eq/eq8").arg(settingsId, trackId), options.value("eq8").toDouble());
                settings.setValue(QString("%1/track/%2/eq/eq9").arg(settingsId, trackId), options.value("eq9").toDouble());
                settings.setValue(QString("%1/track/%2/eq/eq10").arg(settingsId, trackId), options.value("eq10").toDouble());
                settings.setValue(QString("%1/track/%2/eq/pre_amp").arg(settingsId, trackId), options.value("pre_amp").toDouble());
            }
        }
        else {
            settings.remove(QString("%1/track/%2/eq").arg(settingsId, trackId));
            settings.remove(QString("%1/album/%2/eq").arg(settingsId, currentTrackInfo.albumId));

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
