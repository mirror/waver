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
    readySent             = false;
    sendDiagnostics       = false;
    nextIndex             = 0;

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
    return "Waver's Ampache Plugin";
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
void Ampache::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfig(configuration);

    handshake();
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

    emit uiQml(id, settings);

}


// slot receiving configuration dialog results
void Ampache::uiResults(QUuid uniqueId, QJsonDocument results)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfigGlobal(results);
    //jsonToConfig(results);

    emit saveGlobalConfiguration(id, configToJsonGlobal());
    //emit saveConfiguration(id, configToJson());
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

    QUrlQuery query;
    query.addQueryItem("action", "songs");
    query.addQueryItem("auth", authKey);
    query.addQueryItem("limit", QString("%1").arg(trackCount));
    query.addQueryItem("offset", QString("%1").arg(nextIndex + 1));

    QNetworkRequest request = buildRequest(query);

    setState(Playlist);
    networkAccessManager->get(request);

    return;
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

    OpenTracks openTracks;
    /*
        // top level
        if (parentId == 0) {
            foreach (QString genre, selectedGenres) {
                OpenTrack openTrack;
                openTrack.hasChildren = true;
                openTrack.id          = genre;
                openTrack.label       = genre;
                openTrack.selectable = false;
                openTracks.append(openTrack);
            }

            qSort(openTracks.begin(), openTracks.end(), [](OpenTrack a, OpenTrack b) {
                return (a.label.compare(b.label) < 0);
            });

            emit openTracksResults(id, openTracks);

            return;
        }

        // genre level, see if it was already queried
        if (stationsLoaded.contains(parentId)) {
            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_OPEN_GENRE_STATIONS, "SELECT id, name FROM stations WHERE genre = ?", QVariantList({ parentId }));
            return;
        }

        // must query stations
        genreSearchItems.append({ parentId, Opening });
        genreSearch();
    */
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
    /*
        QStringList  openBinds;
        QVariantList openValues;
        QStringList  searchBinds;
        QVariantList searchValues;
        foreach (QString selectedTrack, selectedTracks) {
            if (selectedTrack.startsWith("SEARCH_")) {
                searchBinds.append("?");
                searchValues.append(selectedTrack.replace("SEARCH_", ""));
                continue;
            }
            openBinds.append("?");
            openValues.append(selectedTrack);
        }

        if (openValues.count() > 0) {
            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_OPEN_PLAYLIST, "SELECT id, base, name, genre, url, logo FROM stations WHERE id IN (" + openBinds.join(",") + ")", openValues);
        }
        if (searchValues.count() > 0) {
            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_STATION_SEARCH_PLAYLIST, "SELECT id, base, name, genre, url FROM search WHERE id IN (" + searchBinds.join(",") + ")", searchValues);
        }
    */
}


// request for playlist entries based on search criteria
void Ampache::search(QUuid uniqueId, QString criteria)
{
    if (uniqueId != id) {
        return;
    }

    /*
        stationSearchCriteria = criteria.toLower();

        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_STATION_SEARCH_STATIONS, "SELECT * FROM search WHERE criteria = ?", QVariantList({ stationSearchCriteria }));
    */
}


// user clicked action that was included in track info
void Ampache::action(QUuid uniqueId, int actionKey, TrackInfo trackInfo)
{
    if (uniqueId != id) {
        return;
    }

    if (actionKey == 0) {

    }

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// configuration conversion
QJsonDocument Ampache::configToJson()
{
    QJsonObject jsonObject;

    jsonObject.insert("nextIndex", nextIndex);

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
    Q_UNUSED(jsonDocument)

    if (jsonDocument.object().contains("nextIndex")) {
        nextIndex = jsonDocument.object().value("nextIndex").toInt();
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
    query.addQueryItem("version", "400001");
    query.addQueryItem("user", serverUser);

    QNetworkRequest request = buildRequest(query);

    setState(Handshake);
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

    bool    inAuth         = false;
    QString currentElement = "";

    TracksInfo tracksInfo;
    TrackInfo  trackInfo;
    ExtraInfo  extraInfo;

    QXmlStreamReader xmlStreamReader(reply);
    while (!xmlStreamReader.atEnd()) {
        QXmlStreamReader::TokenType tokenType = xmlStreamReader.readNext();

        if (state == Handshake) {
            if (tokenType == QXmlStreamReader::StartElement) {
                if (xmlStreamReader.name().toString().compare("auth") == 0) {
                    inAuth = true;
                }
            }
            if (tokenType == QXmlStreamReader::Characters) {
                if (inAuth) {
                    authKey = xmlStreamReader.text().toString();
                }
            }
            if (tokenType == QXmlStreamReader::EndElement) {
                inAuth = false;
            }
        }

        if (state == Playlist) {
            if (tokenType == QXmlStreamReader::StartElement) {
                currentElement = xmlStreamReader.name().toString();

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
                }
            }
            if (tokenType == QXmlStreamReader::EndElement) {
                currentElement = "";

                if (xmlStreamReader.name().toString().compare("song") == 0) {
                    tracksInfo.append(trackInfo);
                    nextIndex++;
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

    if (state == Playlist) {
        emit saveConfiguration(id, configToJson());
        emit playlist(id, tracksInfo, extraInfo);
    }

    reply->deleteLater();
    setState(Idle);

    if (!readySent && !authKey.isEmpty()) {
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

    if (url.scheme().compare("https") == 0) {
        // TODO!!! This is temporary for testing purposes. Disables all SLL certificate checking. To be replaced with user-defined whitelist.
        QSslConfiguration sslConfiguration = request.sslConfiguration();
        sslConfiguration.setPeerVerifyMode(QSslSocket::VerifyNone);
        request.setSslConfiguration(sslConfiguration);
    }

    return request;
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

}
