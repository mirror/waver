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

    // save argument
    this->arguments.append(arguments);

    // initializations
    previousTrack                     = NULL;
    currentTrack                      = NULL;
    currentCollection                 = "";
    unableToStartCount                = 0;
    waitingForLocalSource             = true;
    waitingForLocalSourceTimerStarted = false;
    positionSeconds                   = 0;

    // so they can be used in inter-thread signals
    qRegisterMetaType<IpcMessageUtils::IpcMessages>("IpcMessageUtils::IpcMessages");
    qRegisterMetaType<PluginSource::TrackInfo>("PluginSource::TrackInfo");
    qRegisterMetaType<PluginSource::TracksInfo>("PluginSource::TracksInfo");
    qRegisterMetaType<PluginSource::OpenTracks>("PluginSource::OpenTracks");
    qRegisterMetaType<Track::PluginsWithUI>("Track::PluginsWithUI");
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

    serverTcpThread.start();

    // instantiate, set up, and start settings storage

    SettingsHandler *settingsHandler = new SettingsHandler();
    settingsHandler->moveToThread(&settingsThread);

    connect(&settingsThread, SIGNAL(started()),  settingsHandler, SLOT(run()));
    connect(&settingsThread, SIGNAL(finished()), settingsHandler, SLOT(deleteLater()));

    connect(this,            SIGNAL(saveCollectionList(QStringList,QString)),         settingsHandler, SLOT(saveCollectionList(QStringList,QString)));
    connect(this,            SIGNAL(saveCollectionList(QString)),                     settingsHandler, SLOT(saveCollectionList(QString)));
    connect(this,            SIGNAL(getCollectionList()),                             settingsHandler, SLOT(getCollectionList()));
    connect(this,            SIGNAL(savePluginSettings(QUuid,QString,QJsonDocument)), settingsHandler, SLOT(savePluginSettings(QUuid,QString,QJsonDocument)));
    connect(this,            SIGNAL(loadPluginSettings(QUuid,QString)),               settingsHandler, SLOT(loadPluginSettings(QUuid,QString)));
    connect(settingsHandler, SIGNAL(collectionList(QStringList,QString)),             this,            SLOT(collectionList(QStringList,QString)));
    connect(settingsHandler, SIGNAL(loadedPluginSettings(QUuid,QJsonDocument)),       this,            SLOT(loadedPluginSettings(QUuid,QJsonDocument)));

    settingsThread.start();
}


// shutdown this server - private method
void WaverServer::finish()
{
    // delete tracks
    if (previousTrack != NULL) {
        delete previousTrack;
    }
    if (currentTrack != NULL) {
        delete currentTrack;
    }
    foreach (Track *track, playlistTracks) {
        delete track;
    }

    // stop sources
    sourcesThread.requestInterruption();
    sourcesThread.quit();
    sourcesThread.wait();

    // stop interprocess communication handler
    serverTcpThread.quit();
    serverTcpThread.wait(THREAD_TIMEOUT);

    // stop settings storage
    settingsThread.quit();
    settingsThread.wait(THREAD_TIMEOUT);

    // let the world know
    emit finished();
}


// private method
void WaverServer::requestPlaylist()
{
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

    // not requesting anymore tracks if couldn't start too many tracks already
    if (unableToStartCount >= (readyPlugins.count() * MAX_TRACKS_AT_ONCE)) {
        return;
    }

    // don't use the same as before if there are more than one
    if (readyPlugins.count() > 1) {
        readyPlugins.removeAll(lastPlaylistPlugin);
    }

    // random which plugin to use
    int pluginIndex = qrand() % readyPlugins.count();

    // give priority to local files at startup
    if (waitingForLocalSource) {
        pluginIndex = readyPlugins.indexOf(QUuid("{187C9046-4801-4DB2-976C-128761F25BD8}"));
        if (pluginIndex < 0) {
            return;
        }
    }

    // emit signal
    emit getPlaylist(readyPlugins.at(pluginIndex), MAX_TRACKS_AT_ONCE);
    lastPlaylistPlugin = readyPlugins.at(pluginIndex);
}


