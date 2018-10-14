/*
    This file is part of Waver

    Copyright (C) 2017 Peter Papp <peter.papp.p@gmail.com>

    Please visit https://launchpad.net/waver for details

    Waver is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Waver is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    (GPL.TXT) along with Waver. If not, see <http://www.gnu.org/licenses/>.

*/


#include "server.h"

// constructor
WaverServer::WaverServer(QObject *parent, QStringList arguments) : QObject(parent)
{
    // to make debugging easier
    serverTcpThread.setObjectName("server_tcp");
    settingsThread.setObjectName("settings");
    pluginLibsLoaderThread.setObjectName("pluginlibs_loader");
    sourcesThread.setObjectName("sources");

    #ifdef QT_DEBUG
    qSetMessagePattern("%{if-fatal}\n%{endif}%{if-category}%{category}: %{endif}%{message}%{if-fatal}\n%{backtrace depth=7 separator=\"\n\"}%{endif}");
    #endif

    // save argument
    this->arguments.append(arguments);

    // initializations
    previousTrack                       = NULL;
    currentTrack                        = NULL;
    currentCollection                   = "";
    startupCollection                   = "";
    waitingForLocalSource               = true;
    waitingForLocalSourceTimerStarted   = false;
    lastRequestPlaylist                 = QDateTime::fromSecsSinceEpoch(1);
    positionSeconds                     = 0;
    showPreviousTime                    = false;
    previousPositionSeconds             = 0;
    sendErrorLogDiagnostics             = false;
    diagnosticsChanged                  = false;
    streamPlayTime                      = 450 * 1000;
    lovedStreamPlayTime                 = 450 * 1000 * 2;
    playlistAddMode                     = PLAYLIST_ADD_END;

    diagnosticsTimer.setInterval(1000 / 20);
    connect(&diagnosticsTimer, SIGNAL(timeout()), this, SLOT(diagnosticsRefreshUI()));

    // so they can be used in inter-thread signals
    qRegisterMetaType<IpcMessageUtils::IpcMessages>("IpcMessageUtils::IpcMessages");
    qRegisterMetaType<TrackInfo>("TrackInfo");
    qRegisterMetaType<TracksInfo>("TracksInfo");
    qRegisterMetaType<ExtraInfo>("ExtraInfo");
    qRegisterMetaType<OpenTrack>("OpenTrack");
    qRegisterMetaType<OpenTracks>("OpenTracks");
    qRegisterMetaType<DiagnosticItem>("DiagnosticItem");
    qRegisterMetaType<DiagnosticData>("DiagnosticData");
    qRegisterMetaType<SqlResults>("SqlResults");
    qRegisterMetaType<Track::PluginList>("Track::PluginList");
}


// start this server, meant to be called through invokeMethod from the main program
void WaverServer::run()
{
    qsrand(QDateTime::currentDateTime().toTime_t());
    // instantiate, set up, and start interprocess communication handler

    ServerTcpHandler *serverTcpHandler = new ServerTcpHandler();
    serverTcpHandler->moveToThread(&serverTcpThread);

    connect(&serverTcpThread, SIGNAL(started()),  serverTcpHandler, SLOT(run()));
    connect(&serverTcpThread, SIGNAL(finished()), serverTcpHandler, SLOT(deleteLater()));

    connect(this,             SIGNAL(ipcSend(QString)),                                     serverTcpHandler, SLOT(send(QString)));
    connect(serverTcpHandler, SIGNAL(message(IpcMessageUtils::IpcMessages, QJsonDocument)), this,             SLOT(ipcReceivedMessage(IpcMessageUtils::IpcMessages, QJsonDocument)));
    connect(serverTcpHandler, SIGNAL(url(QUrl)),                                            this,             SLOT(ipcReceivedUrl(QUrl)));
    connect(serverTcpHandler, SIGNAL(error(bool, QString)),                                 this,             SLOT(ipcReceivedError(bool, QString)));
    connect(serverTcpHandler, SIGNAL(noClient()),                                           this,             SLOT(ipcNoClient()));

    serverTcpThread.start();
    // instantiate, set up, and start settings storage

    SettingsHandler *settingsHandler = new SettingsHandler();
    settingsHandler->moveToThread(&settingsThread);

    connect(&settingsThread, SIGNAL(started()),  settingsHandler, SLOT(run()));
    connect(&settingsThread, SIGNAL(finished()), settingsHandler, SLOT(deleteLater()));

    connect(this,            SIGNAL(saveWaverSettings(QString, QJsonDocument)),                                     settingsHandler, SLOT(saveWaverSettings(QString, QJsonDocument)));
    connect(this,            SIGNAL(loadWaverSettings(QString)),                                                    settingsHandler, SLOT(loadWaverSettings(QString)));
    connect(this,            SIGNAL(savePluginSettings(QUuid, QString, QJsonDocument)),                             settingsHandler, SLOT(savePluginSettings(QUuid, QString, QJsonDocument)));
    connect(this,            SIGNAL(loadPluginSettings(QUuid, QString)),                                            settingsHandler, SLOT(loadPluginSettings(QUuid, QString)));
    connect(this,            SIGNAL(executeSettingsSql(QUuid, QString, bool, QString, int, QString, QVariantList)), settingsHandler, SLOT(executeSql(QUuid, QString, bool, QString, int, QString, QVariantList)));
    connect(settingsHandler, SIGNAL(loadedWaverSettings(QJsonDocument)),                                            this,            SLOT(loadedWaverSettings(QJsonDocument)));
    connect(settingsHandler, SIGNAL(loadedWaverGlobalSettings(QJsonDocument)),                                      this,            SLOT(loadedWaverGlobalSettings(QJsonDocument)));
    connect(settingsHandler, SIGNAL(loadedPluginSettings(QUuid, QJsonDocument)),                                    this,            SLOT(loadedPluginSettings(QUuid, QJsonDocument)));
    connect(settingsHandler, SIGNAL(loadedPluginGlobalSettings(QUuid, QJsonDocument)),                              this,            SLOT(loadedPluginGlobalSettings(QUuid, QJsonDocument)));
    connect(settingsHandler, SIGNAL(sqlResults(QUuid, bool, QString, int, SqlResults)),                             this,            SLOT(executedPluginSqlResults(QUuid, bool, QString, int, SqlResults)));
    connect(settingsHandler, SIGNAL(globalSqlResults(QUuid, bool, QString, int, SqlResults)),                       this,            SLOT(executedPluginGlobalSqlResults(QUuid, bool, QString, int, SqlResults)));
    connect(settingsHandler, SIGNAL(sqlError(QUuid, bool, QString, int, QString)),                                  this,            SLOT(executedPluginSqlError(QUuid, bool, QString, int, QString)));

    settingsThread.start();
}


// shutdown this server - private method
void WaverServer::finish()
{
    outputError("<INFO>Shutting down", "", false);

    // delete tracks
    if (previousTrack != NULL) {
        emit done(previousTrack->getSourcePluginId(), previousTrack->getTrackInfo().url, false);
        delete previousTrack;
    }
    if (currentTrack != NULL) {
        emit done(currentTrack->getSourcePluginId(), currentTrack->getTrackInfo().url, false);
        delete currentTrack;
    }
    foreach (Track *track, playlistTracks) {
        emit done(track->getSourcePluginId(), track->getTrackInfo().url, false);
        delete track;
    }

    // stop sources
    sourcesThread.requestInterruption();
    sourcesThread.quit();
    sourcesThread.wait();

    // stop settings storage
    settingsThread.quit();
    settingsThread.wait(THREAD_TIMEOUT);

    // let the clients know
    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::QuitClients));

    // must wait a bit for IPC message to complete
    QTimer::singleShot(500, this, SLOT(completeFinish()));
}


// timer slot
void WaverServer::completeFinish()
{
    // stop interprocess communication handler
    serverTcpThread.quit();
    serverTcpThread.wait(THREAD_TIMEOUT);

    // let coreApplication know
    emit finished();
}


// shutdown this server with error message - private method
void WaverServer::finish(QString errorMessage)
{
    Globals::consoleOutput(errorMessage, true);
    finish();
}


