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

    variationSetting     = "Medium";
    readySent            = false;
    sendDiagnostics      = false;

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
    cacheDir.removeRecursively();
    cacheDir.mkpath(cacheDir.absolutePath());
}


// destructor
SFTPSource::~SFTPSource()
{
    // TODO this must be faster
    if (state != SSHLib2Fail) {
        removeAllClients();
        libssh2_exit();
    }
    cacheDir.removeRecursively();
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

    // must delay starting up to give a chance for the UI to display in case login is required
    // TODO requests to show SFTPSettings must be denied during this
    QTimer::singleShot(5000, this, SLOT(delayStartup()));
}


// timer slot
void SFTPSource::delayStartup()
{
    emit loadConfiguration(id);
}


// slot receiving configuration
void SFTPSource::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfig(configuration);

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// slot receiving configuration
void SFTPSource::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
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
        // TODO send unready if there's no connected clients anymore
    }

    // delete client for good
    if (resultsHash.value("button").toString().compare("remove") == 0) {
        SSHClient *client = clientFromId(resultsHash.value("client_id").toInt());
        client->getConfig().thread->quit();
        client->getConfig().thread->wait();
        clients.removeOne(client);
        emit saveConfiguration(id, configToJson());
        // TODO send unready if there's no connected clients anymore
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
    Q_UNUSED(uniqueId);
    Q_UNUSED(url);

    // don't care for now, maybe later
}


// this should never happen because of no casts
void SFTPSource::castFinishedEarly(QUuid uniqueId, QUrl url, int playedSeconds)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(url);
    Q_UNUSED(playedSeconds);
}


// server says this track is no longer needed (for now)
void SFTPSource::done(QUuid uniqueId, QUrl url)
{
    if (uniqueId != id) {
        return;
    }

    // remove from cache
    QFile file(url.toLocalFile());
    file.remove();
}

// server's request for playlist entries
void SFTPSource::getPlaylist(QUuid uniqueId, int trackCount, int mode)
{
    if (uniqueId != id) {
        return;
    }

    TracksInfo returnValue;
    ExtraInfo  returnExtra;

    // TODO loved
    if (mode == PLAYLIST_MODE_LOVED) {
        //returnExtra.insert(trackInfo.url, {{ "loved", PLAYLIST_MODE_LOVED }});
    }
    else if (mode == PLAYLIST_MODE_LOVED_SIMILAR) {
        //returnExtra.insert(trackInfo.url, {{ "loved", PLAYLIST_MODE_LOVED_SIMILAR }});
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
            TrackInfo trackInfo = trackInfoFromFilePath(futurePlaylist.at(i).cachePath.toLocalFile(), futurePlaylist.at(i).clientId);
            returnValue.append(trackInfo);

            // remove from predetermined playlist
            futurePlaylist.remove(i);

            count++;
        }
    }

    // send them back to the server
    emit playlist(id, returnValue, returnExtra);

    // TODO already played
    /*
        // must save updated configuration
        emit saveConfiguration(id, configToJson());
    */

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

    // TODO this
}


// client wants to dispaly open dialog
void SFTPSource::getOpenTracks(QUuid uniqueId, QString parentId)
{
    if (uniqueId != id) {
        return;
    }

    // TODO this
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

    // TODO this
}


// request for playlist entries based on search criteria
void SFTPSource::search(QUuid uniqueId, QString criteria)
{
    if (uniqueId != id) {
        return;
    }

    // TODO this
}