// private method
void WaverServer::startNextTrack()
{
    // so that this method can be called blindly
    if (currentTrack != NULL) {
        return;
    }

    // add new tracks to playlist if needed
    if (playlistTracks.count() < 2) {
        requestPlaylist();
    }

    // make sure there's at least one track waiting (requestPlaylist above runs in paralell; when playlist is received, this method will be called again)
    if (playlistTracks.count() < 1) {
        return;
    }

    // start next track
    currentTrack = playlistTracks.at(0);
    playlistTracks.remove(0);
    positionSeconds = -1;
    currentTrack->setStatus(Track::Playing);

    // these signals should be connected with the current track only
    connect(this,         SIGNAL(requestPluginUi(QUuid)),  currentTrack, SLOT(requestedPluginUi(QUuid)));
    connect(currentTrack, SIGNAL(pluginUi(QUuid,QString)), this,         SLOT(pluginUi(QUuid,QString)));

    // info output
    Globals::consoleOutput(QString("%1 - %2").arg(currentTrack->getTrackInfo().title).arg(currentTrack->getTrackInfo().performer), false);

    // UI signals

    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::TrackInfo, ipcMessageUtils.trackInfoToJSONDocument(currentTrack->getTrackInfo())));

    sendPlaylistToClients();
}


// private method
void WaverServer::handleCollectionsDialogResults(QJsonDocument jsonDocument)
{
    QStringList collections;
    foreach(QVariant collection, jsonDocument.array()) {
        collections.append(collection.toString());
    }

    emit saveCollectionList(collections, currentCollection);
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

    emit pluginUiResults(QUuid(object.value("plugin_id").toString()), (
        (QString(object.value("ui_results").typeName()).compare("QVariantList") == 0) ? QJsonDocument(QJsonArray::fromVariantList(object.value("ui_results").toList())) :
        (QString(object.value("ui_results").typeName()).compare("QVariantHash") == 0) ? QJsonDocument(QJsonObject::fromVariantHash(object.value("ui_results").toHash())) :
        (QString(object.value("ui_results").typeName()).compare("QVariantMap")  == 0) ? QJsonDocument(QJsonObject::fromVariantMap(object.value("ui_results").toMap())) :
                                                                                        QJsonDocument(object.value("ui_results").toJsonObject())));
}