// configuration conversion
QJsonDocument WaverServer::configToJson()
{
    QJsonArray sourcePriorities;
    QJsonArray sourceLovedModes;
    foreach (QUuid pluginId, sourcePlugins.keys()) {
        sourcePriorities.append(QJsonArray({ pluginId.toString(), sourcePlugins.value(pluginId).priority }));
        sourceLovedModes.append(QJsonArray({ pluginId.toString(), sourcePlugins.value(pluginId).lovedMode }));
    }

    QJsonObject jsonObject;
    jsonObject.insert("source_priorities", sourcePriorities);
    jsonObject.insert("source_loved_modes", sourceLovedModes);
    jsonObject.insert("recurring_source", recurringPlugin.toString());
    jsonObject.insert("stream_play_time", streamPlayTime);
    jsonObject.insert("loved_stream_play_time", lovedStreamPlayTime);
    jsonObject.insert("playlist_add_mode", playlistAddMode);

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
QJsonDocument WaverServer::configToJsonGlobal()
{
    QJsonObject jsonObject;
    jsonObject.insert("collections", QJsonArray::fromStringList(collections));
    jsonObject.insert("current_collection", currentCollection);
    jsonObject.insert("startup_collection", startupCollection);

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
void WaverServer::jsonToConfig(QJsonDocument jsonDocument)
{
    foreach (SourcePlugin sourcePlugin, sourcePlugins) {
        sourcePlugin.priority = 4;
    }

    if (jsonDocument.object().contains("source_priorities")) {
        foreach (QJsonValue dataItem, jsonDocument.object().value("source_priorities").toArray()) {
            QVariantList sourcePriority = dataItem.toArray().toVariantList();

            QUuid pluginId(sourcePriority.at(0).toString());
            if (sourcePlugins.contains(pluginId)) {
                int priority = sourcePriority.at(1).toInt();
                sourcePlugins[pluginId].priority = (priority > 10 ? 4 : priority);
            }
        }
    }

    if (jsonDocument.object().contains("source_loved_modes")) {
        foreach (QJsonValue dataItem, jsonDocument.object().value("source_loved_modes").toArray()) {
            QVariantList sourceLovedMode = dataItem.toArray().toVariantList();

            QUuid pluginId(sourceLovedMode.at(0).toString());
            if (sourcePlugins.contains(pluginId)) {
                int lovedMode = sourceLovedMode.at(1).toInt();
                sourcePlugins[pluginId].lovedMode = lovedMode;
            }
        }
    }

    if (jsonDocument.object().contains("recurring_source")) {
        recurringPlugin = QUuid(jsonDocument.object().value("recurring_source").toString());
    }

    if (jsonDocument.object().contains("stream_play_time")) {
        streamPlayTime = jsonDocument.object().value("stream_play_time").toInt();
    }
    if (jsonDocument.object().contains("loved_stream_play_time")) {
        lovedStreamPlayTime = jsonDocument.object().value("loved_stream_play_time").toInt();
    }

    if (jsonDocument.object().contains("playlist_add_mode")) {
        playlistAddMode = jsonDocument.object().value("playlist_add_mode").toInt();
    }
}


// configuration conversion
void WaverServer::jsonToConfigGlobal(QJsonDocument jsonDocument)
{
    collections.clear();
    if (jsonDocument.object().contains("collections")) {
        foreach (QJsonValue jsonValue, jsonDocument.object().value("collections").toArray()) {
            collections.append(jsonValue.toString());
        }
    }
    if (!collections.contains(SettingsHandler::DEFAULT_COLLECTION_NAME)) {
        collections.prepend(SettingsHandler::DEFAULT_COLLECTION_NAME);
    }
    qSort(collections);

    currentCollection.clear();
    if (jsonDocument.object().contains("current_collection")) {
        currentCollection = jsonDocument.object().value("current_collection").toString();
    }
    if (currentCollection.isEmpty()) {
        currentCollection = SettingsHandler::DEFAULT_COLLECTION_NAME;
    }

    startupCollection.clear();
    if (jsonDocument.object().contains("startup_collection")) {
        startupCollection = jsonDocument.object().value("startup_collection").toString();
    }
    if (startupCollection.isEmpty()) {
        startupCollection = Globals::lastUsedCollectionOption();
    }
}


// helper
void WaverServer::outputError(QString errorMessage, QString title, bool fatal)
{
    QString type = fatal ? "Fatal error" : "Error";
    if (errorMessage.startsWith("<INFO>")) {
        type = "Info";
        errorMessage = errorMessage.replace("<INFO>", "");
    }

    // print to error output
    if (title.isEmpty()) {
        Globals::consoleOutput(QString("%1: %2").arg(type, errorMessage), true);
    }
    else {
        Globals::consoleOutput(QString("%1 reported from track '%2': %3").arg(type, title, errorMessage), true);
    }

    // send message to UI
    QVariantHash messageHash;
    messageHash.insert("message", (title.isEmpty() ? errorMessage : title + "\n\n" + errorMessage));
    messageHash.insert("type", type);
    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::InfoMessage, QJsonDocument(QJsonObject::fromVariantHash(messageHash))));

    if (type.compare("Info") != 0) {
        // add to error log
        ErrorLogItem errorLogItem;
        errorLogItem.fatal     = fatal;
        errorLogItem.message   = errorMessage;
        errorLogItem.timestamp = QDateTime::currentDateTime();
        errorLogItem.title     = title;
        while (errorLog.count() >= 100) {
            errorLog.remove(0);
        }
        errorLog.append(errorLogItem);

        // send error log to UI
        if (sendErrorLogDiagnostics) {
            sendErrorLogDiagnosticsToClients();
        }
    }
}


// private method
void WaverServer::requestPlaylist()
{
    // prevent requests from the same plugin during startup
    if (lastRequestPlaylist.secsTo(QDateTime::currentDateTime()) < 5) {
        QTimer::singleShot(5000, this, SLOT(delayedRequestPlaylist()));
        return;
    }

    // enumerate ready source plugins
    QVector<QUuid> readyPlugins;
    foreach (QUuid pluginId, sourcePlugins.keys()) {
        if (sourcePlugins.value(pluginId).ready) {
            readyPlugins.append(pluginId);
        }
    }
    if (readyPlugins.count() < 1) {
        return;
    }

    // which plugin to use
    int pluginIndex;
    if (!recurringPlugin.isNull() && readyPlugins.contains(recurringPlugin) && (lastPlaylistPlugin != recurringPlugin)) {
        // must choose recurring
        pluginIndex = readyPlugins.indexOf(recurringPlugin);
    }
    else {
        // don't choose the recurring plugin if it's there (nothing happens if not), but only if there other plugins too
        if (readyPlugins.count() > 1) {
            readyPlugins.removeAll(recurringPlugin);
        }

        // select next plugin
        pluginIndex = readyPlugins.indexOf(lastPlaylistPlugin == recurringPlugin ? lastPlaylistPluginNotRecurring : lastPlaylistPlugin) + 1;
        if (pluginIndex >= readyPlugins.count()) {
            pluginIndex = 0;
        }
    }

    // give priority to local files at startup
    if (waitingForLocalSource) {
        pluginIndex = readyPlugins.indexOf(QUuid("{187C9046-4801-4DB2-976C-128761F25BD8}"));
        if (pluginIndex < 0) {
            return;
        }
    }

    QUuid pluginId = readyPlugins.at(pluginIndex);

    // emit signal
    if (PluginLibsLoader::isPluginCompatible(sourcePlugins.value(pluginId).waverVersionAPICompatibility, "0.0.5")) {
        if (!loveCounter.contains(pluginId)) {
            loveCounter.insert(pluginId, { 0, true });
        }

        int trackCount = qMin(sourcePlugins.value(pluginId).priority, sourcePlugins.value(pluginId).lovedMode - loveCounter.value(pluginId).counter);
        if (trackCount > 0) {
            emit getPlaylist(pluginId, trackCount, PLAYLIST_MODE_NORMAL);
            loveCounter[pluginId].counter += trackCount;
        }
        while (trackCount < sourcePlugins.value(pluginId).priority) {
            int count = qMin(sourcePlugins.value(pluginId).priority - trackCount, sourcePlugins.value(pluginId).lovedMode);
            emit getPlaylist(pluginId, count, loveCounter.value(pluginId).similarOnly ? PLAYLIST_MODE_LOVED_SIMILAR : PLAYLIST_MODE_LOVED);
            trackCount += count;
            loveCounter[pluginId].counter = count - 1;
            loveCounter[pluginId].similarOnly = !loveCounter.value(pluginId).similarOnly;
        }
    }
    else {
        emit getPlaylist(pluginId, sourcePlugins.value(pluginId).priority);
    }

    lastPlaylistPlugin = pluginId;
    if (pluginId != recurringPlugin) {
        lastPlaylistPluginNotRecurring = pluginId;
    }
    lastRequestPlaylist = QDateTime::currentDateTime();
}


// private method
void WaverServer::startNextTrack()
{
    // so that this method can be called blindly
    if (currentTrack != NULL) {
        return;
    }

    // add new tracks to playlist if needed
    if (playlistTracks.count() <= 2) {
        requestPlaylist();
    }

    // make sure there's at least one track waiting (notice requestPlaylist above; when playlist is received, this method will be called again)
    if (playlistTracks.count() < 1) {
        return;
    }

    // start next track
    currentTrack = playlistTracks.at(0);
    playlistTracks.remove(0);
    positionSeconds = -1;
    currentTrack->setStatus(Track::Playing);

    // these signals should be connected with the current track only
    connect(this,         SIGNAL(requestPluginUi(QUuid)),                currentTrack, SLOT(requestedPluginUi(QUuid)));
    connect(this,         SIGNAL(pluginUiResults(QUuid, QJsonDocument)), currentTrack, SLOT(receivedPluginUiResults(QUuid, QJsonDocument)));
    connect(currentTrack, SIGNAL(pluginUi(QUuid, QString, QString)),     this,         SLOT(pluginUi(QUuid, QString, QString)));

    // UI singals
    if (!currentTrack->getFadeInRequested()) {
        showPreviousTime = false;
        startNextTrackUISignal();
        return;
    }

    showPreviousTime = true;
    QTimer::singleShot(currentTrack->getFadeInRequestedMilliseconds() > 0 ? currentTrack->getFadeInRequestedMilliseconds() / 2 : (Track::INTERRUPT_FADE_SECONDS * 1000) / 2, this, SLOT(startNextTrackUISignal()));
}


// private slot for timer in startNextTrack
void WaverServer::startNextTrackUISignal()
{
    showPreviousTime = false;

    Globals::consoleOutput(QString("%1 - %2").arg(currentTrack->getTrackInfo().title).arg(currentTrack->getTrackInfo().performer), false);

    sendPlaylistToClients();

    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::TrackInfos, ipcMessageUtils.trackInfoToJSONDocument(currentTrack->getTrackInfo(), currentTrack->getAdditionalInfo())));
}


// private method
void WaverServer::reassignFadeIns()
{
    for (int i = 0; i < playlistTracks.count(); i++) {
        if (i == 0) {
            if (currentTrack != NULL) {
                if (currentTrack->getNextTrackFadeInRequested()) {
                    playlistTracks.at(i)->startWithFadeIn(currentTrack->getNextTrackFadeInRequestedMilliseconds());
                }
                else {
                    playlistTracks.at(i)->startWithoutFadeIn();
                }
                if (playlistTracks.at(i)->getPreviousTrackAboutToFinishSendRequested()) {
                    currentTrack->setAboutToFinishSend(playlistTracks.at(i)->getPreviousTrackAboutToFinishSendRequestedMilliseconds());
                }
                else {
                    currentTrack->resetAboutToFinishSend();
                }
            }
        }
        else {
            if (playlistTracks.at(i - 1)->getNextTrackFadeInRequested()) {
                playlistTracks.at(i)->startWithFadeIn(playlistTracks.at(i - 1)->getNextTrackFadeInRequestedMilliseconds());
            }
            else {
                playlistTracks.at(i)->startWithoutFadeIn();
            }
            if (playlistTracks.at(i)->getPreviousTrackAboutToFinishSendRequested()) {
                playlistTracks.at(i - 1)->setAboutToFinishSend(playlistTracks.at(i)->getPreviousTrackAboutToFinishSendRequestedMilliseconds());
            }
            else {
                playlistTracks.at(i - 1)->resetAboutToFinishSend();
            }
        }
    }
}


// private method
QVariantHash WaverServer::positionToElapsedRemaining(bool decoderFinished, long knownDurationMilliseconds, long positionMilliseconds)
{
    QString elapsed   = QDateTime::fromMSecsSinceEpoch(positionMilliseconds).toUTC().toString("hh:mm:ss");
    QString remaining = decoderFinished ? QDateTime::fromMSecsSinceEpoch(knownDurationMilliseconds - positionMilliseconds).toUTC().toString("hh:mm:ss") : "Decoding in progress...";

    if (elapsed.startsWith("00:")) {
        elapsed = elapsed.mid(3);
    }
    if (remaining.startsWith("00:")) {
        remaining = remaining.mid(3);
    }

    QVariantHash returnValue;

    returnValue.insert("elapsed", elapsed);
    returnValue.insert("remaining", remaining);

    return returnValue;
}


