/*
    This file is part of Waver

    Copyright (C) 2018 Peter Papp <peter.papp.p@gmail.com>

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


#include "sftpsource.h"


// plugin factory
void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal)
{
    if (pluginTypesMask & PLUGIN_TYPE_SOURCE) {
        retVal->append((QObject *) new SFTPSource());
    }
}


// constructor
SFTPSource::SFTPSource()
{
    id  = QUuid("{7F95124F-8AE5-4CDA-AAE9-13136DCD4897}");

    variationSetting           = "Medium";
    variationSetCountSinceHigh = 0;
    variationSetCountSinceLow  = 0;
    readySent                  = false;
    sendDiagnostics            = false;
    unableToStartCount         = 0;

    if (libssh2_init(0)) {
        emit infoMessage(id, "Unable to initialize libssh2");
        state = SSHLib2Fail;
        return;
    }

    state = Idle;

    QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataLocation.isEmpty()) {
        dataLocation = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    if (dataLocation.isEmpty()) {
        emit infoMessage(id, "Unable to initialize data directory");
        state = DataDirFail;
        return;
    }

    QDir dataDir(QDir(dataLocation).absoluteFilePath("Waver_SFTP"));
    if (!dataDir.exists()) {
        dataDir.mkpath(dataDir.absolutePath());
    }

    keysDir = QDir(dataDir.absoluteFilePath("ssh_keys"));
    if (!keysDir.exists()) {
        keysDir.mkpath(keysDir.absolutePath());
    }

    cacheDir = QDir(dataDir.absoluteFilePath("cache"));
    if (!cacheDir.exists()) {
        cacheDir.mkpath(cacheDir.absolutePath());
    }
}


// destructor
SFTPSource::~SFTPSource()
{
    // TODO this must be faster
    if (state != SSHLib2Fail) {
        removeAllClients();
        libssh2_exit();
    }
}


// overridden virtual function
int SFTPSource::pluginType()
{
    return PLUGIN_TYPE_SOURCE;
}


// overridden virtual function
QString SFTPSource::pluginName()
{
    return "SFTP";
}


// overridden virtual function
int SFTPSource::pluginVersion()
{
    return 1;
}


// overrided virtual function
QString SFTPSource::waverVersionAPICompatibility()
{
    return "0.0.6";
}


// overridden virtual function
bool SFTPSource::hasUI()
{
    return true;
}


// overridden virtual function
void SFTPSource::setUserAgent(QString userAgent)
{
    Q_UNUSED(userAgent);
}

// overridden virtual function
QUuid SFTPSource::persistentUniqueId()
{
    return id;
}


// thread entry
void SFTPSource::run()
{
    qsrand(QDateTime::currentDateTime().toTime_t());
    srand(QDateTime::currentDateTime().toTime_t());

    emit loadGlobalConfiguration(id);
}


// slot receiving configuration
void SFTPSource::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfig(configuration);

    // keep cache from growing endlessly
    reduceCache();

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    // start all clients
    foreach (SSHClient *client, clients) {
        client->getConfig().thread->start();
    }
}


// slot receiving configuration
void SFTPSource::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfigGlobal(configuration);

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    emit loadConfiguration(id);
}


// configuration
void SFTPSource::sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void SFTPSource::globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}

// configuration
void SFTPSource::sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
{
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);

    if (persistentUniqueId != id) {
        return;
    }

    emit infoMessage(id, error);
}


// message from another plugin
void SFTPSource::messageFromPlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(sourceUniqueId);
    Q_UNUSED(messageId);
    Q_UNUSED(value);
}


// server asks for this plugin's configuration dialog
void SFTPSource::getUiQml(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    QFile settingsFile("://SFTPSettings.qml");
    settingsFile.open(QFile::ReadOnly);
    QString settings = settingsFile.readAll();
    settingsFile.close();

    QString clientElements;
    foreach (SSHClient *client, clients) {
        clientElements.append(QString("ListElement { formatted_user_host: \"%1\"; dir: \"%2\"; is_connected: %3; client_id: %4; } ").arg(client->formatUserHost()).arg(client->getConfig().dir).arg(client->isConnected() ? 1 : 0).arg(client->getConfig().id));
    }
    settings.replace("ListElement{}", clientElements);

    settings.replace("9999", QString("%1").arg(variationSettingId()));

    // there is a queue to display dialogs because of the login interactions
    addToUIQueue(settings);
}


// dialog results from server
void SFTPSource::uiResults(QUuid uniqueId, QJsonDocument results)
{
    if (uniqueId != id) {
        return;
    }

    // dialog done
    removeFromUIQueue();

    QVariantHash resultsHash = results.object().toVariantHash();

    // change variation
    if (resultsHash.value("button").toString().compare("variation") == 0) {
        variationSetting = resultsHash.value("variation").toString();
        emit saveConfiguration(id, configToJson());
    }

    // add client
    if (resultsHash.value("button").toString().compare("add") == 0) {
        SSHClient::SSHClientConfig config;
        config.id   = 0;
        config.host = resultsHash.value("host").toString();
        config.user = resultsHash.value("user").toString();
        addClient(config);
        clients.last()->thread()->start();
    }

    // manually connect client (in case it got disconnected)
    if (resultsHash.value("button").toString().compare("connect") == 0) {
        SSHClient *client = clientFromId(resultsHash.value("client_id").toInt());
        if (!client->isConnected()) {
            emit clientConnect(client->getConfig().id);
        }
    }

    // manually disconnect client
    if (resultsHash.value("button").toString().compare("disconnect") == 0) {
        SSHClient *client = clientFromId(resultsHash.value("client_id").toInt());
        if (client->isConnected()) {
            emit clientDisconnect(client->getConfig().id);
        }
    }

    // delete client for good
    if (resultsHash.value("button").toString().compare("remove") == 0) {
        SSHClient *client = clientFromId(resultsHash.value("client_id").toInt());
        client->getConfig().thread->requestInterruption();
        client->getConfig().thread->quit();
        client->getConfig().thread->wait();
        clients.removeOne(client);
        emit saveConfiguration(id, configToJson());
    }

    // password entry
    if (resultsHash.value("button").toString().compare("psw") == 0) {
        emit clientPasswordEntryResult(resultsHash.value("client").toInt(), resultsHash.value("fingerprint").toString(), resultsHash.value("psw").toString());
    }
    if (resultsHash.value("button").toString().compare("psw_cancel") == 0) {
        emit clientDisconnect(resultsHash.value("client").toInt());
    }

    // public key setup
    if (resultsHash.value("button").toString().compare("key_setup_yes") == 0) {
        emit clientKeySetupQuestionResult(resultsHash.value("client").toInt(), true);
    }
    if (resultsHash.value("button").toString().compare("key_setup_no") == 0) {
        emit clientKeySetupQuestionResult(resultsHash.value("client").toInt(), false);
    }

    // directory selection
    if (resultsHash.value("button").toString().compare("dir_selector_done") == 0) {
        emit clientDirSelectorResult(resultsHash.value("client").toInt(), false, resultsHash.value("full_path").toString());
    }
    if (resultsHash.value("button").toString().compare("dir_selector_open") == 0) {
        emit clientDirSelectorResult(resultsHash.value("client").toInt(), true, resultsHash.value("full_path").toString());
    }

    // show next dialog from queue
    displayNextUIQueue();
}


// request from server to receive updates of this plugin's diagnostic information
void SFTPSource::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = true;
    sendDiagnosticsData();
}


// request from server not to receive updates of this plugin's diagnostic information anymore
void SFTPSource::stopDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = false;
}


// broken download?
void SFTPSource::unableToStart(QUuid uniqueId, QUrl url)
{
    Q_UNUSED(url);

    if (uniqueId != id) {
        return;
    }

    unableToStartCount++;
    if (readySent && (unableToStartCount >= 4)) {
        readySent = false;
        emit unready(id);

        QTimer::singleShot(10 * 60 * 1000, this, SLOT(readyIfReady()));
    }
}


// this should never happen because of no casts
void SFTPSource::castFinishedEarly(QUuid uniqueId, QUrl url, int playedSeconds)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(url);
    Q_UNUSED(playedSeconds);
}


// server says this track is no longer needed (for now)
void SFTPSource::done(QUuid uniqueId, QUrl url, bool wasError)
{
    if (uniqueId != id) {
        return;
    }

    // check if it's loved or similar
    bool lovedOrSimilar = false;
    int i               = 0;
    while ((i < lovedPlaylist.count()) && !lovedOrSimilar) {
        if (lovedPlaylist.at(i).cachePath == url) {
            lovedOrSimilar = true;
        }
        i++;
    }
    i = 0;
    while ((i < similarPlaylist.count()) && !lovedOrSimilar) {
        if (similarPlaylist.at(i).cachePath == url) {
            lovedOrSimilar = true;
        }
        i++;
    }
    if (lovedOrSimilar) {
        return;
    }

    // check if it was opened manually too
    if (doNotDelete.contains(url)) {
        // OK to remove after played again
        doNotDelete.removeOne(url);
        return;
    }

    // remove from cache
    QFile file(url.toLocalFile());
    file.remove();

    // reset "unable to start" counter
    if (!wasError) {
        unableToStartCount = 0;
    }
}

// server's request for playlist entries
void SFTPSource::getPlaylist(QUuid uniqueId, int trackCount, int mode)
{
    if (uniqueId != id) {
        return;
    }

    TracksInfo returnValue;
    ExtraInfo  returnExtra;

    // loved
    if (mode == PLAYLIST_MODE_LOVED) {
        // see which loved was not played yet
        QVector<PlaylistItem> remaining;
        foreach (PlaylistItem playlistItem, lovedPlaylist) {
            if (!alreadyPlayedLoved.contains(formatTrackForLists(playlistItem.clientId, playlistItem.remotePath))) {
                remaining.append(playlistItem);
            }
        }
        // "turn around" if all was played already
        if (remaining.count() < 1) {
            alreadyPlayedLoved.clear();
            remaining.append(lovedPlaylist);
        }

        if (remaining.count() > 0) {
            // let's see which is downloaded already
            QVector<PlaylistItem> downloaded;
            foreach (PlaylistItem playlistItem, remaining) {
                if (playlistItem.cachePath.isValid()) {
                    downloaded.append(playlistItem);
                }
            }
            if (downloaded.count() > 0) {
                // select a loved track
                int index = qrand() % downloaded.count();

                // update global configuration
                alreadyPlayedLoved.append(formatTrackForLists(downloaded.at(index).clientId, downloaded.at(index).remotePath));
                emit saveGlobalConfiguration(id, configToJsonGlobal());

                // add to return value
                bool tagLibOK = true;
                TrackInfo trackInfo = trackInfoFromFilePath(downloaded.at(index).cachePath.toLocalFile(), downloaded.at(index).clientId, &tagLibOK);
                returnValue.append(trackInfo);
                addToExtraInfo(&returnExtra, trackInfo.url, "loved", PLAYLIST_MODE_LOVED);
                if (!tagLibOK) {
                    addToExtraInfo(&returnExtra, trackInfo.url, "incomplete_tags", 1);
                }

                // playlist should be one less now
                trackCount--;
            }
        }
    }
    else if (mode == PLAYLIST_MODE_LOVED_SIMILAR) {
        // let's see which is downloaded already
        QVector<PlaylistItem> downloaded;
        foreach (PlaylistItem playlistItem, similarPlaylist) {
            if (playlistItem.cachePath.isValid()) {
                downloaded.append(playlistItem);
            }
        }

        if (downloaded.count() > 0) {
            // select a loved track
            int index = qrand() % downloaded.count();

            // add to return value
            bool tagLibOK = true;
            TrackInfo trackInfo = trackInfoFromFilePath(downloaded.at(index).cachePath.toLocalFile(), downloaded.at(index).clientId, &tagLibOK);
            returnValue.append(trackInfo);
            addToExtraInfo(&returnExtra, trackInfo.url, "loved", PLAYLIST_MODE_LOVED_SIMILAR);
            if (!tagLibOK) {
                addToExtraInfo(&returnExtra, trackInfo.url, "incomplete_tags", 1);
            }

            // playlist should be one less now
            trackCount--;
        }
    }

    // just send from predetermined playlist
    if (trackCount > 0) {
        int count = 0;
        while (count < trackCount) {
            // find first downloaded (might be holes if there's more than one client)
            int index = -1;
            int i     = 0;
            while ((i < futurePlaylist.count()) && (index < 0)) {
                if (futurePlaylist.at(i).cachePath.isValid()) {
                    index = i;
                }
                i++;
            }
            if (index < 0) {
                // no more downloaded tracks at this time, can't fullfill entirely, but still send what's already found
                break;
            }

            // add to return value
            bool tagLibOK = true;
            TrackInfo trackInfo = trackInfoFromFilePath(futurePlaylist.at(index).cachePath.toLocalFile(), futurePlaylist.at(index).clientId, &tagLibOK);
            returnValue.append(trackInfo);
            if (!tagLibOK) {
                addToExtraInfo(&returnExtra, trackInfo.url, "incomplete_tags", 1);
            }

            // add to already played
            alreadyPlayed.append(formatTrackForLists(futurePlaylist.at(index).clientId, futurePlaylist.at(index).remotePath));

            // remove from predetermined playlist
            futurePlaylist.remove(index);

            count++;
        }
    }

    // send them back to the server
    emit playlist(id, returnValue, returnExtra);

    // must save updated configuration (already played)
    emit saveConfiguration(id, configToJson());

    // diagnostics display
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    // refill predetermined playlist - essentially: downloads more
    appendToPlaylist();
}


// get replacement for a track that could not start
void SFTPSource::getReplacement(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    // find first downloaded (might be holes if there's more than one client)
    int index = -1;
    int i     = 0;
    while ((i < futurePlaylist.count()) && (index < 0)) {
        if (futurePlaylist.at(i).cachePath.isValid()) {
            index = i;
        }
        i++;
    }
    if (index < 0) {
        // no more downloaded tracks at this time, can't fullfill
        return;
    }

    // add to return value
    bool tagLibOK = true;
    TrackInfo trackInfo = trackInfoFromFilePath(futurePlaylist.at(index).cachePath.toLocalFile(), futurePlaylist.at(index).clientId, &tagLibOK);
    emit replacement(id, trackInfo);

    // add to already played
    alreadyPlayed.append(formatTrackForLists(futurePlaylist.at(index).clientId, futurePlaylist.at(index).remotePath));

    // remove from predetermined playlist
    futurePlaylist.remove(index);
}


// client wants to dispaly open dialog
void SFTPSource::getOpenTracks(QUuid uniqueId, QString parentId)
{
    if (uniqueId != id) {
        return;
    }

    OpenTracks openTracks;

    // top level
    if (parentId.length() < 1) {
        foreach (SSHClient *client, clients) {
            OpenTrack openTrack;
            openTrack.hasChildren = true;
            openTrack.id = QString("%1").arg(client->getConfig().id);
            openTrack.label = QString("%1 on %2").arg(client->getConfig().dir).arg(client->formatUserHost());
            openTrack.selectable = false;
            openTracks.append(openTrack);
        }
        emit openTracksResults(id, openTracks);
        return;
    }


    // remote path and client id

    QStringList brokeUp = parentId.split(":");
    if (brokeUp.count() == 1) {
        brokeUp.prepend("");
    }
    if (brokeUp.count() != 2) {
        return;
    }

    QString remotePath = brokeUp.at(0);

    bool OK      = false;
    int clientId = brokeUp.at(1).toInt(&OK);
    if (!OK) {
        return;
    }

    if (remotePath.isEmpty()) {
        remotePath = clientFromId(clientId)->getConfig().dir;
    }

    emit clientGetOpenItems(clientId, remotePath);
}


// turn open dialog selections to playlist entries
void SFTPSource::resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks)
{
    if (uniqueId != id) {
        return;
    }

    if (selectedTracks.count() < 1) {
        return;
    }

    TracksInfo returnValue;
    ExtraInfo  returnExtra;
    bool       wasDownload = false;

    foreach (QString selected, selectedTracks) {
        // remote path and client id

        QStringList brokeUp = selected.split(":");
        if (brokeUp.count() != 2) {
            continue;
        }

        QString remotePath = brokeUp.at(0);

        bool OK      = false;
        int clientId = brokeUp.at(1).toInt(&OK);
        if (!OK) {
            continue;
        }

        // check if it's already downloaded maybe
        if (isDownloaded(clientId, remotePath)) {
            QString localPath = clientFromId(clientId)->remoteToLocal(remotePath);

            // do not delete after first play (if it's in cache then it's probably scheduled to be played from pre-determined playlist too)
            doNotDelete.append(QUrl::fromLocalFile(localPath));

            // add to return value
            bool tagLibOK = true;
            TrackInfo trackInfo = trackInfoFromFilePath(localPath, clientId, &tagLibOK);
            returnValue.append(trackInfo);
            addToExtraInfo(&returnExtra, trackInfo.url, "resolved_open_track", 1);
            if (!tagLibOK) {
                addToExtraInfo(&returnExtra, trackInfo.url, "incomplete_tags", 1);
            }

            continue;
        }

        // remember to send to server right after it's downloaded
        playImmediately.append(QUrl::fromLocalFile(clientFromId(clientId)->remoteToLocal(remotePath)));

        // tell the client to download it
        emit clientGetAudio(clientId, QStringList(remotePath));
        wasDownload = true;
    }

    // send the ones we already have
    if (returnValue.count() > 0) {
        emit playlist(id, returnValue, returnExtra);
    }

    // just a little info
    if (wasDownload) {
        emit this->infoMessage(this->id, "<INFO>Tracks will be added after download completes");
    }
}


// request for playlist entries based on search criteria
void SFTPSource::search(QUuid uniqueId, QString criteria)
{
    if (uniqueId != id) {
        return;
    }

    criteria.replace("_", " ").replace(" ", "");

    QStringList matches;
    QHash<QString, QString> matchLabels;

    foreach (int clientId, audioFiles.keys()) {
        QString clientLabel = clientFromId(clientId)->formatUserHost();

        foreach (QString audioFile, audioFiles.value(clientId)) {
            QString modifying(audioFile);
            modifying.replace("_", " ").replace(" ", "");

            if (modifying.contains(criteria, Qt::CaseInsensitive)) {
                QString match = QString("%1:%2").arg(audioFile).arg(clientId);
                matches.append(match);
                matchLabels.insert(match, QString("%1 %2").arg(clientLabel).arg(audioFile));
            }
        }
    }

    qSort(matches);

    OpenTracks openTracks;
    for (int i = 0; i < matches.count(); i++) {
        OpenTrack openTrack;

        openTrack.hasChildren = false;
        openTrack.id = matches.at(i);
        openTrack.label = matchLabels.value(matches.at(i));
        openTrack.selectable = true;
        openTracks.append(openTrack);
    }

    emit searchResults(id, openTracks);
}


// user clicked action that was included in track info
void SFTPSource::action(QUuid uniqueId, int actionKey, TrackInfo trackInfo)
{
    if (uniqueId != id) {
        return;
    }

    // special action
    if (actionKey == RESERVED_ACTION_TRACKINFOUPDATED) {
        // don't know which client it is, let's let every client know, they can figure it out
        emit clientsTrackInfoUpdated(trackInfo);
        return;
    }

    // split action key to client id and real action key
    int clientId = actionKey / 1000;
    actionKey    = actionKey % 1000;

    // check if track actions contain those actions that are added only if taglib was able to read tags
    bool tagLibOK = false;
    foreach (TrackAction trackAction, trackInfo.actions) {
        if (((trackAction.id % 1000) == 10) || ((trackAction.id % 1000) == 11)) {
            tagLibOK = true;
            break;
        }
    }

    // some stuff
    QString remotePath        = clientFromId(clientId)->localToRemote(trackInfo.url.toLocalFile());
    QString formattedForLists = formatTrackForLists(clientId, remotePath);

    // ban
    if (actionKey == 0) {
        banned.append(formattedForLists);

        emit saveGlobalConfiguration(id, configToJsonGlobal());
        emit requestRemoveTrack(id, trackInfo.url);
    }

    // love
    if (actionKey == 1) {
        if (!loved.contains(formattedForLists)) {
            loved.append(formattedForLists);
        }

        emit saveGlobalConfiguration(id, configToJsonGlobal());

        TrackInfo trackInfoTemp;
        trackInfoTemp.track = 0;
        trackInfoTemp.year  = 0;
        trackInfoTemp.url   = trackInfo.url;
        trackInfoTemp.actions.append({ id, 0, "Ban" });
        trackInfoTemp.actions.append({ id, 2, "Unlove" });
        if (tagLibOK) {
            trackInfoTemp.actions.append({ id, 10, "Lyrics search"});
            trackInfoTemp.actions.append({ id, 11, "Band search"});
        }
        emit updateTrackInfo(id, trackInfoTemp);

        QString remote = clientFromId(clientId)->localToRemote(trackInfo.url.toLocalFile());
        remote = remote.left(remote.lastIndexOf("/"));
        remote.replace(clientFromId(clientId)->getConfig().dir, "");
        emit clientFindAudio(clientId, remote);
    }

    // unlove
    if (actionKey == 2) {
        loved.removeAll(formattedForLists);

        emit saveGlobalConfiguration(id, configToJsonGlobal());

        // TODO remove from lovedPlaylist

        TrackInfo trackInfoTemp;
        trackInfoTemp.track = 0;
        trackInfoTemp.year  = 0;
        trackInfoTemp.url   = trackInfo.url;
        trackInfoTemp.actions.append({ id, 0, "Ban" });
        trackInfoTemp.actions.append({ id, 1, "Love" });
        if (tagLibOK) {
            trackInfoTemp.actions.append({ id, 10, "Lyrics search"});
            trackInfoTemp.actions.append({ id, 11, "Band search"});
        }
        emit updateTrackInfo(id, trackInfoTemp);
    }

    // lyrics search
    if (actionKey == 10) {
        emit openUrl(QUrl(QString("http://google.com/search?q=%1 %2 lyrics").arg(trackInfo.performer).arg(trackInfo.title)));
    }

    // band search
    if (actionKey == 11) {
        emit openUrl(QUrl(QString("http://google.com/search?q=\"%1\" band").arg(trackInfo.performer)));
    }

    // diagnostics
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// configuration conversion
QJsonDocument SFTPSource::configToJson()
{
    QJsonObject jsonObject;

    QJsonArray rawClients;
    foreach (SSHClient *client, clients) {
        QVariantHash clientConfigRaw;

        SSHClient::SSHClientConfig clientConfig = client->getConfig();

        QFileInfo privateKeyFileInfo(clientConfig.privateKeyFile);
        QFileInfo publicKeyFileInfo(clientConfig.publicKeyFile);

        clientConfigRaw.insert("host", clientConfig.host);
        clientConfigRaw.insert("user", clientConfig.user);
        if (!clientConfig.fingerprint.isEmpty()) {
            clientConfigRaw.insert("fingerprint", clientConfig.fingerprint);
        }
        if (privateKeyFileInfo.exists() && publicKeyFileInfo.exists()) {
            clientConfigRaw.insert("private_key", clientConfig.privateKeyFile);
            clientConfigRaw.insert("public_key", clientConfig.publicKeyFile);
        }
        if (!clientConfig.dir.isEmpty()) {
            clientConfigRaw.insert("dir", clientConfig.dir);
        }

        rawClients.append(QJsonObject::fromVariantHash(clientConfigRaw));
    }
    if (rawClients.count() > 0) {
        jsonObject.insert("clients", rawClients);
    }

    jsonObject.insert("variation", variationSetting);
    jsonObject.insert("already_played", QJsonArray::fromStringList(alreadyPlayed));

    return QJsonDocument(jsonObject);
}


// configuration conversion
void SFTPSource::jsonToConfig(QJsonDocument jsonDocument)
{
    if (jsonDocument.object().contains("clients")) {
        removeAllClients();

        foreach (QJsonValue jsonValue, jsonDocument.object().value("clients").toArray()) {
            QVariantHash clientConfigRaw = jsonValue.toObject().toVariantHash();

            if (!clientConfigRaw.contains("host") || !clientConfigRaw.contains("user")) {
                continue;
            }

            SSHClient::SSHClientConfig clientConfig;
            clientConfig.id   = 0;
            clientConfig.host = clientConfigRaw.value("host").toString();
            clientConfig.user = clientConfigRaw.value("user").toString();
            if (clientConfigRaw.contains("fingerprint")) {
                clientConfig.fingerprint = clientConfigRaw.value("fingerprint").toString();
            }
            if (clientConfigRaw.contains("private_key") && clientConfigRaw.contains("public_key")) {
                QFileInfo privateKeyFileInfo(clientConfigRaw.value("private_key").toString());
                QFileInfo publicKeyFileInfo(clientConfigRaw.value("public_key").toString());

                if (privateKeyFileInfo.exists() && publicKeyFileInfo.exists()) {
                    clientConfig.privateKeyFile = privateKeyFileInfo.absoluteFilePath();
                    clientConfig.publicKeyFile  = publicKeyFileInfo.absoluteFilePath();
                }
            }
            if (clientConfigRaw.contains("dir")) {
                clientConfig.dir = clientConfigRaw.value("dir").toString();
            }

            addClient(clientConfig);
        }
    }

    if (jsonDocument.object().contains("variation")) {
        variationSetting           = jsonDocument.object().value("variation").toString();
        variationSetCountSinceLow  = (variationSettingId() == 3 ? 3 : 0);
        variationSetCountSinceHigh = 0;
    }

    if (jsonDocument.object().contains("already_played")) {
        alreadyPlayed.clear();

        foreach (QJsonValue jsonValue, jsonDocument.object().value("already_played").toArray()) {
            bool inClients = false;
            foreach (SSHClient *client, clients) {
                if (jsonValue.toString().startsWith(client->getConfig().host + "/")) {
                    inClients = true;
                    break;
                }
            }
            if (inClients) {
                alreadyPlayed.append(jsonValue.toString());
            }
        }
    }
}


// configuration conversion
QJsonDocument SFTPSource::configToJsonGlobal()
{
    QJsonObject jsonObject;

    jsonObject.insert("banned", QJsonArray::fromStringList(banned));
    jsonObject.insert("loved", QJsonArray::fromStringList(loved));
    jsonObject.insert("already_played_loved", QJsonArray::fromStringList(alreadyPlayedLoved));

    return QJsonDocument(jsonObject);
}


// configuration conversion
void SFTPSource::jsonToConfigGlobal(QJsonDocument jsonDocument)
{
    if (jsonDocument.object().contains("banned")) {
        banned.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("banned").toArray()) {
            banned.append(jsonValue.toString());
        }
    }
    if (jsonDocument.object().contains("loved")) {
        loved.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("loved").toArray()) {
            loved.append(jsonValue.toString());
        }
    }
    if (jsonDocument.object().contains("already_played_loved")) {
        alreadyPlayedLoved.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("already_played_loved").toArray()) {
            alreadyPlayedLoved.append(jsonValue.toString());
        }
    }
}


// private method
void SFTPSource::addClient(SSHClient::SSHClientConfig config)
{
    // validations
    if (clients.count() >= 4) {
        emit infoMessage(id, "Maximum 4 SFTP clients allowed");
        return;
    }
    if (config.host.isEmpty() || config.user.isEmpty()) {
        emit infoMessage(id, "Both host and user is required");
        return;
    }

    // needs new id - simply find next available number
    if (config.id < 1) {
        int highestId = 0;
        foreach (SSHClient *client, clients) {
            SSHClient::SSHClientConfig clientConfig = client->getConfig();
            if (clientConfig.id > highestId) {
                highestId = clientConfig.id;
            }
        }
        config.id = highestId + 1;
    }

    // needs new key files - this does not create the files obviously, only establishes their future location and name
    if (config.privateKeyFile.isEmpty() || config.publicKeyFile.isEmpty()) {
        QString timestampString = QString("%1").arg(QDateTime::currentMSecsSinceEpoch());

        config.privateKeyFile = keysDir.absoluteFilePath(timestampString);
        config.publicKeyFile  = keysDir.absoluteFilePath(timestampString + ".pub");
    }

    // needs to know where to store the disk cache
    if (config.cacheDir.isEmpty() && !config.dir.isEmpty()) {
        config.cacheDir = cacheDir.absoluteFilePath(clientCacheDirName(config.user, config.host, config.dir));
    }

    // will run in its own thread because downloads etc. are blocking
    config.thread = new QThread();

    // instantiate client
    SSHClient *client = new SSHClient(config);

    // move to its own thread
    client->moveToThread(config.thread);

    // thread singal connections
    connect(config.thread, SIGNAL(started()),  client, SLOT(run()));
    connect(config.thread, SIGNAL(finished()), client, SLOT(deleteLater()));

    // plugin signal connections
    connect(this,   SIGNAL(clientConnect(int)),                                         client, SLOT(connectSSH(int)));
    connect(this,   SIGNAL(clientDisconnect(int)),                                      client, SLOT(disconnectSSH(int)));
    connect(this,   SIGNAL(clientPasswordEntryResult(int, QString, QString)),           client, SLOT(passwordEntryResult(int, QString, QString)));
    connect(this,   SIGNAL(clientKeySetupQuestionResult(int, bool)),                    client, SLOT(keySetupQuestionResult(int, bool)));
    connect(this,   SIGNAL(clientDirSelectorResult(int, bool, QString)),                client, SLOT(dirSelectorResult(int, bool, QString)));
    connect(this,   SIGNAL(clientFindAudio(int, QString)),                              client, SLOT(findAudio(int, QString)));
    connect(this,   SIGNAL(clientGetAudio(int, QStringList)),                           client, SLOT(getAudio(int, QStringList)));
    connect(this,   SIGNAL(clientGetOpenItems(int, QString)),                           client, SLOT(getOpenItems(int, QString)));
    connect(this,   SIGNAL(clientsTrackInfoUpdated(TrackInfo)),                         client, SLOT(trackInfoUpdated(TrackInfo)));
    connect(client, SIGNAL(connected(int)),                                             this,   SLOT(clientConnected(int)));
    connect(client, SIGNAL(disconnected(int)),                                          this,   SLOT(clientDisconnected(int)));
    connect(client, SIGNAL(showPasswordEntry(int, QString, QString, QString)),          this,   SLOT(clientShowPasswordEntry(int, QString, QString, QString)));
    connect(client, SIGNAL(showKeySetupQuestion(int, QString)),                         this,   SLOT(clientShowKeySetupQuestion(int, QString)));
    connect(client, SIGNAL(showDirSelector(int, QString, QString, SSHClient::DirList)), this,   SLOT(clientShowDirSelector(int, QString, QString, SSHClient::DirList)));
    connect(client, SIGNAL(updateConfig(int)),                                          this,   SLOT(clientUpdateConfig(int)));
    connect(client, SIGNAL(audioList(int, QStringList, bool)),                          this,   SLOT(clientAudioList(int, QStringList, bool)));
    connect(client, SIGNAL(gotAudio(int, QString, QString)),                            this,   SLOT(clientGotAudio(int, QString, QString)));
    connect(client, SIGNAL(gotOpenItems(int, OpenTracks)),                              this,   SLOT(clientGotOpenItems(int, OpenTracks)));
    connect(client, SIGNAL(error(int, QString)),                                        this,   SLOT(clientError(int, QString)));
    connect(client, SIGNAL(info(int, QString)),                                         this,   SLOT(clientInfo(int, QString)));
    connect(client, SIGNAL(stateChanged(int)),                                          this,   SLOT(clientStateChanged(int)));

    // remember this new client
    clients.append(client);
}


// private method
void SFTPSource::removeAllClients()
{
    foreach (SSHClient *client, clients) {
        client->getConfig().thread->requestInterruption();
        client->getConfig().thread->quit();
        client->getConfig().thread->wait();
    }
    clients.clear();

    emit unready(id);
    readySent = false;
}


QString SFTPSource::clientCacheDirName(QString user, QString host, QString remotePath)
{
    remotePath.remove(QRegExp("/$"));

    return QString("%1_%2_%3").arg(user).arg(host).arg(remotePath).replace(QRegExp("\\W"), "_");
}

// private method
void SFTPSource::addToUIQueue(QString UI)
{
    uiQueue.append(UI);
    if (uiQueue.count() == 1) {
        displayNextUIQueue();
    }
}


// private method
void SFTPSource::removeFromUIQueue()
{
    if (uiQueue.count() > 0) {
        uiQueue.removeFirst();
    }
}


// private method
void SFTPSource::displayNextUIQueue()
{
    if (uiQueue.count() > 0) {
        // gotta give a sec for the UI to finish with the animation so it can display the next dialog
        QThread::currentThread()->sleep(1);

        emit uiQml(id, uiQueue.at(0));
    }
}


// private method
bool SFTPSource::isLoved(int clientId, QString remoteFile)
{
    QString formatted = formatTrackForLists(clientId, remoteFile);
    return loved.contains(formatted);
}


// private method
bool SFTPSource::isDirLoved(int clientId, QString remoteDir)
{
    if (!remoteDir.endsWith("/")) {
        remoteDir = remoteDir + "/";
    }
    QString formatted = formatTrackForLists(clientId, remoteDir);
    return (loved.indexOf(QRegExp(QString("^%1.*$").arg(formatted))) >= 0);
}


// private method
bool SFTPSource::isSimilar(int clientId, QString remoteFile)
{
    QString formatted = formatTrackForLists(clientId, remoteFile);
    formatted = formatted.left(formatted.lastIndexOf("/") + 1);

    bool similar = false;
    int  i       = 0;
    while ((i < loved.count()) && !similar) {
        if (loved.at(i).left(loved.at(i).lastIndexOf("/") + 1).compare(formatted) == 0) {
            similar = true;
            break;
        }
        i++;
    }

    return similar;
}


// private method
bool SFTPSource::isInFuturePlaylist(int clientId, QString remoteFile)
{
    bool found = false;
    foreach (PlaylistItem item, futurePlaylist) {
        if ((item.clientId == clientId) && (item.remotePath.compare(remoteFile) == 0)) {
            found = true;
            break;
        }
    }

    return found;
}


// private method
bool SFTPSource::isInLovedPlaylist(int clientId, QString remoteFile)
{
    bool found = false;
    foreach (PlaylistItem item, lovedPlaylist) {
        if ((item.clientId = clientId) && (item.remotePath.compare(remoteFile) == 0)) {
            found = true;
            break;
        }
    }

    return found;
}


// private method
bool SFTPSource::isInSimilarPlaylist(int clientId, QString remoteFile)
{
    bool found = false;
    foreach (PlaylistItem item, similarPlaylist) {
        if ((item.clientId == clientId) && (item.remotePath.compare(remoteFile) == 0)) {
            found = true;
            break;
        }
    }

    return found;
}


// private method
int SFTPSource::countSameDirInSimilarPlaylist(int clientId, QString remoteFile)
{
    QString remoteDir = remoteFile.left(remoteFile.lastIndexOf("/"));

    int count = 0;
    foreach (PlaylistItem item, similarPlaylist) {
        if ((item.clientId == clientId) && (item.remotePath.left(item.remotePath.lastIndexOf("/")).compare(remoteDir) == 0)) {
            count++;
        }
    }

    return count;
}


// private method
bool SFTPSource::isDownloaded(int clientId, QString remoteFile)
{
    return QFileInfo::exists(clientFromId(clientId)->remoteToLocal(remoteFile));
}


// private method
void SFTPSource::appendToPlaylist()
{
    // can't do anything without files
    if (audioFiles.count() < 1) {
        return;
    }

    // will also start downloading
    QHash<int, QStringList *> downloadList;

    // fill or re-fill predetermed playlist
    while (futurePlaylist.count() < PLAYLIST_DESIRED_SIZE) {
        // select a client for this batch
        QList<int> clientIds = audioFiles.keys();
        int currentClientId = clientIds.at(qrand() % clientIds.count());

        // determine which variation setting to use (3 means user selected random variation)
        int currentVariation = variationSettingId();
        if (currentVariation == 3) {
            if (variationSetCountSinceHigh >= 4) {
                currentVariation = 2;
            }
            else if (variationSetCountSinceLow >= 4) {
                currentVariation = qrand() % 3;
            }
            else {
                currentVariation = (qrand() % 2) + 1;
            }
        }

        // get remaining tracks for currect client
        QStringList remaining;
        foreach (QString audioFile, audioFiles.value(currentClientId)) {
            if (!isInFuturePlaylist(currentClientId, audioFile) && !alreadyPlayed.contains(formatTrackForLists(currentClientId, audioFile)) && !banned.contains(formatTrackForLists(currentClientId, audioFile))) {
                remaining.append(audioFile);
            }
        }
        if ((remaining.count() < 1) && clientFromId(currentClientId)->isConnected()) {
            alreadyPlayed.clear();
            foreach (QString audioFile, audioFiles.value(currentClientId)) {
                if (!isInFuturePlaylist(currentClientId, audioFile) && !banned.contains(formatTrackForLists(currentClientId, audioFile))) {
                    remaining.append(audioFile);
                }
            }
        }
        if (remaining.count() < 1) {
            emit unready(id);
            readySent = false;
            break;
        }

        // determine batch size and "album" to use based on variation setting
        int     variationCount;
        QString variationDir;
        switch (currentVariation) {
            case 0:
                // low variation: 4 - 6 tracks from same dir (usually each dir is one album but not necessary)
                variationCount = (qrand() % 3) + 4;
                variationDir = remaining.at(qrand() % remaining.count());
                variationDir = variationDir.left(variationDir.lastIndexOf("/"));
                variationSetCountSinceHigh++;
                variationSetCountSinceLow = 0;
                break;
            case 1:
                // medium variation: 2 - 3 tracks from same dir
                variationCount = (qrand() % 2) + 2;
                variationDir = remaining.at(qrand() % remaining.count());
                variationDir = variationDir.left(variationDir.lastIndexOf("/"));
                variationSetCountSinceHigh++;
                variationSetCountSinceLow++;
                break;
            default:
                // high variation: 4 tracks totally random
                variationCount = 4;
                variationSetCountSinceHigh = 0;
                variationSetCountSinceLow++;
        }

        // get list of tracks to choose from (either from dir selected based on variation, or all tracks)
        QStringList chooseFrom;
        foreach (QString audioFile, remaining) {
            // this condition has no effect if variationDir is empty
            if (audioFile.startsWith(variationDir)) {
                chooseFrom.append(audioFile);
            }
        }

        // add this batch to predetermined playlist and download list
        int i = 0;
        while ((i < variationCount) && (chooseFrom.count() > 0)) {
            int index = qrand() % chooseFrom.count();

            PlaylistItem playlistItem;
            playlistItem.clientId   = currentClientId;
            playlistItem.remotePath = chooseFrom.at(index);
            if (isDownloaded(currentClientId, playlistItem.remotePath)) {
                playlistItem.cachePath = QUrl::fromLocalFile(clientFromId(currentClientId)->remoteToLocal(playlistItem.remotePath));
            }
            else {
                if (!downloadList.contains(currentClientId)) {
                    downloadList.insert(currentClientId, new QStringList());
                }
                downloadList.value(playlistItem.clientId)->append(playlistItem.remotePath);
            }
            futurePlaylist.append(playlistItem);

            chooseFrom.removeAt(index);

            i++;
        }
    }

    // start downloads
    foreach (int clientId, downloadList.keys()) {
        emit clientGetAudio(clientId, QStringList(*downloadList.value(clientId)));
    }

    // housekeeping
    foreach (QStringList *downloads, downloadList) {
        delete downloads;
    }
}


// private method
QString SFTPSource::formatTrackForLists(int clientId, QString filePath)
{
    return QString("%1/%2").arg(clientFromId(clientId)->getConfig().host).arg(filePath);
}


// private slot from SFTP client
void SFTPSource::clientConnected(int id)
{
    // SFTP connection successful, get list of audio files
    emit clientFindAudio(id, "");
}


// private slot from SFTP client
void SFTPSource::clientDisconnected(int id)
{
    audioFiles.remove(id);

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// private slot from SFTP client
void SFTPSource::clientShowPasswordEntry(int id, QString userAtHost, QString fingerprint, QString explanation)
{
    QFile pswEntryFile("://SFTPPsw.qml");
    pswEntryFile.open(QFile::ReadOnly);
    QString pswEntry = pswEntryFile.readAll();
    pswEntryFile.close();

    pswEntry.replace("<user@host>", userAtHost);
    pswEntry.replace("<fingerprint_display>", "Server's fingerprint: " + fingerprint);
    pswEntry.replace("<fingerprint>", fingerprint);
    pswEntry.replace("<explanation>", explanation);
    pswEntry.replace("\"<clientId>\"", QString("%1").arg(id));

    addToUIQueue(pswEntry);
}


// private slot from SFTP client
void SFTPSource::clientShowKeySetupQuestion(int id, QString userAtHost)
{
    QFile keySetupQuestionFile("://SFTPKeySetupQuestion.qml");
    keySetupQuestionFile.open(QFile::ReadOnly);
    QString keySetupQuestion = keySetupQuestionFile.readAll();
    keySetupQuestionFile.close();

    keySetupQuestion.replace("<user@host>", userAtHost);
    keySetupQuestion.replace("\"<clientId>\"", QString("%1").arg(id));

    addToUIQueue(keySetupQuestion);
}


// private slot from SFTP client
void SFTPSource::clientShowDirSelector(int id, QString userAtHost, QString currentDir, SSHClient::DirList dirList)
{
    qSort(dirList.begin(), dirList.end(), [](SSHClient::DirListItem a, SSHClient::DirListItem b) {
        return (a.name.compare(b.name, Qt::CaseInsensitive) < 0);
    });

    QString listElements;
    foreach (SSHClient::DirListItem item, dirList) {
        if (item.isDir && ((item.name.compare("..") == 0) || (!item.name.startsWith(".")))) {
            listElements.append(QString("ListElement { name: \"%1\"; full_path: \"%2\"; } ").arg(item.name).arg(item.fullPath));
        }
    }

    QFile dirSelectorFile("://SFTPDirChoose.qml");
    dirSelectorFile.open(QFile::ReadOnly);
    QString dirSelector = dirSelectorFile.readAll();
    dirSelectorFile.close();

    dirSelector.replace("ListElement{}", listElements);
    dirSelector.replace("<user@host>", userAtHost);
    dirSelector.replace("<current_dir>", currentDir);
    dirSelector.replace("\"<clientId>\"", QString("%1").arg(id));

    addToUIQueue(dirSelector);
}


// private slot from SFTP client
void SFTPSource::clientUpdateConfig(int id)
{
    SSHClient                  *client = clientFromId(id);
    SSHClient::SSHClientConfig  config = client->getConfig();
    if (config.cacheDir.isEmpty() && !config.dir.isEmpty()) {
        client->setCacheDir(cacheDir.absoluteFilePath(clientCacheDirName(config.user, config.host, config.dir)));
    }

    emit saveConfiguration(this->id, configToJson());
}


// private slot from SFTP client
void SFTPSource::clientAudioList(int id, QStringList files, bool alreadyCached)
{
    // so that similar tracks are not the same all the time
    std::random_shuffle(files.begin(), files.end());

    QStringList downloadList;
    foreach (QString file, files) {
        // add to list of remote files
        if (!audioFiles.value(id).contains(file) && (!alreadyCached || (!isLoved(id, file) && !isSimilar(id, file)))) {
            audioFiles[id].append(file);
        }

        // add to loved playlist
        if (isLoved(id, file)) {
            bool dl = isDownloaded(id, file);
            if (!isInLovedPlaylist(id, file)) {
                PlaylistItem playlistItem;
                playlistItem.clientId   = id;
                playlistItem.remotePath = file;
                if (dl) {
                    playlistItem.cachePath = QUrl::fromLocalFile(clientFromId(id)->remoteToLocal(file));
                }
                lovedPlaylist.append(playlistItem);
            }
            if (!dl) {
                downloadList.append(file);
            }
            continue;
        }

        // add to similar playlist
        if (isSimilar(id, file) && (countSameDirInSimilarPlaylist(id, file) < 3)) {
            bool dl = isDownloaded(id, file);
            if (!isInSimilarPlaylist(id, file)) {
                PlaylistItem playlistItem;
                playlistItem.clientId   = id;
                playlistItem.remotePath = file;
                if (dl) {
                    playlistItem.cachePath = QUrl::fromLocalFile(clientFromId(id)->remoteToLocal(file));
                }
                similarPlaylist.append(playlistItem);
            }
            if (!dl) {
                downloadList.append(file);
            }
        }
    }

    // fill up pre-determind playlist
    appendToPlaylist();

    // see if ready can be sent now
    readyIfReady();

    // download loved and similar if needed
    if (downloadList.count() > 0) {
        emit clientGetAudio(id, downloadList);
    }
}


// private slot from SFTP client
void SFTPSource::clientGotAudio(int id, QString remote, QString local)
{
    // check if it was opened manually
    QUrl localUrl = QUrl::fromLocalFile(local);
    if (playImmediately.contains(localUrl)) {
        // send to server

        TracksInfo returnValue;
        ExtraInfo  returnExtra;

        bool tagLibOK = true;
        TrackInfo trackInfo = trackInfoFromFilePath(local, id, &tagLibOK);
        returnValue.append(trackInfo);
        addToExtraInfo(&returnExtra, trackInfo.url, "resolved_open_track", 1);
        if (!tagLibOK) {
            addToExtraInfo(&returnExtra, trackInfo.url, "incomplete_tags", 1);
        }

        emit playlist(this->id, returnValue, returnExtra);

        // can come up in the playlist later
        playImmediately.removeAll(QUrl::fromLocalFile(local));
    }
    else {
        // update predetermined playlist with local path
        for (int i = 0; i < futurePlaylist.count(); i++) {
            if ((futurePlaylist.at(i).clientId == id) && (futurePlaylist.at(i).remotePath.compare(remote) == 0)) {
                futurePlaylist[i].cachePath = QUrl::fromLocalFile(local);
            }
        }
    }

    // see if ready can be sent now
    readyIfReady();

    // maybe it's also loved or similar
    for (int i = 0; i < lovedPlaylist.count(); i++) {
        if ((lovedPlaylist.at(i).clientId == id) && (lovedPlaylist.at(i).remotePath.compare(remote) == 0)) {
            lovedPlaylist[i].cachePath = QUrl::fromLocalFile(local);
        }
    }
    for (int i = 0; i < similarPlaylist.count(); i++) {
        if ((similarPlaylist.at(i).clientId == id) && (similarPlaylist.at(i).remotePath.compare(remote) == 0)) {
            similarPlaylist[i].cachePath = QUrl::fromLocalFile(local);
        }
    }
}


// private slot from SFTP client
void SFTPSource::clientGotOpenItems(int id, OpenTracks openTracks)
{
    Q_UNUSED(id);

    emit openTracksResults(this->id, openTracks);
}


// private slot from SFTP client
void SFTPSource::clientError(int id, QString errorMessage)
{
    SSHClient *client = clientFromId(id);
    if (client != NULL) {
        emit infoMessage(this->id, QString("%1 - %2").arg(client->getConfig().host).arg(errorMessage));
    }
}


// private slot from SFTP client
void SFTPSource::clientInfo(int id, QString infoMessage)
{
    SSHClient *client = clientFromId(id);
    if (client != NULL) {
        emit this->infoMessage(this->id, QString("<INFO>%1 - %2").arg(client->getConfig().host).arg(infoMessage));
    }
}


// private slot from SFTP client
void SFTPSource::clientStateChanged(int id)
{
    Q_UNUSED(id);

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// helper
int SFTPSource::variationSettingId()
{
    QStringList variations({ "Low", "Medium", "High", "Random" });
    return variations.indexOf(variationSetting);
}


// private method
void SFTPSource::reduceCache()
{
    // check if there's a dir that doesn't belong to any client anymore
    QFileInfoList clientCacheDirInfos = cacheDir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot);
    QStringList toBeDeleted;
    foreach (QFileInfo clientCacheDirInfo, clientCacheDirInfos) {
        bool found = false;
        foreach (SSHClient *sshClient, clients) {
            if (clientCacheDirInfo.fileName().compare(clientCacheDirName(sshClient->getConfig().user, sshClient->getConfig().host, sshClient->getConfig().dir)) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            toBeDeleted.append(clientCacheDirInfo.absoluteFilePath());
        }
    }
    foreach (QString dirName, toBeDeleted) {
        QDir(dirName).removeRecursively();
    }

    // get storage stats
    QStorageInfo storageInfo(cacheDir);
    double bytesTotalMegabytes     = (double)storageInfo.bytesTotal()     / (1024 * 1024);
    double bytesAvailableMegabytes = (double)storageInfo.bytesAvailable() / (1024 * 1024);

    // max size limit, one gigabyte
    double maxSizeMegabytes = 1024;

    // total cache size
    double cacheSumSize = 0;
    dirSumSizeMegabytes(cacheDir, &cacheSumSize);

    // delete until desired size reached
    while (((cacheSumSize > (bytesTotalMegabytes * 0.1)) || (cacheSumSize > ((bytesAvailableMegabytes + cacheSumSize) * 0.25)) || (cacheSumSize > maxSizeMegabytes))) {
        foreach (SSHClient *sshClient, clients) {
            // get list of files and subdirs
            QFileInfoList entries = QDir(cacheDir.absoluteFilePath(clientCacheDirName(sshClient->getConfig().user, sshClient->getConfig().host, sshClient->getConfig().dir))).entryInfoList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

            // sort, oldest first
            qSort(entries.begin(), entries.end(), [](QFileInfo a, QFileInfo b) {
                return (a.lastModified() < b.lastModified());
            });

            // try to find a subdir that doesn't contain loved first, otherwise just delete the first (a.k.a. oldest) item, whatever it is
            QFileInfo toBeDeleted;
            foreach (QFileInfo entry, entries) {
                if (!entry.isDir()) {
                    continue;
                }
                if (isDirLoved(sshClient->getConfig().id, sshClient->localToRemote(entry.absoluteFilePath()))) {
                    continue;
                }
                toBeDeleted = entry;
                break;
            }
            if (!toBeDeleted.exists()) {
                toBeDeleted = entries.first().absoluteFilePath();
            }

            // do the delete
            if (toBeDeleted.isDir()) {
                QDir(toBeDeleted.absoluteFilePath()).removeRecursively();
            }
            else {
                QFile(toBeDeleted.absoluteFilePath()).remove();
            }
        }

        // refresh cache size
        cacheSumSize = 0;
        dirSumSizeMegabytes(cacheDir, &cacheSumSize);
    }

    // delete the oldest loved dir to refresh similar (this might delete some lower-level subdirs that aren't contain loved, but that's OK)
    foreach (SSHClient *sshClient, clients) {
        // get list of files and subdirs
        QFileInfoList entries = QDir(cacheDir.absoluteFilePath(clientCacheDirName(sshClient->getConfig().user, sshClient->getConfig().host, sshClient->getConfig().dir))).entryInfoList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

        // filter for loved
        QFileInfoList lovedEntries;
        foreach (QFileInfo entry, entries) {
            if (!entry.isDir()) {
                continue;
            }
            if (isDirLoved(sshClient->getConfig().id, sshClient->localToRemote(entry.absoluteFilePath()))) {
                lovedEntries.append(entry);
            }
        }

        if (lovedEntries.count() > 0) {
            // sort, oldest first
            qSort(lovedEntries.begin(), lovedEntries.end(), [](QFileInfo a, QFileInfo b) {
                return (a.lastModified() < b.lastModified());
            });

            QDir(lovedEntries.at(0).absoluteFilePath()).removeRecursively();
        }
    }
}


// helper
void SFTPSource::dirSumSizeMegabytes(QDir dir, double *sumSize)
{
    QFileInfoList entries = dir.entryInfoList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    foreach (QFileInfo entry, entries) {
        if (entry.isDir()) {
            dirSumSizeMegabytes(QDir(entry.absoluteFilePath()), sumSize);
            continue;
        }
        *sumSize += (double)entry.size() / (1024 * 1024);
    }
}


// helper
void SFTPSource::setState(State state)
{
    this->state = state;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// helper
void SFTPSource::addToExtraInfo(ExtraInfo *extraInfo, QUrl url, QString key, QVariant value)
{
    QVariantHash temp = extraInfo->value(url);
    temp.insert(key, value);
    extraInfo->insert(url, temp);
}


// helper
SSHClient *SFTPSource::clientFromId(int id)
{
    SSHClient *returnValue = NULL;

    foreach (SSHClient *client, clients) {
        if (client->getConfig().id == id) {
            returnValue = client;
            break;
        }
    }

    return returnValue;
}


// helper
void SFTPSource::readyIfReady()
{
    if (!readySent) {
        // count number of downloaded tracks
        int downloaded = 0;
        foreach (PlaylistItem playlistItem, futurePlaylist) {
            if (playlistItem.cachePath.isValid()) {
                downloaded++;
            }
        }

        // send ready if enough tracks are downloaded already
        if (downloaded >= qMin(PLAYLIST_READY_SIZE, PLAYLIST_DESIRED_SIZE)) {
            emit ready(this->id);
            readySent          = true;
            unableToStartCount = 0;
        }
    }
}


// helper
TrackInfo SFTPSource::trackInfoFromFilePath(QString filePath, int clientId, bool *tagLibOK)
{
    // defaults
    TrackInfo trackInfo;
    trackInfo.url   = QUrl::fromLocalFile(filePath);
    trackInfo.cast  = false;
    trackInfo.year  = 0;
    trackInfo.track = 0;

    // try taglib first
    *tagLibOK = true;
    TagLib::FileRef fileRef(QFile::encodeName(filePath).constData());
    if (!fileRef.isNull() && !fileRef.tag()->isEmpty()) {
        trackInfo.title     = TStringToQString(fileRef.tag()->title());
        trackInfo.performer = TStringToQString(fileRef.tag()->artist());
        trackInfo.album     = TStringToQString(fileRef.tag()->album());
        trackInfo.year      = fileRef.tag()->year();
        trackInfo.track     = fileRef.tag()->track();
    }


    // figure out based on file path if taglib failed
    if (trackInfo.title.isEmpty() || trackInfo.performer.isEmpty() || trackInfo.album.isEmpty()) {
        *tagLibOK = false;

        // track info discovery
        QStringList trackRelative = QString(filePath).remove(clientFromId(clientId)->getConfig().cacheDir).split("/");
        if (trackRelative.at(0).isEmpty()) {
            trackRelative.removeFirst();
        }
        if (trackRelative.count() > 0) {
            if (trackInfo.title.isEmpty()) {
                trackInfo.title = trackRelative.last().replace(".mp3", "", Qt::CaseInsensitive);
            }
            trackRelative.removeLast();
        }
        if (trackRelative.count() > 0) {
            if (trackInfo.performer.isEmpty()) {
                trackInfo.performer = trackRelative.first();
            }
            trackRelative.removeFirst();
        }
        if ((trackRelative.count() > 0) && (trackInfo.album.isEmpty())) {
            trackInfo.album = trackRelative.join(" - ");
        }
    }

    // search for pictures
    QVector<QUrl> pictures;
    QFileInfoList entries = QDir(filePath.left(filePath.lastIndexOf("/"))).entryInfoList();
    foreach (QFileInfo entry, entries) {
        if (entry.exists() && entry.isFile() && (entry.fileName().endsWith(".jpg", Qt::CaseInsensitive) || entry.fileName().endsWith(".jpeg", Qt::CaseInsensitive) || entry.fileName().endsWith(".png", Qt::CaseInsensitive))) {
            pictures.append(QUrl::fromLocalFile(entry.absoluteFilePath()));
        }
    }
    trackInfo.pictures.append(pictures);

    QString remotePath = clientFromId(clientId)->localToRemote(trackInfo.url.toLocalFile());

    trackInfo.actions.append({ id, clientId * 1000 + 0, "Ban" });
    if (loved.contains(formatTrackForLists(clientId, remotePath))) {
        trackInfo.actions.append({ id, clientId * 1000 + 2, "Unlove" });
    }
    else {
        trackInfo.actions.append({ id, clientId * 1000 + 1, "Love" });
    }
    if (*tagLibOK) {
        trackInfo.actions.append({ id, clientId * 1000 + 10, "Lyrics search"});
        trackInfo.actions.append({ id, clientId * 1000 + 11, "Band search"});
    }

    return trackInfo;
}


// diagnostics
void SFTPSource::sendDiagnosticsData()
{
    DiagnosticData diagnosticData;

    diagnosticData.append({ "Banned", QString("%1").arg(banned.count()) });
    diagnosticData.append({ "Loved",  QString("%1").arg(loved.count()) });

    foreach (SSHClient *sshClient, clients) {
        SSHClient::SSHClientState state    = sshClient->getState();
        QString                   stateStr = "Unknown";
        switch (state) {
            case SSHClient::Idle:
                stateStr = "Idle";
                break;
            case SSHClient::Connecting:
                stateStr = "Connecting";
                break;
            case SSHClient::Disconnecting:
                stateStr = "Disconnecting";
                break;
            case SSHClient::CheckingCache:
                stateStr = "Checking cache";
                break;
            case SSHClient::ExecutingSSH:
                stateStr = "Executing SSH command";
                break;
            case SSHClient::Downloading:
                stateStr = "Downloading";
                break;
            case SSHClient::Uploading:
                stateStr = "Uploading";
                break;
            case SSHClient::GettingDirList:
                stateStr = "Getting directory listing";
        }

        qint64 bytes = sshClient->getLoadingBytes();
        QString bytesStr = "";
        if (bytes >= (1024 * 1024)) {
            bytesStr = QString(" %1 MB").arg(static_cast<double>(bytes) / (1024 * 1024), 0, 'f', 2);
        }
        else if (bytes >= 1024) {
            bytesStr = QString(" %1 kB").arg(static_cast<double>(bytes) / 1024, 0, 'f', 1);
        }
        else if (bytes > 0) {
            bytesStr = QString(" %1 bytes").arg(static_cast<double>(bytes), 0, 'f', 0);
        }

        diagnosticData.append({ sshClient->formatUserHost(), stateStr + bytesStr });
    }

    emit diagnostics(id, diagnosticData);
}