// private method
void WaverServer::handleOpenTracksRequest(QJsonDocument jsonDocument)
{
    QVariantHash object = jsonDocument.object().toVariantHash();

    // if no plugin id is specified, then plugin list must be sent
    if (object.value("plugin_id").toString().length() < 1) {
        foreach (QUuid uuid, sourcePlugins.keys()) {
            PluginSource::OpenTrack openTrack;
            openTrack.hasChildren = true;
            openTrack.selectable  = false;
            openTrack.label       = Track::formatPluginName(sourcePlugins.value(uuid));
            openTrack.id          = "";

            PluginSource::OpenTracks openTracks;
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
    QHash<QUuid, QStringList*> requestedTracks;
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

    foreach(QUuid uuid, sourcePlugins.keys()) {
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
void WaverServer::sendPlaylistToClients()
{
    IpcMessageUtils ipcMessageUtils;
    QJsonArray      playlist;

    foreach (Track *track, playlistTracks) {
        QJsonDocument info = ipcMessageUtils.trackInfoToJSONDocument(track->getTrackInfo());
        playlist.append(info.object());
    }

    // TODO make this able to send to specific client only (when client requests it at startup)
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::Playlist, QJsonDocument(playlist)));
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
void WaverServer::sendOpenTracksToClients(IpcMessageUtils::IpcMessages message, QUuid uniqueId, PluginSource::OpenTracks openTracks)
{
    QJsonArray tracksJson;
    foreach (PluginSource::OpenTrack track, openTracks) {
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


// plugin libraries loader signal handler
void WaverServer::pluginLibsLoaded()
{
    // load source plugins
    foreach (PluginLibsLoader::LoadedLib loadedLib, loadedLibs) {

        // call library's plugin factory (only want source plugins here)
        PluginFactoryResults plugins;
        loadedLib.pluginFactory(PluginBase::PLUGIN_TYPE_SOURCE, &plugins);

        // process each plugin one by one
        foreach (PluginBase *plugin, plugins) {

            // just to be on the safe side
            if (sourcePlugins.contains(plugin->persistentUniqueId())) {
                continue;
            }

            // cast the plugin, asked for source plugins only
            PluginSource *pluginSource = (PluginSource*) plugin;

            // remember some info about the plugin
            SourcePlugin pluginData;
            pluginData.name          = pluginSource->pluginName();
            pluginData.version       = pluginSource->pluginVersion();
            pluginData.baseVersion   = pluginSource->PLUGIN_BASE_VERSION;
            pluginData.pluginTypeVersion = pluginSource->PLUGIN_SOURCE_VERSION;
            pluginData.hasUI         = pluginSource->hasUI();
            pluginData.ready         = false;
            sourcePlugins[pluginSource->persistentUniqueId()] = pluginData;

            // move to sources thread
            pluginSource->moveToThread(&sourcesThread);

            // connect thread signals
            connect(&sourcesThread, SIGNAL(started()),  pluginSource, SLOT(run()));
            connect(&sourcesThread, SIGNAL(finished()), pluginSource, SLOT(deleteLater()));

            // connect source plugin signals
            connect(pluginSource, SIGNAL(ready(QUuid)),                                      this,         SLOT(pluginReady(QUuid)));
            connect(pluginSource, SIGNAL(unready(QUuid)),                                    this,         SLOT(pluginUnready(QUuid)));
            connect(pluginSource, SIGNAL(saveConfiguration(QUuid,QJsonDocument)),            this,         SLOT(saveConfiguration(QUuid,QJsonDocument)));
            connect(pluginSource, SIGNAL(loadConfiguration(QUuid)),                          this,         SLOT(loadConfiguration(QUuid)));
            connect(pluginSource, SIGNAL(uiQml(QUuid,QString)),                              this,         SLOT(pluginUi(QUuid,QString)));
            connect(pluginSource, SIGNAL(infoMessage(QUuid,QString)),                        this,         SLOT(pluginInfoMessage(QUuid,QString)));
            connect(pluginSource, SIGNAL(playlist(QUuid,PluginSource::TracksInfo)),          this,         SLOT(playlist(QUuid,PluginSource::TracksInfo)));
            connect(pluginSource, SIGNAL(requestRemoveTracks(QUuid)),                        this,         SLOT(requestedRemoveTracks(QUuid)));
            connect(pluginSource, SIGNAL(openTracksResults(QUuid,PluginSource::OpenTracks)), this,         SLOT(openTracksResults(QUuid,PluginSource::OpenTracks)));
            connect(pluginSource, SIGNAL(searchResults(QUuid,PluginSource::OpenTracks)),     this,         SLOT(searchResults(QUuid,PluginSource::OpenTracks)));
            connect(this,         SIGNAL(unableToStart(QUuid,QUrl)),                         pluginSource, SLOT(unableToStart(QUuid,QUrl)));
            connect(this,         SIGNAL(loadedConfiguration(QUuid,QJsonDocument)),          pluginSource, SLOT(loadedConfiguration(QUuid,QJsonDocument)));
            connect(this,         SIGNAL(requestPluginUi(QUuid)),                            pluginSource, SLOT(getUiQml(QUuid)));
            connect(this,         SIGNAL(pluginUiResults(QUuid,QJsonDocument)),              pluginSource, SLOT(uiResults(QUuid,QJsonDocument)));
            connect(this,         SIGNAL(getPlaylist(QUuid,int)),                            pluginSource, SLOT(getPlaylist(QUuid,int)));
            connect(this,         SIGNAL(getOpenTracks(QUuid,QString)),                      pluginSource, SLOT(getOpenTracks(QUuid,QString)));
            connect(this,         SIGNAL(search(QUuid,QString)),                             pluginSource, SLOT(search(QUuid,QString)));
            connect(this,         SIGNAL(resolveOpenTracks(QUuid,QStringList)),              pluginSource, SLOT(resolveOpenTracks(QUuid,QStringList)));
        }
    }

    // start source thread and hance source plugins
    sourcesThread.start();
}


// plugin libraries loader signal handler
void WaverServer::pluginLibsFailInfo(QString info)
{
    Globals::consoleOutput(info, true);
}


// interprocess communication signal handler
void WaverServer::ipcReceivedMessage(IpcMessageUtils::IpcMessages message, QJsonDocument jsonDocument)
{
    IpcMessageUtils ipcMessageUtils;

    // do what needs to be done
    switch (message) {

    case IpcMessageUtils::CollectionList:
        emit getCollectionList();
        break;

    case IpcMessageUtils::CollectionMenuChange:
        // settings handler will reply with collectionList
        emit saveCollectionList(jsonDocument.object().toVariantHash().value("collection").toString());
        break;

    case IpcMessageUtils::CollectionsDialogResults:
        handleCollectionsDialogResults(jsonDocument);
        break;

    case IpcMessageUtils::Next:
        if (currentTrack != NULL) {
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
        break;

    case IpcMessageUtils::Playlist:
        sendPlaylistToClients();
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

    case IpcMessageUtils::TrackInfo:
        if (currentTrack != NULL) {
            emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::TrackInfo, ipcMessageUtils.trackInfoToJSONDocument(currentTrack->getTrackInfo())));
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


// settings storage signal handler
void WaverServer::collectionList(QStringList collections, QString currentCollection)
{
    // settings storage sends this signal right after it just started at server startup
    if (this->currentCollection.isEmpty()) {
        this->currentCollection = currentCollection;

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

    // is collection changed?
    if (this->currentCollection.compare(currentCollection) != 0) {
        this->currentCollection = currentCollection;

        // load plugin settings for new collection
        foreach (QUuid pluginId, sourcePlugins.keys()) {
            emit loadPluginSettings(pluginId, this->currentCollection);
        }

        // empty playlist
        QVector<Track*> tracksToBeDeleted;
        tracksToBeDeleted.append(playlistTracks);
        foreach(Track *track, tracksToBeDeleted) {
            playlistTracks.removeAll(track);
            delete track;
        }

        // stop current track
        currentTrack->interrupt();

        // UI signal
        sendPlaylistToClients();

        // next track from newly selected collection
        startNextTrack();
    }

    // send to UI

    qSort(collections);

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


// settings storage signal handler
void WaverServer::loadedPluginSettings(QUuid uniqueId, QJsonDocument settings)
{
    // nothing much to do, just re-emit for sources and tracks
    emit loadedConfiguration(uniqueId, settings);
}


// source plugin and track signal handler
void WaverServer::pluginUi(QUuid uniqueId, QString qml)
{
    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::PluginUI, QJsonDocument(QJsonObject::fromVariantHash(QVariantHash({
        {
            "plugin_id", uniqueId.toString()
        },
        {
            "ui_qml", qml
        }
    })))));
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
    if (playlistTracks.count() <= 1) {
        requestPlaylist();
    }
}


// timer signal handler
void WaverServer::notWaitingForLocalSourceAnymore()
{
    waitingForLocalSource = false;

    if (playlistTracks.count() <= 1) {
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
    Q_UNUSED(uniqueId);

    QVariantHash messageHash;
    messageHash.insert("message", message);
    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::InfoMessage, QJsonDocument(QJsonObject::fromVariantHash(messageHash))));
}


// plugin signal handler (this can come from source or track)
void WaverServer::loadConfiguration(QUuid uniqueId)
{
    // convert parameters and re-emit for setting storage handler
    emit loadPluginSettings(uniqueId, currentCollection);
}


// plugin signal handler (this can come from source or track)
void WaverServer::saveConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    // parameter check
    if (configuration.isEmpty()) {
        return;
    }

    // convert parameters and re-emit and re-emit for setting storage handler
    emit savePluginSettings(uniqueId, currentCollection, configuration);
}


// source plugin signal handler
void WaverServer::playlist(QUuid uniqueId, PluginSource::TracksInfo tracksInfo)
{
    Globals::consoleOutput(QString("Received %1 tracks from %2").arg(tracksInfo.count()).arg(sourcePlugins.value(uniqueId).name), false);

    foreach(PluginSource::TrackInfo trackInfo, tracksInfo) {
        // create and set up track

        Track *track = new Track(&loadedLibs, trackInfo, uniqueId, this);

        connect(this,  SIGNAL(loadedConfiguration(QUuid,QJsonDocument)),          track, SLOT(loadedPluginSettings(QUuid,QJsonDocument)));
        connect(this,  SIGNAL(pluginUiResults(QUuid,QJsonDocument)),              track, SLOT(receivedPluginUiResults(QUuid,QJsonDocument)));
        connect(track, SIGNAL(savePluginSettings(QUuid,QJsonDocument)),           this,  SLOT(saveConfiguration(QUuid,QJsonDocument)));
        connect(track, SIGNAL(loadPluginSettings(QUuid)),                         this,  SLOT(loadConfiguration(QUuid)));
        connect(track, SIGNAL(requestFadeInForNextTrack(QUrl,qint64)),            this,  SLOT(trackRequestFadeInForNextTrack(QUrl,qint64)));
        connect(track, SIGNAL(playPosition(QUrl,bool,bool,long,long)),            this,  SLOT(trackPosition(QUrl,bool,bool,long,long)));
        connect(track, SIGNAL(aboutToFinish(QUrl)),                               this,  SLOT(trackAboutToFinish(QUrl)));
        connect(track, SIGNAL(finished(QUrl)),                                    this,  SLOT(trackFinished(QUrl)));
        connect(track, SIGNAL(trackInfoUpdated(QUrl)),                            this,  SLOT(trackInfoUpdated(QUrl)));
        connect(track, SIGNAL(error(QUrl,bool,QString)),                          this,  SLOT(trackError(QUrl,bool,QString)));
        connect(track, SIGNAL(loadedPluginsWithUI(Track::PluginsWithUI)),         this,  SLOT(trackLoadedPluginsWithUI(Track::PluginsWithUI)));

        // add to playlist
        playlistTracks.append(track);
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
}


// source plugin signal handler
void WaverServer::openTracksResults(QUuid uniqueId, PluginSource::OpenTracks openTracks)
{
    sendOpenTracksToClients(IpcMessageUtils::OpenTracks, uniqueId, openTracks);
}


// source plugin signal handler
void WaverServer::searchResults(QUuid uniqueId, PluginSource::OpenTracks openTracks)
{
    sendOpenTracksToClients(IpcMessageUtils::Search, uniqueId, openTracks);
}


// source plugin signal handler
void WaverServer::requestedRemoveTracks(QUuid uniqueId)
{
    // remove from playlist
    QVector<Track*> tracksToBeDeleted;
    foreach(Track *track, playlistTracks) {
        if (track->getSourcePluginId() == uniqueId) {
            tracksToBeDeleted.append(track);
        }
    }
    foreach(Track *track, tracksToBeDeleted) {
        playlistTracks.removeAll(track);
        delete track;
    }
    sendPlaylistToClients();

    // check current track
    if ((currentTrack != NULL) && (currentTrack->getSourcePluginId() == uniqueId)) {
        currentTrack->interrupt();
    }
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
void WaverServer::trackPosition(QUrl url, bool cast, bool decoderFinished, long knownDurationMilliseconds, long positionMilliseconds)
{
    // disregard this signal if comes from a track other than the current track
    if ((currentTrack == NULL) || (url != currentTrack->getTrackInfo().url)) {
        return;
    }

    // stop playing cast if reached time
    if (cast && (positionMilliseconds >= CAST_PLAYTIME_MILLISECONDS)) {
        currentTrack->interrupt();
        return;
    }

    // start buffering next track before current ends
    if ((cast && (positionMilliseconds >= (CAST_PLAYTIME_MILLISECONDS - START_DECODE_PRE_MILLISECONDS))) || (decoderFinished && (positionMilliseconds >= (knownDurationMilliseconds - START_DECODE_PRE_MILLISECONDS)))) {
        if ((playlistTracks.count() > 0) && (playlistTracks.at(0)->status() == Track::Idle)) {
            playlistTracks.at(0)->setStatus(Track::Decoding);
        }
    }

    // UI signal only once a second
    if (positionSeconds != (positionMilliseconds / 1000)) {
        positionSeconds = (positionMilliseconds / 1000);

        unableToStartCount = 0;

        QVariantHash positionHash;

        positionHash.insert("elapsed", QDateTime::fromMSecsSinceEpoch(positionMilliseconds).toUTC().toString("mm:ss"));
        positionHash.insert("remaining", cast ? QDateTime::fromMSecsSinceEpoch(CAST_PLAYTIME_MILLISECONDS - positionMilliseconds).toUTC().toString("mm:ss") : (decoderFinished ? QDateTime::fromMSecsSinceEpoch(knownDurationMilliseconds - positionMilliseconds).toUTC().toString("mm:ss") : ""));

        IpcMessageUtils ipcMessageUtils;
        emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::Position, QJsonDocument(QJsonObject::fromVariantHash(positionHash))));
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
    disconnect(this,         SIGNAL(requestPluginUi(QUuid)),  currentTrack, SLOT(requestedPluginUi(QUuid)));
    disconnect(currentTrack, SIGNAL(pluginUi(QUuid,QString)), this,         SLOT(pluginUi(QUuid,QString)));

    // move current track to previous track
    if (previousTrack != NULL) {
        delete previousTrack;
    }
    previousTrack = currentTrack;
    currentTrack = NULL;

    // start next track
    startNextTrack();
}


// track signal handler
void WaverServer::trackFinished(QUrl url)
{
    // is this the previous track?
    if ((previousTrack != NULL) && (url == previousTrack->getTrackInfo().url)) {
        // housekeeping
        delete previousTrack;
        previousTrack = NULL;
        return;
    }

    // is this the current track?
    if ((currentTrack != NULL) && (url == currentTrack->getTrackInfo().url)) {
        // send message to source if could not even start
        if (positionSeconds < 1) {
            unableToStartCount++;
            emit unableToStart(currentTrack->getSourcePluginId(), url);
        }

        // housekeeping
        delete currentTrack;
        currentTrack = NULL;

        // start next track
        startNextTrack();

        return;
    }

    // let's see if this is a track in the playlist (for example if decoder gave up)
    QVector<Track*> tracksToBeDeleted;
    foreach(Track *track, playlistTracks) {
        if (track->getTrackInfo().url == url) {
            tracksToBeDeleted.append(track);
        }
    }
    foreach(Track *track, tracksToBeDeleted) {
        emit unableToStart(track->getSourcePluginId(), track->getTrackInfo().url);
        playlistTracks.removeAll(track);
        delete track;
    }

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
        // noting to do
        return;
    }

    // is this the current track?
    if ((currentTrack != NULL) && (url == currentTrack->getTrackInfo().url)) {
        IpcMessageUtils ipcMessageUtils;
        emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::TrackInfo, ipcMessageUtils.trackInfoToJSONDocument(currentTrack->getTrackInfo())));
        return;
    }

    // must be somewhere in the playlist
    sendPlaylistToClients();
}


// track signal handler
void WaverServer::trackError(QUrl url, bool fatal, QString errorString)
{
    // print to error output
    Globals::consoleOutput(QString("%1 reported from track '%2': %3").arg(fatal ? "Fatal error" : "Error").arg(url.toString()).arg(errorString), true);

    // send message to UI
    QVariantHash messageHash;
    messageHash.insert("message", errorString);
    IpcMessageUtils ipcMessageUtils;
    emit ipcSend(ipcMessageUtils.constructIpcString(IpcMessageUtils::InfoMessage, QJsonDocument(QJsonObject::fromVariantHash(messageHash))));

    // cancel the track if fatal
    if (fatal) {
        trackFinished(url);
    }
}


// track signal handler
void WaverServer::trackLoadedPluginsWithUI(Track::PluginsWithUI pluginsWithUI)
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
PluginSource::TrackInfo WaverServer::notificationsHelper_Metadata()
{
    PluginSource::TrackInfo returnValue;

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