// private method
QString WaverServer::findTitleFromUrl(QUrl url)
{
    if ((previousTrack != NULL) && (previousTrack->getTrackInfo().url == url)) {
        return previousTrack->getTrackInfo().title;
    }

    if ((currentTrack != NULL) && (currentTrack->getTrackInfo().url == url)) {
        return currentTrack->getTrackInfo().title;
    }

    QString title = "";
    int     i     = 0;
    while ((title.length() < 1) && (i < playlistTracks.count())) {
        if (playlistTracks.at(i)->getTrackInfo().url == url) {
            title = playlistTracks.at(i)->getTrackInfo().title;
        }
        i++;
    }

    return title;
}


// private method
void WaverServer::stopAllDiagnostics()
{
    sendErrorLogDiagnostics = false;

    if (!lastDiagnosticsPluginId.isNull()) {
        emit stopDiagnostics(lastDiagnosticsPluginId);
        lastDiagnosticsPluginId = QUuid();
    }
    diagnosticsChanged = false;
    diagnosticsAggregator.clear();
    diagnosticsTimer.stop();
}


// private method
void WaverServer::handleCollectionMenuChange(QJsonDocument jsonDocument)
{
    QString newCurrentCollection = jsonDocument.object().toVariantHash().value("collection").toString();
    if (newCurrentCollection.compare(currentCollection) == 0) {
        return;
    }

    // source plugins aren't allowed to save collection-specific settings during collection change
    foreach (QUuid sourceId, okToSaveConfig.keys()) {
        okToSaveConfig[sourceId] = false;
    }

    currentCollection = newCurrentCollection;
    emit saveWaverSettings("", configToJsonGlobal());

    // load waver settings for new collection
    emit loadWaverSettings(currentCollection);

    // load plugin settings for new collection
    foreach (QUuid pluginId, sourcePlugins.keys()) {
        emit loadPluginSettings(pluginId, currentCollection);
    }

    // empty playlist
    QVector<Track *> tracksToBeDeleted;
    tracksToBeDeleted.append(playlistTracks);
    foreach (Track *track, tracksToBeDeleted) {
        playlistTracks.removeAll(track);
        emit done(track->getSourcePluginId(), track->getTrackInfo().url, false);
        delete track;
    }

    // stop current track
    currentTrack->interrupt();

    // UI signal
    sendPlaylistToClients();

    // next track from newly selected collection
    startNextTrack();
}


// private method
void WaverServer::handleCollectionsDialogResults(QJsonDocument jsonDocument)
{
    collections.clear();
    foreach (QVariant collection, jsonDocument.array()) {
        collections.append(collection.toString());
    }

    emit saveWaverSettings("", configToJsonGlobal());

    if (!collections.contains(currentCollection)) {
        handleCollectionMenuChange(QJsonDocument(QJsonObject::fromVariantHash(QVariantHash({
            {
                "collection", SettingsHandler::DEFAULT_COLLECTION_NAME
            }
        }))));
    }

    sendCollectionListToClients();
}


// private method
void WaverServer::handlePluginUIRequest(QJsonDocument jsonDocument)
{
    // get the plugin's id from message data
    QUuid uniqueId(jsonDocument.object().toVariantHash().value("plugin_id").toString());

    // let's just emit the signals blindly, only the plugin with the same id will respond anyways
    emit requestPluginUi(uniqueId);
}


// private method
void WaverServer::handlePluginUIResults(QJsonDocument jsonDocument)
{
    QVariantHash object = jsonDocument.object().toVariantHash();

    // *INDENT-OFF*
    emit pluginUiResults(QUuid(object.value("plugin_id").toString()), (
            (QString(object.value("ui_results").typeName()).compare("QVariantList") == 0) ? QJsonDocument(QJsonArray::fromVariantList(object.value("ui_results").toList()))  :
            (QString(object.value("ui_results").typeName()).compare("QVariantHash") == 0) ? QJsonDocument(QJsonObject::fromVariantHash(object.value("ui_results").toHash())) :
            (QString(object.value("ui_results").typeName()).compare("QVariantMap")  == 0) ? QJsonDocument(QJsonObject::fromVariantMap(object.value("ui_results").toMap()))   :
                                                                                            QJsonDocument(object.value("ui_results").toJsonObject())
    ));
    // *INDENT-ON*
}


// private method
void WaverServer::handleOpenTracksRequest(QJsonDocument jsonDocument)
{
    QVariantHash object = jsonDocument.object().toVariantHash();

    // if no plugin id is specified, then plugin list must be sent
    if (object.value("plugin_id").toString().length() < 1) {
        foreach (QUuid uuid, sourcePlugins.keys()) {
            OpenTrack openTrack;
            openTrack.hasChildren = true;
            openTrack.selectable  = false;
            openTrack.label       = Track::formatPluginName(sourcePlugins.value(uuid));
            openTrack.id          = "";

            OpenTracks openTracks;
            openTracks.append(openTrack);

            sendOpenTracksToClients(IpcMessageUtils::OpenTracks, uuid, openTracks);
        }

        return;
    }

    emit getOpenTracks(QUuid(object.value("plugin_id").toString()), object.value("parent_id").toString());
}


// private method
void WaverServer::handleOpenTracksSelection(QJsonDocument jsonDocument)
{
    QJsonArray jsonArray = jsonDocument.array();

    // must put into buckets by plugin
    QHash<QUuid, QStringList *> requestedTracks;
    foreach (QVariant track, jsonArray) {
        QVariantHash trackHash = track.toHash();
        QUuid        uuid      = QUuid(trackHash.value("plugin_id").toString());

        if (!requestedTracks.contains(uuid)) {
            requestedTracks.insert(uuid, new QStringList());
        }
        requestedTracks.value(uuid)->append(trackHash.value("track_id").toString());
    }

    // emit the signals
    foreach (QUuid pluginId, requestedTracks.keys()) {
        QStringList stringList;
        stringList.append(*requestedTracks.value(pluginId));
        delete requestedTracks.value(pluginId);

        emit resolveOpenTracks(pluginId, stringList);
    }
}


// private method
void WaverServer::handleSearchRequest(QJsonDocument jsonDocument)
{
    QString criteria = jsonDocument.object().toVariantHash().value("criteria").toString();

    foreach (QUuid uuid, sourcePlugins.keys()) {
        emit search(uuid, criteria);
    }
}


// private method
void WaverServer::handleSearchSelection(QJsonDocument jsonDocument)
{
    // same thing as open tracks
    handleOpenTracksSelection(jsonDocument);
}


// private method
void WaverServer::handleTrackActionsRequest(QJsonDocument jsonDocument)
{
    // get data from JSON
    int     index  = jsonDocument.object().toVariantHash().value("index").toInt();
    QString action = jsonDocument.object().toVariantHash().value("action").toString();

    // find track
    Track *track = (index < 0 ? currentTrack : playlistTracks.at(index));
    if (track == NULL) {
        return;
    }

    // action is plugin id and action id concatenated
    QStringList ids = action.split('~');

    // handle built-in actions
    if (ids.at(0).compare("s") == 0) {
        if (ids.at(1).compare("play_more") == 0) {
            track->addMoreToCastPlaytime();
            return;
        }
        if (ids.at(1).compare("play_forever") == 0) {
            track->addALotToCastPlaytime();
            return;
        }

        if (ids.at(1).compare("down") == 0) {
            playlistTracks.move(index, index + 1);
            sendPlaylistToClients(index + 1);
        }
        if (ids.at(1).compare("up") == 0) {
            playlistTracks.move(index, index - 1);
            sendPlaylistToClients(index - 1);
        }
        if (ids.at(1).compare("move") == 0) {
            bool OK = false;
            int moveTo = ids.at(2).toInt(&OK);
            if (OK) {
                playlistTracks.move(index, moveTo);
                sendPlaylistToClients(index - 1);
            }
        }

        if (ids.at(1).compare("remove") == 0) {
            Track *toBeRemoved = playlistTracks.at(index);
            playlistTracks.remove(index);
            emit done(toBeRemoved->getSourcePluginId(), toBeRemoved->getTrackInfo().url, false);
            delete toBeRemoved;
            sendPlaylistToClients();
        }

        // order has changed, must reassign fade in times based on previous track's request
        reassignFadeIns();
        return;
    }

    QUuid pluginId = QUuid(ids.at(0));

    bool OK;
    int  actionInt = ids.at(1).toInt(&OK);
    if (!OK) {
        // this should never happen
        return;
    }

    // is it for the source plugin?
    if (pluginId == track->getSourcePluginId()) {
        if (PluginLibsLoader::isPluginCompatible(sourcePlugins.value(pluginId).waverVersionAPICompatibility, "0.0.5")) {
            emit trackAction(pluginId, actionInt, track->getTrackInfo());
        }
        else {
            emit trackAction(pluginId, actionInt, track->getTrackInfo().url);
        }
        return;
    }

    // it's for some other plugin, pas it to the track
    emit trackTrackAction(pluginId, actionInt, track->getTrackInfo().url);
}


// private method
void WaverServer::handleDiagnostics(QJsonDocument jsonDocument)
{
    // one client can override another client which is good because we don't want to clog the system with too many diagnostics

    // get mode from JSON
    QString mode = jsonDocument.object().toVariantHash().value("mode").toString();

    // start sending diagnostics
    if (mode.compare("get") == 0) {
        // stop previous diagnostics
        stopAllDiagnostics();

        // get id from JSON
        QString id = jsonDocument.object().toVariantHash().value("id").toString();

        // error log is a special case
        if (id.compare("error_log") == 0) {
            sendErrorLogDiagnostics = true;
            sendErrorLogDiagnosticsToClients();
            return;
        }

        // start now
        diagnosticsTimer.start();
        lastDiagnosticsPluginId = QUuid(id);
        emit startDiagnostics(lastDiagnosticsPluginId);
    }

    // stop diagnostics
    if (mode.compare("done") == 0) {
        stopAllDiagnostics();
    }
}


// private method
void WaverServer::handleSourcePrioritiesRequest(QJsonDocument jsonDocument)
{
    Q_UNUSED(jsonDocument);

    QJsonArray jsonArray;
    foreach (QUuid pluginId, sourcePlugins.keys()) {
        jsonArray.append(QJsonObject::fromVariantHash(QVariantHash({
            {
                "id", pluginId.toString()
            },
            {
                "name", sourcePlugins.value(pluginId).name
            },
            {
                "priority", sourcePlugins.value(pluginId).priority
            },
            {
                "lovedMode", formatLovedMode(sourcePlugins.value(pluginId).lovedMode)
            },
            {
                "recurring", (recurringPlugin == pluginId)
            }
        })));
    }

    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::SourcePriorities, QJsonDocument(jsonArray)));
}


