/*
    This file is part of Waver

    Copyright (C) 2017-2019 Peter Papp <peter.papp.p@gmail.com>

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


#include "ampache.h"

// plugin factory
void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal)
{
    if (pluginTypesMask & PLUGIN_TYPE_SOURCE) {
        retVal->append((QObject *) new Ampache());
    }
}


// constructor
Ampache::Ampache()
{
    id  = QUuid("{2650B433-F8DA-4D1B-81E8-CA389BFF1049}");

    networkAccessManager  = nullptr;
    serverApiVersion      = 0;
    readySent             = false;
    sendDiagnostics       = false;
    resolveScheduled      = false;

    lastReturnedAlbumId = 0;

    variationSetting           = "Medium";
    variationSetCountSinceHigh = 0;
    variationSetCountSinceLow  = 0;
    variationSetCurrentRemainingAlbum();

    setState(Idle);
}


// destructor
Ampache::~Ampache()
{
    if (networkAccessManager != nullptr) {
        networkAccessManager->deleteLater();
    }
}


// overridden virtual function
int Ampache::pluginType()
{
    return PLUGIN_TYPE_SOURCE;
}


// overridden virtual function
QString Ampache::pluginName()
{
    return "Ampache";
}


// overridden virtual function
int Ampache::pluginVersion()
{
    return 1;
}


// overrided virtual function
QString Ampache::waverVersionAPICompatibility()
{
    return "0.0.6";
}


// overridden virtual function
bool Ampache::hasUI()
{
    return true;
}


// overridden virtual function
void Ampache::setUserAgent(QString userAgent)
{
    this->userAgent = userAgent;
}


// overridden virtual function
QUuid Ampache::persistentUniqueId()
{
    return id;
}


// thread entry
void Ampache::run()
{
    qsrand(QDateTime::currentDateTime().toTime_t());

    networkAccessManager = new QNetworkAccessManager();
    connect(networkAccessManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(networkFinished(QNetworkReply *)));

    emit loadGlobalConfiguration(id);
}


// slot receiving configuration
void Ampache::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfigGlobal(configuration);

    emit loadConfiguration(id);
}


// slot receiving configuration
void Ampache::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfig(configuration);

    handshake();
}


// configuration
void Ampache::sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId)
    Q_UNUSED(temporary)
    Q_UNUSED(clientIdentifier)
    Q_UNUSED(clientSqlIdentifier)
    Q_UNUSED(results)
}


// temporary storage
void Ampache::globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId)
    Q_UNUSED(temporary)
    Q_UNUSED(clientIdentifier)
    Q_UNUSED(clientSqlIdentifier)
    Q_UNUSED(results)
}


// configuration
void Ampache::sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
{
    Q_UNUSED(persistentUniqueId)
    Q_UNUSED(temporary)
    Q_UNUSED(clientIdentifier)
    Q_UNUSED(clientSqlIdentifier)
    Q_UNUSED(error)
}


// message handler
void Ampache::messageFromPlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value)
{
    Q_UNUSED(uniqueId)
    Q_UNUSED(sourceUniqueId)
    Q_UNUSED(messageId)
    Q_UNUSED(value)
}


// client wants to display this plugin's configuration dialog
void Ampache::getUiQml(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    QFile settingsFile("://AASettings.qml");
    settingsFile.open(QFile::ReadOnly);
    QString settings = settingsFile.readAll();
    settingsFile.close();

    if (serverUrl.isValid()) {
        settings.replace("<https://server.com>", serverUrl.toString());
    }
    if (!serverUser.isEmpty()) {
        settings.replace("<username>", serverUser);
    }
    if (!serverPassword.isEmpty()) {
        settings.replace("<password>", serverPassword);
    }

    settings.replace("9999", QString("%1").arg(variationSettingId()));

    emit uiQml(id, settings);
}


// slot receiving configuration dialog results
void Ampache::uiResults(QUuid uniqueId, QJsonDocument results)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfigGlobal(results);
    jsonToConfig(results);

    emit saveGlobalConfiguration(id, configToJsonGlobal());
    emit saveConfiguration(id, configToJson());
    emit requestRemoveTracks(id);
}


// client wants to receive updates of this plugin's diagnostic information
void Ampache::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = true;
    sendDiagnosticsData();
}


// client doesnt want to receive updates of this plugin's diagnostic information anymore
void Ampache::stopDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = false;
}


// server says unable to start a track
void Ampache::unableToStart(QUuid uniqueId, QUrl url)
{
    if (uniqueId != id) {
        return;
    }
}


// server says track stopped playing too early
void Ampache::castFinishedEarly(QUuid uniqueId, QUrl url, int playedSeconds)
{
    if (uniqueId != id) {
        return;
    }

    if (playedSeconds < 30) {
        unableToStart(id, url);
    }
}


// server is done with a track
void Ampache::done(QUuid uniqueId, QUrl url, bool wasError)
{
    Q_UNUSED(uniqueId)
    Q_UNUSED(url)
    Q_UNUSED(wasError)
}

// reuest for playlist entries
void Ampache::getPlaylist(QUuid uniqueId, int trackCount, int mode)
{
    if (uniqueId != id) {
        return;
    }

    playlistRequests.append({ trackCount, mode });
    playlisting();
}


// get replacement for a track that could not start or ended prematurely
void Ampache::getReplacement(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    getPlaylist(id, 1, PLAYLIST_MODE_NORMAL);
}


// client wants to dispaly open dialog
void Ampache::getOpenTracks(QUuid uniqueId, QString parentId)
{
    if (uniqueId != id) {
        return;
    }

    QUrlQuery query;
    query.addQueryItem("auth", authKey);
    query.addQueryItem("limit", "none");

    // top level
    if (parentId.length() == 0) {
        OpenTracks openTracks;

        OpenTrack  browseTrack;
        browseTrack.id = "99B";
        browseTrack.label = "Browse";
        browseTrack.selectable = false;
        browseTrack.hasChildren = true;

        OpenTrack  playlistsTrack;
        playlistsTrack.id = "99P";
        playlistsTrack.label = "Playlists";
        playlistsTrack.selectable = false;
        playlistsTrack.hasChildren = true;

        openTracks.append(browseTrack);
        openTracks.append(playlistsTrack);

        emit openTracksResults(id, openTracks);
        return;
    }

    // browse top level
    else if (parentId.compare("99B") == 0) {
        query.addQueryItem("action", "artists");
        setState(OpeningArtistList);
    }

    // playlists top level
    else if (parentId.compare("99P") == 0) {
        query.addQueryItem("action", "get_indexes");
        query.addQueryItem("type", "playlist");
        setState(OpeningPlaylistList);
    }

    // everything else
    else {
        QStringList parentIdParts = parentId.split("~");
        QString     parentType    = parentIdParts.at(0);

        parentIdParts.removeFirst();
        parentId = parentIdParts.join("");

        if (parentType.compare("A") == 0) {
            query.addQueryItem("action", "artist_albums");
            query.addQueryItem("filter", parentId);
            setState(OpeningAlbumList);
        }
        else if (parentType.compare("L") == 0) {
            query.addQueryItem("action", "album_songs");
            query.addQueryItem("filter", parentId);
            setState(OpeningSongList);
        }
        else if (parentType.compare("P") == 0) {
            query.addQueryItem("action", "playlist_songs");
            query.addQueryItem("filter", parentId);
            setState(OpeningSongList);
        }
    }

    QNetworkRequest request = buildRequest(query);

    networkAccessManager->get(request);
}


// turn open dialog selections to playlist entries
void Ampache::resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks)
{
    if (uniqueId != id) {
        return;
    }

    if (selectedTracks.count() < 1) {
        return;
    }

    resolveIds.append(selectedTracks);
    resolve();
}


// request for playlist entries based on search criteria
void Ampache::search(QUuid uniqueId, QString criteria)
{
    if (uniqueId != id) {
        return;
    }

    QUrlQuery query;
    query.addQueryItem("auth", authKey);
    query.addQueryItem("limit", "none");
    query.addQueryItem("action", "songs");
    query.addQueryItem("filter", criteria);

    QNetworkRequest request = buildRequest(query);

    setState(Searching);
    networkAccessManager->get(request);
}


// user clicked action that was included in track info
void Ampache::action(QUuid uniqueId, int actionKey, TrackInfo trackInfo)
{
    if (uniqueId != id) {
        return;
    }

    if (actionKey == 1) {
        if (urlsToServerId.contains(trackInfo.url)) {
            updateFlag(urlsToServerId.value(trackInfo.url), true);

            TrackInfo trackInfoTemp;
            trackInfoTemp.track = 0;
            trackInfoTemp.year  = 0;
            trackInfoTemp.url   = trackInfo.url;
            trackInfoTemp.actions.append({ id, 2, "Unlove" });
            trackInfoTemp.actions.append({ id, 10, "Lyrics search"});
            trackInfoTemp.actions.append({ id, 11, "Band search"});
            emit updateTrackInfo(id, trackInfoTemp);
        }
    }

    if (actionKey == 2) {
        if (urlsToServerId.contains(trackInfo.url)) {
            updateFlag(urlsToServerId.value(trackInfo.url), false);

            TrackInfo trackInfoTemp;
            trackInfoTemp.track = 0;
            trackInfoTemp.year  = 0;
            trackInfoTemp.url   = trackInfo.url;
            trackInfoTemp.actions.append({ id, 1, "Love" });
            trackInfoTemp.actions.append({ id, 10, "Lyrics search"});
            trackInfoTemp.actions.append({ id, 11, "Band search"});
            emit updateTrackInfo(id, trackInfoTemp);
        }
    }

    if (actionKey == 10) {
        emit openUrl(QUrl(QString("http://google.com/search?q=%1 %2 lyrics").arg(trackInfo.performer).arg(trackInfo.title)));
    }

    if (actionKey == 11) {
        emit openUrl(QUrl(QString("http://google.com/search?q=\"%1\" band").arg(trackInfo.performer)));
    }

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// configuration conversion
QJsonDocument Ampache::configToJson()
{
    QJsonObject jsonObject;

    jsonObject.insert("variation", variationSetting);

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
QJsonDocument Ampache::configToJsonGlobal()
{
    QJsonObject jsonObject;

    jsonObject.insert("url", serverUrl.toString());
    jsonObject.insert("user", serverUser);
    jsonObject.insert("psw", serverPassword);

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
void Ampache::jsonToConfig(QJsonDocument jsonDocument)
{
    if (jsonDocument.object().contains("variation")) {
        variationSetting = jsonDocument.object().value("variation").toString();
        variationSetCountSinceLow  = (variationSettingId() == 3 ? 3 : 0);
        variationSetCountSinceHigh = 0;
    }
}


// configuration conversion
void Ampache::jsonToConfigGlobal(QJsonDocument jsonDocument)
{
    if (jsonDocument.object().contains("url")) {
        serverUrl.setUrl(jsonDocument.object().value("url").toString());
    }
    if (jsonDocument.object().contains("user")) {
        serverUser = jsonDocument.object().value("user").toString();
    }
    if (jsonDocument.object().contains("psw")) {
        serverPassword = jsonDocument.object().value("psw").toString();
    }
}


void Ampache::handshake()
{
    if (readySent) {
        readySent = false;
        emit unready(id);
    }

    authKey = "";

    QString    timeStr  = QString("%1").arg(QDateTime::currentMSecsSinceEpoch() / 1000);
    QByteArray pswHash  = QCryptographicHash::hash(serverPassword.toLatin1(), QCryptographicHash::Sha256);
    QByteArray authHash = QCryptographicHash::hash(QString(timeStr + QString(pswHash.toHex())).toLatin1(), QCryptographicHash::Sha256);

    QUrlQuery query;
    query.addQueryItem("action", "handshake");
    query.addQueryItem("auth", authHash.toHex());
    query.addQueryItem("timestamp", timeStr);
    query.addQueryItem("version", QString("%1").arg(SERVER_API_VERSION_MIN));
    query.addQueryItem("user", serverUser);

    QNetworkRequest request = buildRequest(query);

    setState(Handshake);
    networkAccessManager->get(request);
}


void Ampache::playlisting()
{
    if (playlistScheduled || !playlistRequests.count()) {
        return;
    }

    playlistScheduled = true;
    QTimer::singleShot(NET_DELAY, this, SLOT(playlistNext()));
}


void Ampache::playlistNext()
{
    PlaylistRequest playlistRequest = playlistRequests.first();
    playlistRequests.removeFirst();

    QUrlQuery query;
    query.addQueryItem("auth", authKey);
    query.addQueryItem("action", "playlist_generate");

    if (playlistRequest.mode == PLAYLIST_MODE_NORMAL) {
        if (variationRemaining == 0) {
            variationSetCurrentRemainingAlbum();
        }

        if (variationCurrent < 2) {
            if (variationAlbum == 0) {
                query.addQueryItem("limit", "1");
                playlistRequestsSent.append({ 1, PLAYLIST_MODE_VARIATION_FIRST });

                if (playlistRequest.trackCount > 1) {
                    playlistRequests.prepend({ playlistRequest.trackCount - 1, PLAYLIST_MODE_NORMAL });
                }
            }
            else {
                int thisRequestCount = qMin(playlistRequest.trackCount, variationRemaining);
                int nextRequestCount = qMax(playlistRequest.trackCount - variationRemaining, 0);

                query.addQueryItem("limit", QString("%1").arg(thisRequestCount));
                query.addQueryItem("album", QString("%1").arg(variationAlbum));
                playlistRequestsSent.append({ thisRequestCount, playlistRequest.mode });

                if (nextRequestCount) {
                    playlistRequests.prepend({ nextRequestCount, PLAYLIST_MODE_NORMAL });
                }
            }
        }
        else {
            query.addQueryItem("mode", "forgotten");
            query.addQueryItem("limit", QString("%1").arg(playlistRequest.trackCount));
            playlistRequestsSent.append({ playlistRequest.trackCount, playlistRequest.mode });
        }
    }
    else {
        query.addQueryItem("limit", "1");
        if (playlistRequest.mode == PLAYLIST_MODE_LOVED_SIMILAR_RESOLVE) {
            query.addQueryItem("album", QString("%1").arg(lastReturnedAlbumId));
        }
        else {
            query.addQueryItem("flag", "1");
        }
        playlistRequestsSent.append({ 1, playlistRequest.mode });

        if (playlistRequest.trackCount > 1) {
            playlistRequests.prepend({ playlistRequest.trackCount - 1, PLAYLIST_MODE_NORMAL });
        }
    }

    QNetworkRequest request = buildRequest(query);
    setState(Playlist);
    networkAccessManager->get(request);
}


void Ampache::resolve()
{
    if (resolveScheduled || !resolveIds.count()) {
        return;
    }

    resolveScheduled = true;
    QTimer::singleShot(NET_DELAY, this, SLOT(resolveNext()));
}


void Ampache::resolveNext()
{
    QUrlQuery query;
    query.addQueryItem("auth", authKey);
    query.addQueryItem("limit", "none");

    QString resolveId = resolveIds.first();
    resolveIds.removeFirst();

    if (resolveId.startsWith("P~")) {
        query.addQueryItem("action", "playlist_songs");
        resolveId.replace("P~", "");
    }
    else {
        query.addQueryItem("action", "song");
    }
    query.addQueryItem("filter", resolveId);

    QNetworkRequest request = buildRequest(query);

    setState(Resolving);
    networkAccessManager->get(request);
}


void Ampache::updateFlag(int songId, bool flag)
{
    QUrlQuery query;
    query.addQueryItem("auth", authKey);
    query.addQueryItem("action", "flag");
    query.addQueryItem("type", "song");
    query.addQueryItem("id", QString("%1").arg(songId));
    query.addQueryItem("flag", flag ? "1" : "0");

    QNetworkRequest request = buildRequest(query);

    setState(Flagging);
    networkAccessManager->get(request);
}


void Ampache::networkFinished(QNetworkReply *reply)
{
    // make sure reply OK
    if (reply->error() != QNetworkReply::NoError) {
        emit infoMessage(id, QString("Network error: %1").arg(reply->errorString()));
        reply->deleteLater();
        setState(Idle);
        return;
    }
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 200) {
        emit infoMessage(id, QString("Ampache server returned status %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString()));
        reply->deleteLater();
        setState(Idle);
        return;
    }

    // TODO reconnect if session timed out, check https://github.com/ampache/ampache/wiki/XML-API-ERRORS

    QString currentElement = "";
    int     errorCode      = 0;

    qint32     trackServerId;
    TracksInfo tracksInfo;
    TrackInfo  trackInfo;

    OpenTracks openTracks;
    OpenTrack  openTrack;

    PlaylistRequest playlistRequest;
    if (state == Playlist) {
        playlistRequest = playlistRequestsSent.first();
        playlistRequestsSent.removeFirst();
    }

    QXmlStreamReader xmlStreamReader(reply);
    while (!xmlStreamReader.atEnd()) {
        QXmlStreamReader::TokenType tokenType = xmlStreamReader.readNext();

        if (tokenType == QXmlStreamReader::StartElement) {
            currentElement = xmlStreamReader.name().toString();

            if (currentElement.compare("error") == 0) {
                QXmlStreamAttributes attributes = xmlStreamReader.attributes();
                if (attributes.hasAttribute("code")) {
                    errorCode = attributes.value("code").toInt();
                }
            }
        }
        if (tokenType == QXmlStreamReader::EndElement) {
            currentElement = "";
        }

        if (tokenType == QXmlStreamReader::Characters) {
            if (currentElement.compare("error") == 0) {
                emit infoMessage(id, QString("Error %1: %2").arg(errorCode).arg(xmlStreamReader.text().toString()));
            }
        }

        if (state == Handshake) {
            if (tokenType == QXmlStreamReader::Characters) {
                if (currentElement.compare("auth") == 0) {
                    authKey = xmlStreamReader.text().toString();
                }
                if (currentElement.compare("api") == 0) {
                    serverApiVersion = xmlStreamReader.text().toLong();
                }
            }
        }

        if ((state == Playlist) || (state == Resolving)) {
            if (tokenType == QXmlStreamReader::StartElement) {
                QXmlStreamAttributes attributes = xmlStreamReader.attributes();

                if (currentElement.compare("song") == 0) {
                    trackInfo.title     = "";
                    trackInfo.album     = "";
                    trackInfo.performer = "";
                    trackInfo.track     = 0;
                    trackInfo.year      = 0;
                    trackInfo.cast      = false;
                    trackInfo.actions.clear();
                    trackInfo.pictures.clear();
                    trackInfo.url.clear();

                    if (attributes.hasAttribute("id")) {
                        trackServerId = attributes.value("id").toInt();
                    }
                }

                if ((currentElement.compare("album") == 0) && (attributes.hasAttribute("id"))) {
                    lastReturnedAlbumId = attributes.value("id").toInt();
                }
            }
            if (tokenType == QXmlStreamReader::Characters) {
                if (currentElement.compare("title") == 0) {
                    trackInfo.title = xmlStreamReader.text().toString();
                }
                if (currentElement.compare("album") == 0) {
                    trackInfo.album = xmlStreamReader.text().toString();
                }
                if (currentElement.compare("artist") == 0) {
                    trackInfo.performer = xmlStreamReader.text().toString();
                }
                if (currentElement.compare("track") == 0) {
                    trackInfo.track = xmlStreamReader.text().toInt();
                }
                if (currentElement.compare("year") == 0) {
                    trackInfo.year = xmlStreamReader.text().toInt();
                }
                if (currentElement.compare("art") == 0) {
                    trackInfo.pictures.append(QUrl(xmlStreamReader.text().toString()));
                }
                if (currentElement.compare("url") == 0) {
                    trackInfo.url = QUrl(xmlStreamReader.text().toString());
                    urlsToServerId.insert(trackInfo.url, trackServerId);
                }
                if (currentElement.compare("flag") == 0) {
                    if (xmlStreamReader.text().toInt()) {
                        trackInfo.actions.append({ id, 2, "Unlove" });
                    }
                    else {
                        trackInfo.actions.append({ id, 1, "Love" });
                    }
                    trackInfo.actions.append({ id, 10, "Lyrics search"});
                    trackInfo.actions.append({ id, 11, "Band search"});
                }
            }
            if (tokenType == QXmlStreamReader::EndElement) {
                if (xmlStreamReader.name().toString().compare("song") == 0) {
                    if (playlistRequest.mode != PLAYLIST_MODE_LOVED_SIMILAR) {
                        tracksInfo.append(trackInfo);
                    }
                }
            }
        }

        if (state == OpeningArtistList) {
            if (tokenType == QXmlStreamReader::StartElement) {
                if (currentElement.compare("artist") == 0) {
                    QXmlStreamAttributes attributes = xmlStreamReader.attributes();

                    if (attributes.hasAttribute("id")) {
                        openTrack.id         = QString("A~%1").arg(attributes.value("id").toString());
                        openTrack.label      = "";
                        openTrack.selectable = false;
                        openTrack.hasChildren = true;
                    }
                }
            }
            if (tokenType == QXmlStreamReader::Characters) {
                if (currentElement.compare("name") == 0) {
                    openTrack.label = xmlStreamReader.text().toString();
                }
            }
            if (tokenType == QXmlStreamReader::EndElement) {
                if ((xmlStreamReader.name().toString().compare("artist") == 0) && (openTrack.id.length())) {
                    openTracks.append(openTrack);
                }
            }
        }

        if (state == OpeningAlbumList) {
            if (tokenType == QXmlStreamReader::StartElement) {
                if (currentElement.compare("album") == 0) {
                    QXmlStreamAttributes attributes = xmlStreamReader.attributes();
                    if (attributes.hasAttribute("id")) {
                        openTrack.id          = QString("L~%1").arg(attributes.value("id").toString());
                        openTrack.label       = "";
                        openTrack.selectable  = false;
                        openTrack.hasChildren = true;
                    }
                }
            }
            if (tokenType == QXmlStreamReader::Characters) {
                if (currentElement.compare("name") == 0) {
                    openTrack.label = xmlStreamReader.text().toString();
                }
            }
            if (tokenType == QXmlStreamReader::EndElement) {
                if ((xmlStreamReader.name().toString().compare("album") == 0) && (openTrack.id.length())) {
                    openTracks.append(openTrack);
                }
            }
        }

        if (state == OpeningPlaylistList) {
            if (tokenType == QXmlStreamReader::StartElement) {
                if (currentElement.compare("playlist") == 0) {
                    QXmlStreamAttributes attributes = xmlStreamReader.attributes();
                    if (attributes.hasAttribute("id")) {
                        openTrack.id          = QString("P~%1").arg(attributes.value("id").toString());
                        openTrack.label       = "";
                        openTrack.selectable  = true;
                        openTrack.hasChildren = true;
                    }
                }
            }
            if (tokenType == QXmlStreamReader::Characters) {
                if (currentElement.compare("name") == 0) {
                    openTrack.label = xmlStreamReader.text().toString();
                }
            }
            if (tokenType == QXmlStreamReader::EndElement) {
                if ((xmlStreamReader.name().toString().compare("playlist") == 0) && (openTrack.id.length())) {
                    openTracks.append(openTrack);
                }
            }
        }

        if ((state == OpeningSongList) || (state == Searching)) {
            if (tokenType == QXmlStreamReader::StartElement) {
                if (currentElement.compare("song") == 0) {
                    QXmlStreamAttributes attributes = xmlStreamReader.attributes();
                    if (attributes.hasAttribute("id")) {
                        openTrack.id          = QString("%1").arg(attributes.value("id").toString());
                        openTrack.label       = "";
                        openTrack.selectable  = true;
                        openTrack.hasChildren = false;
                    }
                }
            }
            if (tokenType == QXmlStreamReader::Characters) {
                if (currentElement.compare("title") == 0) {
                    openTrack.label = xmlStreamReader.text().toString();
                }

                if ((currentElement.compare("artist") == 0) && (state == Searching)) {
                    openTrack.label.append(" - " + xmlStreamReader.text().toString());
                }
            }
            if (tokenType == QXmlStreamReader::EndElement) {
                if ((xmlStreamReader.name().toString().compare("song") == 0) && (openTrack.id.length())) {
                    openTracks.append(openTrack);
                }
            }
        }
    }

    if (xmlStreamReader.hasError()) {
        emit infoMessage(id, "Error while parsing XML response from Ampache server");
        reply->deleteLater();
        setState(Idle);
        return;
    }

    if (state == Handshake) {
        if (serverApiVersion < SERVER_API_VERSION_MIN) {
            emit infoMessage(id, QString("Server API version %1 does not satisfy minimum requirement %2").arg(serverApiVersion).arg(SERVER_API_VERSION_MIN));
        }
    }
    else if (state == Playlist) {
        playlistTracksInfo.append(tracksInfo);

        if (variationRemaining > 0) {
            variationRemaining = qMax(variationRemaining - tracksInfo.count(), 0);
        }

        if (playlistRequest.mode == PLAYLIST_MODE_LOVED) {
            QVariantHash temp = playlistExtraInfo.value(trackInfo.url);
            temp.insert("loved", PLAYLIST_MODE_LOVED);
            playlistExtraInfo.insert(trackInfo.url, temp);
        }
        else if (playlistRequest.mode == PLAYLIST_MODE_LOVED_SIMILAR) {
            playlistRequests.prepend({ 1, PLAYLIST_MODE_LOVED_SIMILAR_RESOLVE });
        }
        else if (playlistRequest.mode == PLAYLIST_MODE_LOVED_SIMILAR_RESOLVE) {
            QVariantHash temp = playlistExtraInfo.value(trackInfo.url);
            temp.insert("loved", PLAYLIST_MODE_LOVED_SIMILAR);
            playlistExtraInfo.insert(trackInfo.url, temp);
        }
        else if (playlistRequest.mode == PLAYLIST_MODE_VARIATION_FIRST) {
            variationAlbum = lastReturnedAlbumId;
        }

        if (playlistRequests.count()) {
            QTimer::singleShot(NET_DELAY, this, SLOT(playlistNext()));
        }
        else {
            if (variationCurrent != 0) {
                variationRemaining = 0;
            }

            playlistScheduled = false;
            emit playlist(id, playlistTracksInfo, playlistExtraInfo);
            playlistTracksInfo.clear();
            playlistExtraInfo.clear();
        }
    }
    else if (state == Resolving) {
        resolveTracksInfo.append(tracksInfo);

        if (resolveIds.count()) {
            QTimer::singleShot(NET_DELAY, this, SLOT(resolveNext()));
        }
        else {
            resolveScheduled = false;
            ExtraInfo  extraInfo;
            foreach (TrackInfo trackInfo, resolveTracksInfo) {
                QVariantHash temp = extraInfo.value(trackInfo.url);
                temp.insert("resolved_open_track", 1);
                extraInfo.insert(trackInfo.url, temp);
            }
            emit playlist(id, resolveTracksInfo, extraInfo);
            resolveTracksInfo.clear();
        }
    }
    else if ((state == OpeningArtistList) || (state == OpeningAlbumList) || (state == OpeningSongList) || (state == OpeningPlaylistList)) {
        emit openTracksResults(id, openTracks);
    }
    else if (state == Searching) {
        emit searchResults(id, openTracks);
    }

    reply->deleteLater();
    setState(Idle);

    if (!readySent && !authKey.isEmpty() && (serverApiVersion >= SERVER_API_VERSION_MIN)) {
        emit ready(id);
        readySent = true;
    }
}


// helper
QNetworkRequest Ampache::buildRequest(QUrlQuery query)
{
    QUrl url(serverUrl.toString());
    url.setPath("/server/xml.server.php");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", userAgent.toUtf8());

    return request;
}


// helper
int Ampache::variationSettingId()
{
    QStringList variations({ "Low", "Medium", "High", "Random" });
    return variations.indexOf(variationSetting);
}


// helper
void Ampache::variationSetCurrentRemainingAlbum()
{
    if (variationSettingId() == 3) {
        if (variationSetCountSinceHigh >= 4) {
            variationCurrent = 2;
        }
        else if (variationSetCountSinceLow >= 4) {
            variationCurrent = qrand() % 3;
        }
        else {
            variationCurrent = (qrand() % 2) + 1;
        }
    }
    else {
        variationCurrent = variationSettingId();
    }

    switch (variationCurrent) {
        case 0:
            variationRemaining = (qrand() % 3) + 4;
            variationSetCountSinceHigh++;
            variationSetCountSinceLow = 0;
            break;
        case 1:
            variationRemaining = (qrand() % 2) + 2;
            variationSetCountSinceHigh++;
            variationSetCountSinceLow++;
            break;
        case 2:
            variationRemaining = -1;
            variationSetCountSinceHigh = 0;
            variationSetCountSinceLow++;
    }

    variationAlbum = 0;
}

// helper
void Ampache::setState(State state)
{
    this->state = state;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// helper
void Ampache::sendDiagnosticsData()
{
    DiagnosticData diagnosticData;

    switch (state) {
        case Idle:
            diagnosticData.append({ "Status", "Idle"});
            break;
        case Handshake:
            diagnosticData.append({ "Status", "Performing handshake" });
            break;
        case Playlist:
            diagnosticData.append({ "Status", "Getting playlist" });
            break;
        case Replacement:
            diagnosticData.append({ "Status", "Getting replacement" });
            break;
        case OpeningArtistList:
            diagnosticData.append({ "Status", "Getting performer's list" });
            break;
        case OpeningAlbumList:
            diagnosticData.append({ "Status", "Getting album list" });
            break;
        case OpeningSongList:
            diagnosticData.append({ "Status", "Getting track list" });
            break;
        case Searching:
            diagnosticData.append({ "Status", "Searching" });
            break;
        case Resolving:
            diagnosticData.append({ "Status", "Resolving tracks" });
            break;
        case Flagging:
            diagnosticData.append({ "Status", "Updati8ng flag" });
            break;
    }

    emit diagnostics(id, diagnosticData);
}