// user clicked action that was included in track info
void SFTPSource::action(QUuid uniqueId, int actionKey, TrackInfo trackInfo)
{
    if (uniqueId != id) {
        return;
    }

    // TODO this

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
        variationSetting = jsonDocument.object().value("variation").toString();
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
    if (config.cacheDir.isEmpty()) {
        config.cacheDir = cacheDir.absoluteFilePath(QString("%1").arg(config.id));
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
    connect(this,   SIGNAL(clientFindAudio(int)),                                       client, SLOT(findAudio(int)));
    connect(this,   SIGNAL(clientGetAudio(int, QStringList)),                           client, SLOT(getAudio(int, QStringList)));
    connect(client, SIGNAL(connected(int)),                                             this,   SLOT(clientConnected(int)));
    connect(client, SIGNAL(disconnected(int)),                                          this,   SLOT(clientDisconnected(int)));
    connect(client, SIGNAL(showPasswordEntry(int, QString, QString, QString)),          this,   SLOT(clientShowPasswordEntry(int, QString, QString, QString)));
    connect(client, SIGNAL(showKeySetupQuestion(int, QString)),                         this,   SLOT(clientShowKeySetupQuestion(int, QString)));
    connect(client, SIGNAL(showDirSelector(int, QString, QString, SSHClient::DirList)), this,   SLOT(clientShowDirSelector(int, QString, QString, SSHClient::DirList)));
    connect(client, SIGNAL(updateConfig(int)),                                          this,   SLOT(clientUpdateConfig(int)));
    connect(client, SIGNAL(audioList(int, QStringList)),                                this,   SLOT(clientAudioList(int, QStringList)));
    connect(client, SIGNAL(gotAudio(int, QString, QString)),                            this,   SLOT(clientGotAudio(int, QString, QString)));
    connect(client, SIGNAL(error(int, QString)),                                        this,   SLOT(clientError(int, QString)));
    connect(client, SIGNAL(info(int, QString)),                                         this,   SLOT(clientInfo(int, QString)));

    // remember this new client
    clients.append(client);

    // start its thread
    config.thread->start();
}


// private method
void SFTPSource::removeAllClients()
{
    // TODO this must be faster when quitting
    foreach (SSHClient *client, clients) {
        client->getConfig().thread->quit();
        client->getConfig().thread->wait();
    }
    clients.clear();
    // TODO send unready
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
void SFTPSource::appendToPlaylist()
{
    // will also start downloading
    QHash<int, QStringList *> downloadList;

    // fill or re-fill predetermed playlist
    while (futurePlaylist.count() < PLAYLIST_DESIRED_SIZE) {
        // select a client for this batch
        QList<int> clientIds = audioFiles.keys();
        int currentClientId = clientIds.at(qrand() % clientIds.count());

        // detrmine which variation setting to use (3 means user selected random variation)
        int currentVariation = variationSettingId();
        if (currentVariation == 3) {
            currentVariation = qrand() % 3;
        }

        // determine batch size and "album" to use based on variation setting
        int     variationCount;
        QString variationDir;
        switch (currentVariation) {
            case 0:
                // low variation: 4 - 6 tracks from same dir (usually each dir is one album but not necessary)
                variationCount = (qrand() % 3) + 4;
                variationDir = audioFiles.value(currentClientId).at(qrand() % audioFiles.value(currentClientId).count());
                variationDir = variationDir.left(variationDir.lastIndexOf("/"));
                break;
            case 1:
                // medium variation: 2 - 3 tracks from same dir
                variationCount = (qrand() % 2) + 2;
                variationDir = audioFiles.value(currentClientId).at(qrand() % audioFiles.value(currentClientId).count());
                variationDir = variationDir.left(variationDir.lastIndexOf("/"));
                break;
            default:
                // high variation: 4 tracks totally random
                variationCount = 4;
        }

        // get list of tracks to choose from (either from dir selected based on variation, or all tracks)
        // TODO already played and banned
        QStringList chooseFrom;
        foreach (QString audioFile, audioFiles.value(currentClientId)) {
            // this condition has no effect if variationDir is empty
            if (audioFile.startsWith(variationDir)) {
                chooseFrom.append(audioFile);
            }
        }

        // add this batch to predetermined playlist and download list
        int i = 0;
        while (i < variationCount) {
            PlaylistItem playlistItem;
            playlistItem.clientId   = currentClientId;
            playlistItem.remotePath = chooseFrom.at(qrand() % chooseFrom.count());

            futurePlaylist.append(playlistItem);

            if (!downloadList.contains(currentClientId)) {
                downloadList.insert(currentClientId, new QStringList());
            }
            downloadList.value(playlistItem.clientId)->append(playlistItem.remotePath);

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


// private slot from SFTP client
void SFTPSource::clientConnected(int id)
{
    // SFTP connection successful, get list of audio files
    emit clientFindAudio(id);
}


// private slot from SFTP client
void SFTPSource::clientDisconnected(int id)
{
    audioFiles.remove(id);

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    // TODO send unready if there's no connected clients anymore
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

    QFile keySelectorFile("://SFTPDirChoose.qml");
    keySelectorFile.open(QFile::ReadOnly);
    QString keySelector = keySelectorFile.readAll();
    keySelectorFile.close();

    keySelector.replace("ListElement{}", listElements);
    keySelector.replace("<user@host>", userAtHost);
    keySelector.replace("<current_dir>", currentDir);
    keySelector.replace("\"<clientId>\"", QString("%1").arg(id));

    addToUIQueue(keySelector);
}


// private slot from SFTP client
void SFTPSource::clientUpdateConfig(int id)
{
    Q_UNUSED(id);

    emit saveConfiguration(this->id, configToJson());
}


// private slot from SFTP client
void SFTPSource::clientAudioList(int id, QStringList files)
{
    audioFiles.insert(id, files);
    appendToPlaylist();
}


// private slot from SFTP client
void SFTPSource::clientGotAudio(int id, QString remote, QString local)
{
    // update predetermined playlist with local path
    for (int i = 0; i < futurePlaylist.count(); i++) {
        if ((futurePlaylist.at(i).clientId == id) && (futurePlaylist.at(i).remotePath.compare(remote) == 0)) {
            futurePlaylist[i].cachePath = QUrl::fromLocalFile(local);
        }
    }

    // see if ready can be sent now
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
            readySent = true;
        }
    }
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


// helper
int SFTPSource::variationSettingId()
{
    QStringList variations({ "Low", "Medium", "High", "Random" });
    return variations.indexOf(variationSetting);
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
TrackInfo SFTPSource::trackInfoFromFilePath(QString filePath, int clientId)
{
    // defaults
    TrackInfo trackInfo;
    trackInfo.url   = QUrl::fromLocalFile(filePath);
    trackInfo.cast  = false;
    trackInfo.year  = 0;
    trackInfo.track = 0;

    // try taglib first
    bool tagLibOK = true;
    // TODO taglib
    /*
        TagLib::FileRef fileRef(QFile::encodeName(filePath).constData());
        if (!fileRef.isNull() && !fileRef.tag()->isEmpty()) {
        trackInfo.title     = TStringToQString(fileRef.tag()->title());
        trackInfo.performer = TStringToQString(fileRef.tag()->artist());
        trackInfo.album     = TStringToQString(fileRef.tag()->album());
        trackInfo.year      = fileRef.tag()->year();
        trackInfo.track     = fileRef.tag()->track();
        }
    */

    // figure out based on file path if taglib failed
    if (trackInfo.title.isEmpty() || trackInfo.performer.isEmpty() || trackInfo.album.isEmpty()) {
        tagLibOK = false;

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

    // TODO actions
    /*
        trackInfo.actions.append({ id, 0, "Ban" });
        if (lovedFileNames.contains(filePath)) {
        trackInfo.actions.append({ id, 2, "Unlove" });
        }
        else {
        trackInfo.actions.append({ id, 1, "Love" });
        }
        if (tagLibOK) {
        trackInfo.actions.append({ id, 10, "Lyrics search"});
        trackInfo.actions.append({ id, 11, "Band search"});
        }
    */

    return trackInfo;
}


// diagnostics
void SFTPSource::sendDiagnosticsData()
{
    // TODO this
}