// private method
void WaverServer::handleSourcePrioritiesResult(QJsonDocument jsonDocument)
{
    recurringPlugin = QUuid();

    QVariantList data = jsonDocument.array().toVariantList();
    foreach (QVariant dataItem, data) {
        QVariantMap sourcePriority = dataItem.toMap();
        sourcePlugins[QUuid(sourcePriority.value("id").toString())].priority  = sourcePriority.value("priority").toInt();
        sourcePlugins[QUuid(sourcePriority.value("id").toString())].lovedMode = lovedModeFromString(sourcePriority.value("lovedMode").toString());
        if (sourcePriority.value("recurring").toBool()) {
            recurringPlugin = QUuid(sourcePriority.value("id").toString());
        }
    }

    emit saveWaverSettings(currentCollection, configToJson());
}


// private method
void WaverServer::handleOptionsRequest(QJsonDocument jsonDocument)
{
    Q_UNUSED(jsonDocument);

    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::Options, QJsonDocument(QJsonObject::fromVariantHash(QVariantHash({
        {
            "streamPlayTime", streamPlayTime
        },
        {
            "lovedStreamPlayTime", lovedStreamPlayTime
        },
        {
            "playlistAddMode", playlistAddMode
        },
        {
            "startupCollection", startupCollection
        }
    })))));
}


// private method
void WaverServer::handleOptionsResult(QJsonDocument jsonDocument)
{
    QVariantHash data = jsonDocument.object().toVariantHash();

    startupCollection = data.value("startupCollection").toString();

    emit saveWaverSettings("", configToJsonGlobal());

    streamPlayTime      = data.value("castPlayTime").toInt();
    lovedStreamPlayTime = data.value("castLovedPlayTime").toInt();
    playlistAddMode     = data.value("playlistAddMode").toInt();

    emit saveWaverSettings(currentCollection, configToJson());
}


// private method
void WaverServer::sendCollectionListToClients()
{
    QJsonArray collectionArray = QJsonArray::fromStringList(collections);

    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::CollectionList, QJsonDocument(QJsonObject::fromVariantHash(QVariantHash({
        {
            "collections", collectionArray
        },
        {
            "current_collection", currentCollection
        }
    })))));
}


// private method
void WaverServer::sendPlaylistToClients(int contextShowTrackIndex)
{
    IpcMessageUtils ipcMessageUtils;
    QJsonArray      playlist;

    foreach (Track *track, playlistTracks) {
        QJsonDocument info = ipcMessageUtils.trackInfoToJSONDocument(track->getTrackInfo(), track->getAdditionalInfo());
        playlist.append(info.object());
    }

    QVariantHash data({
        {
            "playlist", playlist
        },
        {
            "contextShowTrackIndex", contextShowTrackIndex
        }
    });

    // TODO make this able to send to specific client only (when client requests it at startup)
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::Playlist, QJsonDocument(QJsonObject::fromVariantHash(data))));
}

void WaverServer::sendPlaylistToClients()
{
    sendPlaylistToClients(-1);
}


// private method
void WaverServer::sendPluginsToClients()
{
    QVariantHash plugins;
    foreach (QUuid id, this->plugins.keys()) {
        plugins.insert(id.toString(), this->plugins.value(id));
    }

    // TODO make this able to send to specific client only (when client requests it at startup)
    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::Plugins, QJsonDocument(QJsonObject::fromVariantHash(plugins))));
}


// private method
void WaverServer::sendPluginsWithUiToClients()
{
    QVariantHash plugins;
    foreach (QUuid id, pluginsWithUI.keys()) {
        plugins.insert(id.toString(), pluginsWithUI.value(id));
    }

    // TODO make this able to send to specific client only (when client requests it at startup)
    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::PluginsWithUI, QJsonDocument(QJsonObject::fromVariantHash(plugins))));
}


// private method
void WaverServer::sendOpenTracksToClients(IpcMessageUtils::IpcMessages message, QUuid uniqueId, OpenTracks openTracks)
{
    QJsonArray tracksJson;
    foreach (OpenTrack track, openTracks) {
        QVariantHash trackHash;
        trackHash.insert("hasChildren", track.hasChildren);
        trackHash.insert("selectable",  track.selectable);
        trackHash.insert("label",       track.label);
        trackHash.insert("id",          track.id);

        tracksJson.append(QJsonObject::fromVariantHash(trackHash));
    }

    QVariantHash data({
        {
            "plugin_id", uniqueId.toString()
        },
        {
            "tracks", tracksJson
        }
    });

    // TODO make this able to send to specific client only (when client requests it at startup)
    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(message, QJsonDocument(QJsonObject::fromVariantHash(data))));
}


// private method
void WaverServer::sendErrorLogDiagnosticsToClients()
{
    QJsonArray errorLogJson;
    foreach (ErrorLogItem errorLogItem, errorLog) {
        QVariantHash errorLogItemHash;
        errorLogItemHash.insert("fatal",     errorLogItem.fatal);
        errorLogItemHash.insert("message",   errorLogItem.message);
        errorLogItemHash.insert("timestamp", errorLogItem.timestamp.toString("hh:mm:ss.zzz"));
        errorLogItemHash.insert("title",     errorLogItem.title);

        errorLogJson.append(QJsonObject::fromVariantHash(errorLogItemHash));
    }

    if (errorLogJson.count() < 1) {
        QVariantHash errorLogItemHash;
        errorLogItemHash.insert("fatal",     false);
        errorLogItemHash.insert("message",   "No errors");
        errorLogItemHash.insert("timestamp", QDateTime::currentDateTime().toString("hh:mm:ss.zzz"));
        errorLogItemHash.insert("title",     "");

        errorLogJson.append(QJsonObject::fromVariantHash(errorLogItemHash));
    }

    QVariantHash data({
        {
            "id", "error_log"
        },
        {
            "data", errorLogJson
        }
    });

    // TODO make this able to send to specific client only
    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::Diagnostics, QJsonDocument(QJsonObject::fromVariantHash(data))));
}


// plugin libraries loader signal handler
void WaverServer::pluginLibsLoaded()
{
    // load source plugins
    foreach (PluginLibsLoader::LoadedLib loadedLib, loadedLibs) {

        // call library's plugin factory (only want source plugins here)
        PluginFactoryResults plugins;
        loadedLib.pluginFactory(PLUGIN_TYPE_SOURCE, &plugins);

        // process each plugin one by one
        foreach (QObject *plugin, plugins) {
            // get the ID of the plugin
            QUuid persistentUniqueId;
            if (!plugin->metaObject()->invokeMethod(plugin, "persistentUniqueId", Qt::DirectConnection, Q_RETURN_ARG(QUuid, persistentUniqueId))) {
                finish("Failed to invoke method on plugin");
            }

            // just to be on the safe side
            if (sourcePlugins.contains(persistentUniqueId)) {
                continue;
            }

            okToSaveConfig.insert(persistentUniqueId, false);

            // remember some info about the plugin
            SourcePlugin pluginData;
            pluginData.ready     = false;
            pluginData.priority  = 4;
            pluginData.lovedMode = LOVE_NORMAL;
            if (!plugin->metaObject()->invokeMethod(plugin, "pluginName", Qt::DirectConnection, Q_RETURN_ARG(QString, pluginData.name))) {
                finish("Failed to invoke method on plugin");
            }
            if (!plugin->metaObject()->invokeMethod(plugin, "pluginVersion", Qt::DirectConnection, Q_RETURN_ARG(int, pluginData.version))) {
                finish("Failed to invoke method on plugin");
            }
            if (!plugin->metaObject()->invokeMethod(plugin, "waverVersionAPICompatibility", Qt::DirectConnection, Q_RETURN_ARG(QString, pluginData.waverVersionAPICompatibility))) {
                finish("Failed to invoke method on plugin");
            }
            if (!plugin->metaObject()->invokeMethod(plugin, "hasUI", Qt::DirectConnection, Q_RETURN_ARG(bool, pluginData.hasUI))) {
                finish("Failed to invoke method on plugin");
            }
            sourcePlugins[persistentUniqueId] = pluginData;

            // initializations
            if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.4")) {
                if (!plugin->metaObject()->invokeMethod(plugin, "setUserAgent", Qt::DirectConnection, Q_ARG(QString, Globals::userAgent()))) {
                    finish("Failed to invoke method on plugin");
                }
            }

            // move to sources thread
            plugin->moveToThread(&sourcesThread);

            // connect thread signals
            connect(&sourcesThread, SIGNAL(started()),  plugin, SLOT(run()));
            connect(&sourcesThread, SIGNAL(finished()), plugin, SLOT(deleteLater()));

            // connect source plugin signals
            connect(plugin, SIGNAL(ready(QUuid)),                                       this,   SLOT(pluginReady(QUuid)));
            connect(plugin, SIGNAL(unready(QUuid)),                                     this,   SLOT(pluginUnready(QUuid)));
            connect(plugin, SIGNAL(saveConfiguration(QUuid, QJsonDocument)),            this,   SLOT(saveConfiguration(QUuid, QJsonDocument)));
            connect(plugin, SIGNAL(loadConfiguration(QUuid)),                           this,   SLOT(loadConfiguration(QUuid)));
            connect(plugin, SIGNAL(uiQml(QUuid, QString)),                              this,   SLOT(pluginUi(QUuid, QString)));
            connect(plugin, SIGNAL(infoMessage(QUuid, QString)),                        this,   SLOT(pluginInfoMessage(QUuid, QString)));
            connect(plugin, SIGNAL(requestRemoveTracks(QUuid)),                         this,   SLOT(requestedRemoveTracks(QUuid)));
            connect(plugin, SIGNAL(openTracksResults(QUuid, OpenTracks)),               this,   SLOT(openTracksResults(QUuid, OpenTracks)));
            connect(plugin, SIGNAL(searchResults(QUuid, OpenTracks)),                   this,   SLOT(searchResults(QUuid, OpenTracks)));
            connect(this,   SIGNAL(unableToStart(QUuid, QUrl)),                         plugin, SLOT(unableToStart(QUuid, QUrl)));
            connect(this,   SIGNAL(loadedConfiguration(QUuid, QJsonDocument)),          plugin, SLOT(loadedConfiguration(QUuid, QJsonDocument)));
            connect(this,   SIGNAL(requestPluginUi(QUuid)),                             plugin, SLOT(getUiQml(QUuid)));
            connect(this,   SIGNAL(pluginUiResults(QUuid, QJsonDocument)),              plugin, SLOT(uiResults(QUuid, QJsonDocument)));
            connect(this,   SIGNAL(getOpenTracks(QUuid, QString)),                      plugin, SLOT(getOpenTracks(QUuid, QString)));
            connect(this,   SIGNAL(search(QUuid, QString)),                             plugin, SLOT(search(QUuid, QString)));
            connect(this,   SIGNAL(resolveOpenTracks(QUuid, QStringList)),              plugin, SLOT(resolveOpenTracks(QUuid, QStringList)));
            if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.3")) {
                connect(plugin, SIGNAL(saveGlobalConfiguration(QUuid, QJsonDocument)),   this,   SLOT(saveGlobalConfiguration(QUuid, QJsonDocument)));
                connect(plugin, SIGNAL(loadGlobalConfiguration(QUuid)),                  this,   SLOT(loadGlobalConfiguration(QUuid)));
                connect(plugin, SIGNAL(requestRemoveTrack(QUuid, QUrl)),                 this,   SLOT(requestedRemoveTrack(QUuid, QUrl)));
                connect(this,   SIGNAL(loadedGlobalConfiguration(QUuid, QJsonDocument)), plugin, SLOT(loadedGlobalConfiguration(QUuid, QJsonDocument)));
            }
            if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.4")) {
                connect(plugin, SIGNAL(replacement(QUuid, TrackInfo)),                                      this,   SLOT(replacement(QUuid, TrackInfo)));
                connect(plugin, SIGNAL(diagnostics(QUuid, DiagnosticData)),                                 this,   SLOT(pluginDiagnostics(QUuid, DiagnosticData)));
                connect(plugin, SIGNAL(executeSql(QUuid, bool, QString, int, QString, QVariantList)),       this,   SLOT(executeSql(QUuid, bool, QString, int, QString, QVariantList)));
                connect(plugin, SIGNAL(executeGlobalSql(QUuid, bool, QString, int, QString, QVariantList)), this,   SLOT(executeGlobalSql(QUuid, bool, QString, int, QString, QVariantList)));
                connect(plugin, SIGNAL(updateTrackInfo(QUuid, TrackInfo)),                                  this,   SLOT(pluginUpdateTrackInfo(QUuid, TrackInfo)));
                connect(this,   SIGNAL(castFinishedEarly(QUuid, QUrl, int)),                                plugin, SLOT(castFinishedEarly(QUuid, QUrl, int)));
                connect(this,   SIGNAL(getReplacement(QUuid)),                                              plugin, SLOT(getReplacement(QUuid)));
                connect(this,   SIGNAL(startDiagnostics(QUuid)),                                            plugin, SLOT(startDiagnostics(QUuid)));
                connect(this,   SIGNAL(stopDiagnostics(QUuid)),                                             plugin, SLOT(stopDiagnostics(QUuid)));
                connect(this,   SIGNAL(executedSqlResults(QUuid, bool, QString, int, SqlResults)),          plugin, SLOT(sqlResults(QUuid, bool, QString, int, SqlResults)));
                connect(this,   SIGNAL(executedGlobalSqlResults(QUuid, bool, QString, int, SqlResults)),    plugin, SLOT(globalSqlResults(QUuid, bool, QString, int, SqlResults)));
                connect(this,   SIGNAL(executedSqlError(QUuid, bool, QString, int, QString)),               plugin, SLOT(sqlError(QUuid, bool, QString, int, QString)));
            }
            if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.5")) {
                connect(plugin, SIGNAL(playlist(QUuid, TracksInfo, ExtraInfo)), this,   SLOT(playlist(QUuid, TracksInfo, ExtraInfo)));
                connect(plugin, SIGNAL(openUrl(QUrl)),                          this,   SLOT(sourceOpenUrl(QUrl)));
                connect(this,   SIGNAL(getPlaylist(QUuid, int, int)),           plugin, SLOT(getPlaylist(QUuid, int, int)));
                connect(this,   SIGNAL(trackAction(QUuid, int, TrackInfo)),     plugin, SLOT(action(QUuid, int, TrackInfo)));
            }
            else {
                connect(plugin, SIGNAL(playlist(QUuid, TracksInfo)),   this,   SLOT(playlist(QUuid, TracksInfo)));
                connect(this,   SIGNAL(getPlaylist(QUuid, int)),       plugin, SLOT(getPlaylist(QUuid, int)));
                connect(this,   SIGNAL(trackAction(QUuid, int, QUrl)), plugin, SLOT(action(QUuid, int, QUrl)));
            }
            if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.6")) {
                connect(this, SIGNAL(done(QUuid, QUrl, bool)), plugin, SLOT(done(QUuid, QUrl, bool)));
            }
        }
    }

    // get the priorities
    emit loadWaverSettings(currentCollection);

    // start source thread and hance source plugins
    sourcesThread.start();
}


// plugin libraries loader signal handler
void WaverServer::pluginLibsFailInfo(QString info)
{
    outputError(info, "", true);
}


// interprocess communication signal handler
void WaverServer::ipcReceivedMessage(IpcMessageUtils::IpcMessages message, QJsonDocument jsonDocument)
{
    IpcMessageUtils ipcMessageUtils;

    // do what needs to be done
    switch (message) {

        case IpcMessageUtils::CollectionList:
            sendCollectionListToClients();
            break;

        case IpcMessageUtils::CollectionMenuChange:
            handleCollectionMenuChange(jsonDocument);
            break;

        case IpcMessageUtils::CollectionsDialogResults:
            handleCollectionsDialogResults(jsonDocument);
            break;

        case IpcMessageUtils::Diagnostics:
            handleDiagnostics(jsonDocument);
            break;

        case IpcMessageUtils::Next:
            if (playlistTracks.count() > 0) {
                playlistTracks.at(0)->startWithoutFadeIn();
            }
            if (currentTrack != NULL) {
                currentTrack->setReplacable(false);
                currentTrack->setStatus(Track::Paused);
                currentTrack->interrupt();
                return;
            }
            startNextTrack();
            break;

        case IpcMessageUtils::OpenTracks:
            handleOpenTracksRequest(jsonDocument);
            break;

        case IpcMessageUtils::OpenTracksSelected:
            handleOpenTracksSelection(jsonDocument);
            break;

        case IpcMessageUtils::Pause:
            if (currentTrack != NULL) {
                // current track is always either playing or paused
                currentTrack->setStatus(Track::Paused);
                emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::Pause));
            }
            // shut down crossfade if in progress
            if (previousTrack != NULL) {
                showPreviousTime = false;
                previousTrack->setStatus(Track::Paused);
                previousTrack->interrupt();
            }
            break;

        case IpcMessageUtils::Playlist:
            sendPlaylistToClients();
            break;

        case IpcMessageUtils::PlayPauseState:
            if (currentTrack != NULL) {
                if (currentTrack->status() == Track::Paused) {
                    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::Pause));
                    break;
                }
            }
            emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::Resume));
            break;

        case IpcMessageUtils::Plugins:
            sendPluginsToClients();
            break;

        case IpcMessageUtils::PluginsWithUI:
            sendPluginsWithUiToClients();
            break;

        case IpcMessageUtils::PluginUI:
            handlePluginUIRequest(jsonDocument);
            break;

        case IpcMessageUtils::PluginUIResults:
            handlePluginUIResults(jsonDocument);
            break;

        case IpcMessageUtils::Quit:
            finish();
            break;

        case IpcMessageUtils::Resume:
            if (currentTrack != NULL) {
                // current track is always either playing or paused
                currentTrack->setStatus(Track::Playing);
                emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::Resume));
            }
            break;

        case IpcMessageUtils::Search:
            handleSearchRequest(jsonDocument);
            break;

        case IpcMessageUtils::SourcePriorities:
            handleSourcePrioritiesRequest(jsonDocument);
            break;

        case IpcMessageUtils::SourcePriorityResults:
            handleSourcePrioritiesResult(jsonDocument);
            break;

        case IpcMessageUtils::Options:
            handleOptionsRequest(jsonDocument);
            break;

        case IpcMessageUtils::OptionsResults:
            handleOptionsResult(jsonDocument);
            break;

        case IpcMessageUtils::TrackAction:
            handleTrackActionsRequest(jsonDocument);
            break;

        case IpcMessageUtils::TrackInfos:
            if (currentTrack != NULL) {
                emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::TrackInfos, ipcMessageUtils.trackInfoToJSONDocument(currentTrack->getTrackInfo(), currentTrack->getAdditionalInfo())));
            }
            break;

        default:
            break;
    }
}


// interprocess communication signal handler
void WaverServer::ipcReceivedUrl(QUrl url)
{
    // TODO implement this
}


// interprocess communication signal handler
void WaverServer::ipcReceivedError(bool fatal, QString error)
{
    outputError(error, "interprocess communications", fatal);
}


// interprocess communication signal handler
void WaverServer::ipcNoClient()
{
    stopAllDiagnostics();
}


// settings storage signal handler
void WaverServer::loadedWaverGlobalSettings(QJsonDocument settings)
{
    bool justStarted = this->currentCollection.isEmpty();

    jsonToConfigGlobal(settings);

    // settings storage sends this signal right after it just started at server startup
    if (justStarted) {
        // figure which collection to use
        if (startupCollection.compare(Globals::lastUsedCollectionOption()) != 0) {
            if (collections.contains(startupCollection)) {
                currentCollection = startupCollection;
            }
            else {
                currentCollection = SettingsHandler::DEFAULT_COLLECTION_NAME;
            }
        }

        // instantiate, set up, and start plugin library loader (it sends finished right after plugins are loaded)

        PluginLibsLoader *pluginLibsLoader = new PluginLibsLoader(NULL, &loadedLibs);
        pluginLibsLoader->moveToThread(&pluginLibsLoaderThread);

        connect(&pluginLibsLoaderThread, SIGNAL(started()),  pluginLibsLoader, SLOT(run()));
        connect(&pluginLibsLoaderThread, SIGNAL(finished()), pluginLibsLoader, SLOT(deleteLater()));

        connect(pluginLibsLoader, SIGNAL(finished()),        &pluginLibsLoaderThread, SLOT(quit()));
        connect(pluginLibsLoader, SIGNAL(finished()),        this,                    SLOT(pluginLibsLoaded()));
        connect(pluginLibsLoader, SIGNAL(failInfo(QString)), this,                    SLOT(pluginLibsFailInfo(QString)));

        pluginLibsLoaderThread.start();

        // nothing else to do now
        return;
    }
}


// settings storage signal handler
void WaverServer::loadedWaverSettings(QJsonDocument settings)
{
    jsonToConfig(settings);
}


// settings storage signal handler
void WaverServer::loadedPluginSettings(QUuid uniqueId, QJsonDocument settings)
{
    // nothing much to do, just re-emit for sources and tracks
    emit loadedConfiguration(uniqueId, settings);

    // it's OK now for sources to save configuration
    okToSaveConfig[uniqueId] = true;
}


// settings storage signal handler
void WaverServer::loadedPluginGlobalSettings(QUuid uniqueId, QJsonDocument settings)
{
    // nothing much to do, just re-emit for sources and tracks
    emit loadedGlobalConfiguration(uniqueId, settings);
}


// settings storage signal handler
void WaverServer::executedPluginSqlResults(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    // nothing much to do, just re-emit for sources and tracks
    emit executedSqlResults(uniqueId, temporary, clientIdentifier, clientSqlIdentifier, results);
}


// settings storage signal handler
void WaverServer::executedPluginGlobalSqlResults(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    // nothing much to do, just re-emit for sources and tracks
    emit executedGlobalSqlResults(uniqueId, temporary, clientIdentifier, clientSqlIdentifier, results);
}


// settings storage signal handler
void WaverServer::executedPluginSqlError(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
{
    // nothing much to do, just re-emit for sources and tracks
    emit executedSqlError(uniqueId, temporary, clientIdentifier, clientSqlIdentifier, error);
}


// track signal handler
void WaverServer::pluginUi(QUuid uniqueId, QString qml, QString header)
{
    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::PluginUI,
    QJsonDocument(QJsonObject::fromVariantHash(QVariantHash({
        {
            "plugin_id", uniqueId.toString()
        },
        {
            "ui_qml", qml
        },
        {
            "header", header
        }
    })))));
}


// source plugin signal handler
void WaverServer::WaverServer::pluginUi(QUuid uniqueId, QString qml)
{
    QString header;
    if (sourcePlugins.contains(uniqueId)) {
        header = Track::formatPluginName(sourcePlugins.value(uniqueId), true);
    }

    pluginUi(uniqueId, qml, header);
}



// source plugin signal handler
void WaverServer::pluginReady(QUuid uniqueId)
{
    // parameter check
    if (!sourcePlugins.contains(uniqueId)) {
        return;
    }

    // this is for prioritizing local source plugin (see in requestPlaylist)
    if (waitingForLocalSource && !waitingForLocalSourceTimerStarted) {
        waitingForLocalSourceTimerStarted = true;
        QTimer::singleShot(1500, this, SLOT(notWaitingForLocalSourceAnymore()));
    }

    // remember
    sourcePlugins[uniqueId].ready = true;

    // get tracks now if needed
    if (playlistTracks.count() <= 2) {
        requestPlaylist();
    }
}


// timer signal handler
void WaverServer::notWaitingForLocalSourceAnymore()
{
    waitingForLocalSource = false;

    if (playlistTracks.count() <= 2) {
        requestPlaylist();
    }
}


// timer signal handler
void WaverServer::delayedRequestPlaylist()
{
    if (playlistTracks.count() <= 2) {
        requestPlaylist();
    }
}


// source plugin signal handler
void WaverServer::pluginUnready(QUuid uniqueId)
{
    // parameter check
    if (!sourcePlugins.contains(uniqueId)) {
        return;
    }

    // remember
    sourcePlugins[uniqueId].ready = false;
}


// source plugin signal handler
void WaverServer::pluginInfoMessage(QUuid uniqueId, QString message)
{
    outputError(message, sourcePlugins.value(uniqueId).name, false);
}


// source plugin signal handler
void WaverServer::pluginUpdateTrackInfo(QUuid uniqueId, TrackInfo trackInfo)
{
    if ((previousTrack != NULL) && (previousTrack->getTrackInfo().url == trackInfo.url)) {
        previousTrack->infoUpdateTrackInfo(uniqueId, trackInfo);
    }
    if ((currentTrack != NULL) && (currentTrack->getTrackInfo().url == trackInfo.url)) {
        currentTrack->infoUpdateTrackInfo(uniqueId, trackInfo);
    }
    foreach (Track *track, playlistTracks) {
        if (track->getTrackInfo().url == trackInfo.url) {
            track->infoUpdateTrackInfo(uniqueId, trackInfo);
        }
    }
}


// source plugin signal handler
void WaverServer::pluginDiagnostics(QUuid uniqueId, DiagnosticData data)
{
    pluginDiagnostics(uniqueId, QUrl(), data);
}


// plugin signal handler (this comes from track)
void WaverServer::pluginDiagnostics(QUuid uniqueId, QUrl url, DiagnosticData data)
{
    // don't accept from other then what we're looking at
    if (uniqueId != lastDiagnosticsPluginId) {
        return;
    }

    // aggregate diagnostics because
    // 1. plugins aren't required to send all of their data items in every signal
    // 2. to be able to limit client refreshes to 20 fps
    foreach (DiagnosticItem diagnosticItem, data) {
        bool found = false;
        int  i     = 0;
        while ((i < diagnosticsAggregator.value(url).count()) && !found) {
            if (diagnosticsAggregator.value(url).at(i).label.compare(diagnosticItem.label) == 0) {
                diagnosticsAggregator[url][i].message = diagnosticItem.message;
                found = true;
            }
            i++;
        }
        if (!found) {
            diagnosticsAggregator[url].append(diagnosticItem);
        }
    }

    diagnosticsChanged = true;
}


// private slot for the diagnostics timer
void WaverServer::diagnosticsRefreshUI()
{
    // see if anything changed
    if (!diagnosticsChanged) {
        return;
    }
    diagnosticsChanged = false;

    // construct data
    QVariantHash  urlsDataHash;
    QVector<QUrl> toBeRemoved;
    foreach (QUrl url, diagnosticsAggregator.keys()) {
        QString title = url.isEmpty() ? "~" : findTitleFromUrl(url);

        // previous track is finished
        if (title.length() < 1) {
            toBeRemoved.append(url);
            continue;
        }

        // convert to JSON
        QJsonArray dataJson;
        foreach (DiagnosticItem diagnosticItem, diagnosticsAggregator.value(url)) {
            QVariantHash diagnosticItemHash;
            diagnosticItemHash.insert("label",     diagnosticItem.label);
            diagnosticItemHash.insert("message",   diagnosticItem.message);

            dataJson.append(QJsonObject::fromVariantHash(diagnosticItemHash));
        }

        urlsDataHash.insert(title, dataJson);
    }

    // remove tracks that are finished
    foreach (QUrl url, toBeRemoved) {
        diagnosticsAggregator.remove(url);
    }

    // construct message data
    QVariantHash dataHash({
        {
            "id", lastDiagnosticsPluginId.toString()
        },
        {
            "data", QJsonObject::fromVariantHash(urlsDataHash)
        }
    });

    // TODO make this able to send to specific client only
    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::Diagnostics, QJsonDocument(QJsonObject::fromVariantHash(dataHash))));

}


// plugin signal handler (this can come from source or track)
void WaverServer::loadConfiguration(QUuid uniqueId)
{
    // re-emit for settings storage handler
    emit loadPluginSettings(uniqueId, currentCollection);
}


// plugin signal handler (this can come from source or track)
void WaverServer::loadGlobalConfiguration(QUuid uniqueId)
{
    // re-emit for settings storage handler
    emit loadPluginSettings(uniqueId, "");
}


// plugin signal handler (this can come from source or track)
void WaverServer::saveConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    // parameter check
    if (configuration.isEmpty()) {
        return;
    }
    if (okToSaveConfig.contains(uniqueId) && !okToSaveConfig.value(uniqueId)) {
        outputError(QString("Denied saving collection specific configuration of %1").arg(sourcePlugins.value(uniqueId).name), "Server info", false);
        return;
    }

    // re-emit for settings storage handler
    emit savePluginSettings(uniqueId, currentCollection, configuration);
}


// plugin signal handler (this can come from source or track)
void WaverServer::saveGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    // parameter check
    if (configuration.isEmpty()) {
        return;
    }

    // re-emit for settings storage handler
    emit savePluginSettings(uniqueId, "", configuration);
}


// plugin signal handler (this can come from source or track)
void WaverServer::executeSql(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString sql, QVariantList values)
{
    // parameter check
    if (sql.isEmpty()) {
        return;
    }

    // re-emit for settings storage handler
    executeSettingsSql(uniqueId, currentCollection, temporary, clientIdentifier, clientSqlIdentifier, sql, values);
}


// plugin signal handler (this can come from source or track)
void WaverServer::executeGlobalSql(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString sql, QVariantList values)
{
    // parameter check
    if (sql.isEmpty()) {
        return;
    }

    // re-emit for settings storage handler
    executeSettingsSql(uniqueId, "", temporary, clientIdentifier, clientSqlIdentifier, sql, values);
}


// source plugin signal handler
void WaverServer::playlist(QUuid uniqueId, TracksInfo tracksInfo)
{
    ExtraInfo extraInfo;
    playlist(uniqueId, tracksInfo, extraInfo);
}


// source plugin signal handler
void WaverServer::playlist(QUuid uniqueId, TracksInfo tracksInfo, ExtraInfo extraInfo)
{
    Globals::consoleOutput(QString("Received %1 tracks from %2").arg(tracksInfo.count()).arg(sourcePlugins.value(uniqueId).name), false);

    bool stopCurrentTrack  = false;
    int  addBeginningIndex = 0;
    if ((playlistTracks.count() > 0) && (playlistTracks.at(0)->getTrackInfo().cast) && (playlistTracks.at(0)->status() != Track::Idle)) {
        addBeginningIndex = 1;
    }
    foreach (TrackInfo trackInfo, tracksInfo) {
        // handle extra info
        QVariantHash additionalInfo;
        if (extraInfo.contains(trackInfo.url)) {
            additionalInfo = extraInfo.value(trackInfo.url);
        }

        // create track
        Track *track = createTrack(trackInfo, additionalInfo, uniqueId);

        // add to playlist
        if (extraInfo.contains(trackInfo.url) && extraInfo.value(trackInfo.url).contains("resolved_open_track") && extraInfo.value(trackInfo.url).value("resolved_open_track").toInt()) {
            if (playlistAddMode == PLAYLIST_ADD_BEGINNING) {
                playlistTracks.insert(addBeginningIndex, track);
                addBeginningIndex++;
            }
            else if (playlistAddMode == PLAYLIST_ADD_START_IMMEDIATELY) {
                playlistTracks.insert(addBeginningIndex, track);
                addBeginningIndex++;
                stopCurrentTrack = true;
            }
            else {
                playlistTracks.append(track);
            }
        }
        else {
            playlistTracks.append(track);
        }
    }

    // diagnostics?
    if (!lastDiagnosticsPluginId.isNull()) {
        emit startDiagnostics(lastDiagnosticsPluginId);
    }

    // make sure there's at least one track waiting (source might returned an empty list in error)
    if (playlistTracks.count() < 1) {
        requestPlaylist();
        return;
    }

    // UI signal
    sendPlaylistToClients();

    // this will not do anything if a track is already playing
    startNextTrack();

    // start immediately mode
    if (stopCurrentTrack && (currentTrack != NULL)) {
        currentTrack->interrupt();
    }
}


// source plugin signal handler
void WaverServer::replacement(QUuid uniqueId, TrackInfo trackInfo)
{
    Globals::consoleOutput(QString("Received replacement track from %1").arg(sourcePlugins.value(uniqueId).name), false);

    // create track
    Track *track = createTrack(trackInfo, QVariantHash(), uniqueId);

    // find the position to insert
    int insertPosition = 0;
    while ((insertPosition < playlistTracks.count()) && (playlistTracks.at(insertPosition)->getSourcePluginId() == uniqueId) && (playlistTracks.at(0)->status() != Track::Idle)) {
        insertPosition++;
    }

    // insert track to playlist
    playlistTracks.insert(insertPosition, track);

    // take care of fades
    reassignFadeIns();

    // UI signal
    sendPlaylistToClients();
}


// helper
Track *WaverServer::createTrack(TrackInfo trackInfo, QVariantHash additionalInfo, QUuid pluginId)
{
    Track *track = new Track(&loadedLibs, trackInfo, additionalInfo, streamPlayTime, lovedStreamPlayTime, pluginId, this);

    connect(this,  SIGNAL(loadedConfiguration(QUuid, QJsonDocument)),                                  track, SLOT(loadedPluginSettings(QUuid, QJsonDocument)));
    connect(this,  SIGNAL(loadedGlobalConfiguration(QUuid, QJsonDocument)),                            track, SLOT(loadedPluginGlobalSettings(QUuid, QJsonDocument)));
    connect(this,  SIGNAL(executedSqlResults(QUuid, bool, QString, int, SqlResults)),                  track, SLOT(executedPluginSqlResults(QUuid, bool, QString, int, SqlResults)));
    connect(this,  SIGNAL(executedGlobalSqlResults(QUuid, bool, QString, int, SqlResults)),            track, SLOT(executedPluginGlobalSqlResults(QUuid, bool, QString, int, SqlResults)));
    connect(this,  SIGNAL(executedSqlError(QUuid, bool, QString, int, QString)),                       track, SLOT(executedPluginSqlError(QUuid, bool, QString, int, QString)));
    connect(this,  SIGNAL(startDiagnostics(QUuid)),                                                    track, SLOT(startPluginDiagnostics(QUuid)));
    connect(this,  SIGNAL(stopDiagnostics(QUuid)),                                                     track, SLOT(stopPluginDiagnostics(QUuid)));
    connect(this,  SIGNAL(trackTrackAction(QUuid, int, QUrl)),                                         track, SLOT(trackActionRequest(QUuid, int, QUrl)));
    connect(track, SIGNAL(savePluginSettings(QUuid, QJsonDocument)),                                   this,  SLOT(saveConfiguration(QUuid, QJsonDocument)));
    connect(track, SIGNAL(savePluginGlobalSettings(QUuid, QJsonDocument)),                             this,  SLOT(saveGlobalConfiguration(QUuid, QJsonDocument)));
    connect(track, SIGNAL(loadPluginSettings(QUuid)),                                                  this,  SLOT(loadConfiguration(QUuid)));
    connect(track, SIGNAL(loadPluginGlobalSettings(QUuid)),                                            this,  SLOT(loadGlobalConfiguration(QUuid)));
    connect(track, SIGNAL(executeSettingsSql(QUuid, bool, QString, int, QString, QVariantList)),       this,  SLOT(executeSql(QUuid, bool, QString, int, QString, QVariantList)));
    connect(track, SIGNAL(executeGlobalSettingsSql(QUuid, bool, QString, int, QString, QVariantList)), this,  SLOT(executeGlobalSql(QUuid, bool, QString, int, QString, QVariantList)));
    connect(track, SIGNAL(requestFadeInForNextTrack(QUrl, qint64)),                                    this,  SLOT(trackRequestFadeInForNextTrack(QUrl, qint64)));
    connect(track, SIGNAL(requestAboutToFinishSendForPreviousTrack(QUrl, qint64)),                     this,  SLOT(trackRequestAboutToFinishSendForPreviousTrack(QUrl, qint64)));
    connect(track, SIGNAL(playPosition(QUrl, bool, long, long)),                                       this,  SLOT(trackPosition(QUrl, bool, long, long)));
    connect(track, SIGNAL(aboutToFinish(QUrl)),                                                        this,  SLOT(trackAboutToFinish(QUrl)));
    connect(track, SIGNAL(finished(QUrl)),                                                             this,  SLOT(trackFinished(QUrl)));
    connect(track, SIGNAL(trackInfoUpdated(QUrl)),                                                     this,  SLOT(trackInfoUpdated(QUrl)));
    connect(track, SIGNAL(error(QUrl, bool, QString)),                                                 this,  SLOT(trackError(QUrl, bool, QString)));
    connect(track, SIGNAL(loadedPlugins(Track::PluginList)),                                           this,  SLOT(trackLoadedPlugins(Track::PluginList)));
    connect(track, SIGNAL(loadedPluginsWithUI(Track::PluginList)),                                     this,  SLOT(trackLoadedPluginsWithUI(Track::PluginList)));
    connect(track, SIGNAL(pluginDiagnostics(QUuid, QUrl, DiagnosticData)),                             this,  SLOT(pluginDiagnostics(QUuid, QUrl, DiagnosticData)));

    return track;
}


// source plugin signal handler
void WaverServer::openTracksResults(QUuid uniqueId, OpenTracks openTracks)
{
    sendOpenTracksToClients(IpcMessageUtils::OpenTracks, uniqueId, openTracks);
}


// source plugin signal handler
void WaverServer::searchResults(QUuid uniqueId, OpenTracks openTracks)
{
    sendOpenTracksToClients(IpcMessageUtils::Search, uniqueId, openTracks);
}


// source plugin signal handler
void WaverServer::requestedRemoveTracks(QUuid uniqueId)
{
    // remove from playlist
    QVector<Track *> tracksToBeDeleted;
    foreach (Track *track, playlistTracks) {
        if (track->getSourcePluginId() == uniqueId) {
            tracksToBeDeleted.append(track);
        }
    }
    foreach (Track *track, tracksToBeDeleted) {
        playlistTracks.removeAll(track);
        emit done(track->getSourcePluginId(), track->getTrackInfo().url, false);
        delete track;
    }
    sendPlaylistToClients();

    // check current track
    if ((currentTrack != NULL) && (currentTrack->getSourcePluginId() == uniqueId)) {
        currentTrack->interrupt();
    }
}


// source plugin signal handler
void WaverServer::requestedRemoveTrack(QUuid uniqueId, QUrl url)
{
    // remove from playlist
    QVector<Track *> tracksToBeDeleted;
    foreach (Track *track, playlistTracks) {
        if ((track->getSourcePluginId() == uniqueId) && (track->getTrackInfo().url == url)) {
            tracksToBeDeleted.append(track);
        }
    }
    foreach (Track *track, tracksToBeDeleted) {
        playlistTracks.removeAll(track);
        emit done(track->getSourcePluginId(), track->getTrackInfo().url, false);
        delete track;
    }
    sendPlaylistToClients();

    // check current track
    if ((currentTrack != NULL) && (currentTrack->getSourcePluginId() == uniqueId) && (currentTrack->getTrackInfo().url == url)) {
        currentTrack->interrupt();
    }
}

// source plugin signal handler
void WaverServer::sourceOpenUrl(QUrl urlToOpen)
{
    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::OpenUrl, QJsonDocument(QJsonObject({{ "url", urlToOpen.toString() }}))));
}

// track signal handler
void WaverServer::trackRequestFadeInForNextTrack(QUrl url, qint64 lengthMilliseconds)
{
    // pass it on to next track; source can be either the current track or the first track in the playlist

    if ((playlistTracks.count() > 0) && (url == currentTrack->getTrackInfo().url)) {
        playlistTracks.at(0)->startWithFadeIn(lengthMilliseconds);
        return;
    }

    if ((playlistTracks.count() > 1) && (url == playlistTracks.at(0)->getTrackInfo().url)) {
        playlistTracks.at(1)->startWithFadeIn(lengthMilliseconds);
    }
}


// track signal handler
void WaverServer::trackRequestAboutToFinishSendForPreviousTrack(QUrl url, qint64 posBeforeEndMilliseconds)
{
    // pass it on to current track; source can only be the first track in the playlist
    if ((playlistTracks.count() > 0) && (url == playlistTracks.at(0)->getTrackInfo().url)) {
        currentTrack->setAboutToFinishSend(posBeforeEndMilliseconds);
    }
}


// track signal handler
void WaverServer::trackPosition(QUrl url, bool decoderFinished, long knownDurationMilliseconds, long positionMilliseconds)
{
    // show previous track's time still? (during crossfade)
    if (showPreviousTime && (previousTrack != NULL) && (url == previousTrack->getTrackInfo().url) && (previousPositionSeconds != (positionMilliseconds / 1000))) {
        previousPositionSeconds = positionMilliseconds / 1000;

        IpcMessageUtils ipcMessageUtils;
        emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::Position, QJsonDocument(QJsonObject::fromVariantHash(positionToElapsedRemaining(decoderFinished, knownDurationMilliseconds, positionMilliseconds)))));

        return;
    }

    // otherwise disregard this signal if comes from a track other than the current track
    if ((currentTrack == NULL) || (url != currentTrack->getTrackInfo().url)) {
        return;
    }

    // stop playing cast if reached time
    if (decoderFinished && (positionMilliseconds >= knownDurationMilliseconds)) {
        currentTrack->interrupt();
        return;
    }

    // start buffering next track before current ends
    if (decoderFinished && (positionMilliseconds >= (knownDurationMilliseconds - START_DECODE_PRE_MILLISECONDS))) {
        if ((playlistTracks.count() > 0) && (playlistTracks.at(0)->status() == Track::Idle)) {
            playlistTracks.at(0)->setStatus(Track::Decoding);
        }
    }

    // UI signal only once a second and if not showing previous still (there can be one signal while stopped, when decoder finishes)
    if (!showPreviousTime && ((positionSeconds != (positionMilliseconds / 1000)) || currentTrack->status() == Track::Paused)) {
        positionSeconds = (positionMilliseconds / 1000);

        // pass position to UI
        IpcMessageUtils ipcMessageUtils;
        emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::Position, QJsonDocument(QJsonObject::fromVariantHash(positionToElapsedRemaining(decoderFinished, knownDurationMilliseconds, positionMilliseconds)))));
    }
}


// track signal handler
void WaverServer::trackAboutToFinish(QUrl url)
{
    // disregard this signal if it comes from a track other than the current track
    if ((currentTrack == NULL) || (url != currentTrack->getTrackInfo().url)) {
        return;
    }

    // these signals should be connected with the current track only
    disconnect(this,         SIGNAL(requestPluginUi(QUuid)),                currentTrack, SLOT(requestedPluginUi(QUuid)));
    disconnect(this,         SIGNAL(pluginUiResults(QUuid, QJsonDocument)), currentTrack, SLOT(receivedPluginUiResults(QUuid, QJsonDocument)));
    disconnect(currentTrack, SIGNAL(pluginUi(QUuid, QString, QString)),     this,         SLOT(pluginUi(QUuid, QString, QString)));

    // move current track to previous track
    if (previousTrack != NULL) {
        emit done(previousTrack->getSourcePluginId(), previousTrack->getTrackInfo().url, false);
        delete previousTrack;
    }
    previousTrack           = currentTrack;
    previousPositionSeconds = positionSeconds;
    currentTrack            = NULL;

    // start next track
    startNextTrack();
}


// track signal handler
void WaverServer::trackFinished(QUrl url)
{
    // is this the previous track?
    if ((previousTrack != NULL) && (url == previousTrack->getTrackInfo().url)) {
        // send message to source if cast finished too early
        if ((previousTrack->getTrackInfo().cast) && (previousPositionSeconds < CAST_EARLY_SECONDS)) {
            emit castFinishedEarly(previousTrack->getSourcePluginId(), url, previousPositionSeconds);
        }

        // get replacement if needed
        bool wasError = ((previousPositionSeconds < 1) || (previousTrack->getTrackInfo().cast && (previousPositionSeconds < CAST_EARLY_SECONDS)));
        if (previousTrack->isReplacable() && (sourcePlugins.value(previousTrack->getSourcePluginId()).ready) && wasError) {
            emit getReplacement(previousTrack->getSourcePluginId());
        }

        // housekeeping
        emit done(previousTrack->getSourcePluginId(), previousTrack->getTrackInfo().url, wasError);
        delete previousTrack;
        previousTrack = NULL;
        previousPositionSeconds = 0;
        return;
    }

    // is this the current track?
    if ((currentTrack != NULL) && (url == currentTrack->getTrackInfo().url)) {
        // send message to source if could not even start
        if (positionSeconds < 1) {
            emit unableToStart(currentTrack->getSourcePluginId(), url);
            if (playlistTracks.count() > 0) {
                playlistTracks.at(0)->startWithoutFadeIn();
            }
        }

        // send message to source if cast finished too early
        if ((currentTrack->getTrackInfo().cast) && (positionSeconds < CAST_EARLY_SECONDS)) {
            emit castFinishedEarly(currentTrack->getSourcePluginId(), url, positionSeconds);
            if (playlistTracks.count() > 0) {
                playlistTracks.at(0)->startWithoutFadeIn();
            }
        }

        // get replacement if needed
        bool wasError = ((positionSeconds < 1) || (currentTrack->getTrackInfo().cast && (positionSeconds < CAST_EARLY_SECONDS)));
        if (currentTrack->isReplacable() && (sourcePlugins.value(currentTrack->getSourcePluginId()).ready) && wasError) {
            emit getReplacement(currentTrack->getSourcePluginId());
        }

        // housekeeping
        emit done(currentTrack->getSourcePluginId(), currentTrack->getTrackInfo().url, wasError);
        delete currentTrack;
        currentTrack = NULL;

        // start next track
        startNextTrack();

        return;
    }

    // let's see if this is a track in the playlist (for example if decoder gave up)
    QVector<Track *> tracksToBeDeleted;
    foreach (Track *track, playlistTracks) {
        if (track->getTrackInfo().url == url) {
            tracksToBeDeleted.append(track);
            if (track->isReplacable() && (sourcePlugins.value(track->getSourcePluginId()).ready)) {
                emit getReplacement(track->getSourcePluginId());
            }
        }
    }
    foreach (Track *track, tracksToBeDeleted) {
        emit unableToStart(track->getSourcePluginId(), track->getTrackInfo().url);
        emit done(track->getSourcePluginId(), track->getTrackInfo().url, true);
        playlistTracks.removeAll(track);
        delete track;
    }
    reassignFadeIns();

    // UI signal
    sendPlaylistToClients();

    // did the playlist become empty?
    if (playlistTracks.count() < 1) {
        requestPlaylist();
    }
}


// track signal handler
void WaverServer::trackInfoUpdated(QUrl url)
{
    // is this the previous track?
    if ((previousTrack != NULL) && (url == previousTrack->getTrackInfo().url)) {
        TrackInfo previousTrackInfo = previousTrack->getTrackInfo();

        // let the source know
        if (PluginLibsLoader::isPluginCompatible(sourcePlugins.value(previousTrack->getSourcePluginId()).waverVersionAPICompatibility, "0.0.6")) {
            emit trackAction(previousTrack->getSourcePluginId(), RESERVED_ACTION_TRACKINFOUPDATED, previousTrackInfo);
        }

        // see if picture must be shared with other tracks
        if (previousTrackInfo.pictures.count() > 0) {
            foreach (Track *track, playlistTracks) {
                TrackInfo trackInfo = track->getTrackInfo();
                if ((trackInfo.performer.compare(previousTrackInfo.performer, Qt::CaseInsensitive) == 0) && (trackInfo.album.compare(previousTrackInfo.album, Qt::CaseInsensitive) == 0)) {
                    track->addPictures(previousTrackInfo.pictures);
                }
            }
        }
        sendPlaylistToClients();
        return;
    }

    // is this the current track?
    if ((currentTrack != NULL) && (url == currentTrack->getTrackInfo().url)) {
        TrackInfo currentTrackInfo = currentTrack->getTrackInfo();

        // let the source know
        if (PluginLibsLoader::isPluginCompatible(sourcePlugins.value(currentTrack->getSourcePluginId()).waverVersionAPICompatibility, "0.0.6")) {
            emit trackAction(currentTrack->getSourcePluginId(), RESERVED_ACTION_TRACKINFOUPDATED, currentTrackInfo);
        }

        // update current track UI
        IpcMessageUtils ipcMessageUtils;
        emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::TrackInfos, ipcMessageUtils.trackInfoToJSONDocument(currentTrackInfo, currentTrack->getAdditionalInfo())));

        // see if picture must be shared with other tracks
        if (currentTrackInfo.pictures.count() > 0) {
            foreach (Track *track, playlistTracks) {
                TrackInfo trackInfo = track->getTrackInfo();
                if ((trackInfo.performer.compare(currentTrackInfo.performer, Qt::CaseInsensitive) == 0) && (trackInfo.album.compare(currentTrackInfo.album, Qt::CaseInsensitive) == 0)) {
                    track->addPictures(currentTrackInfo.pictures);
                }
            }
        }
        sendPlaylistToClients();

        return;
    }

    // must be somewhere in the playlist
    foreach (Track *track, playlistTracks) {
        // let the source know
        if ((url == track->getTrackInfo().url) && PluginLibsLoader::isPluginCompatible(sourcePlugins.value(track->getSourcePluginId()).waverVersionAPICompatibility, "0.0.6")) {
            emit trackAction(track->getSourcePluginId(), RESERVED_ACTION_TRACKINFOUPDATED, track->getTrackInfo());
        }
    }
    sendPlaylistToClients();
}


// track signal handler
void WaverServer::trackError(QUrl url, bool fatal, QString errorString)
{
    outputError(errorString, findTitleFromUrl(url), fatal);

    // cancel the track if fatal
    if (fatal) {
        trackFinished(url);
    }
}


// track signal handler
void WaverServer::trackLoadedPlugins(Track::PluginList plugins)
{
    // add source plugins
    foreach (QUuid id, sourcePlugins.keys()) {
        plugins.insert(id, Track::formatPluginName(sourcePlugins.value(id)));
    }

    // update UI if changed
    if (plugins != this->plugins) {
        this->plugins.clear();
        foreach (QUuid id, plugins.keys()) {
            this->plugins.insert(id, plugins.value(id));
        }
        sendPluginsToClients();
    }
}


// track signal handler
void WaverServer::trackLoadedPluginsWithUI(Track::PluginList pluginsWithUI)
{
    // add source plugins with UI
    foreach (QUuid id, sourcePlugins.keys()) {
        if (sourcePlugins.value(id).hasUI) {
            pluginsWithUI.insert(id, Track::formatPluginName(sourcePlugins.value(id)));
        }
    }

    // update UI if changed
    if (pluginsWithUI != this->pluginsWithUI) {
        this->pluginsWithUI.clear();
        foreach (QUuid id, pluginsWithUI.keys()) {
            this->pluginsWithUI.insert(id, pluginsWithUI.value(id));
        }
        sendPluginsWithUiToClients();
    }
}


// public method
TrackInfo WaverServer::notificationsHelper_Metadata()
{
    TrackInfo returnValue;

    if (currentTrack != NULL) {
        returnValue = currentTrack->getTrackInfo();
    }

    return returnValue;
}


// public method
void WaverServer::notificationsHelper_Next()
{
    ipcReceivedMessage(IpcMessageUtils::Next, QJsonDocument());
}


// public method
void WaverServer::notificationsHelper_OpenUri(QString uri)
{
    QUrl url = QUrl::fromUserInput(uri);

    if (url.isValid()) {
        ipcReceivedUrl(url);
    }
}


// public method
void WaverServer::notificationsHelper_Pause()
{
    ipcReceivedMessage(IpcMessageUtils::Pause, QJsonDocument());
}


// public method
void WaverServer::notificationsHelper_Play()
{
    ipcReceivedMessage(IpcMessageUtils::Resume, QJsonDocument());
}


// public method
Track::Status WaverServer::notificationsHelper_PlaybackStatus()
{
    if (currentTrack == NULL) {
        return Track::Idle;
    }

    return currentTrack->status();
}


// public method
void WaverServer::notificationsHelper_PlayPause()
{
    if (currentTrack != NULL) {
        if (currentTrack->status() == Track::Paused) {
            ipcReceivedMessage(IpcMessageUtils::Resume, QJsonDocument());
            return;
        }

        ipcReceivedMessage(IpcMessageUtils::Pause, QJsonDocument());
    }
}


// public method
long WaverServer::notificationsHelper_Position()
{
    return positionSeconds * 1000000;
}


// public method
void WaverServer::notificationsHelper_Quit()
{
    ipcReceivedMessage(IpcMessageUtils::Quit, QJsonDocument());
}


// public method
void WaverServer::notificationsHelper_Raise()
{
    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::QuitClients));

    QProcess::startDetached(QCoreApplication::applicationFilePath());
}


// public method
void WaverServer::notificationsHelper_Stop()
{
    ipcReceivedMessage(IpcMessageUtils::Quit, QJsonDocument());
}


// public method
double WaverServer::notificationsHelper_Volume()
{
    // TODO implemet this, until then just return 80%
    return 0.8;
}


// helper
QString WaverServer::formatLovedMode(int lovedMode)
{
    if (lovedMode == LOVE_FREQUENT) {
        return "Frequent";
    }
    if (lovedMode == LOVE_RARE) {
        return "Rare";
    }
    return "Normal";
}


// helper
int WaverServer::lovedModeFromString(QString str)
{
    if (str.compare("Frequent", Qt::CaseInsensitive) == 0) {
        return LOVE_FREQUENT;
    }
    if (str.compare("Rare", Qt::CaseInsensitive) == 0) {
        return LOVE_RARE;
    }
    return LOVE_NORMAL;
}
