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


#include "fmasource.h"


// plugin factory
void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal)
{
    if (pluginTypesMask & PLUGIN_TYPE_SOURCE) {
        retVal->append((QObject *) new FMASource());
    }
}


// constructor
FMASource::FMASource()
{
    id  = QUuid("{A41DEA06-4B1E-4C3F-8108-76EB29556089}");
    key = getKey();

    networkAccessManager = NULL;
    readySent            = false;
    sendDiagnostics      = false;
    state                = Idle;
}


// destructor
FMASource::~FMASource()
{
    if (networkAccessManager != NULL) {
        networkAccessManager->deleteLater();
    }
}


// overridden virtual function
int FMASource::pluginType()
{
    return PLUGIN_TYPE_SOURCE;
}


// overridden virtual function
QString FMASource::pluginName()
{
    return "Free Music Archive";
}


// overridden virtual function
int FMASource::pluginVersion()
{
    return 1;
}


// overrided virtual function
QString FMASource::waverVersionAPICompatibility()
{
    return "0.0.4";
}


// overridden virtual function
bool FMASource::hasUI()
{
    return true;
}


// overridden virtual function
void FMASource::setUserAgent(QString userAgent)
{
    this->userAgent = userAgent;
}

// overridden virtual function
QUuid FMASource::persistentUniqueId()
{
    return id;
}


// thread entry
void FMASource::run()
{
    qsrand(QDateTime::currentDateTime().toTime_t());

    networkAccessManager = new QNetworkAccessManager();
    connect(networkAccessManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(networkFinished(QNetworkReply *)));

    emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_CREATETABLE_ALBUMS, "CREATE TABLE IF NOT EXISTS albums (id INT UNIQUE, genre_id INT, album, performer, year INT)", QVariantList());
}


// slot receiving configuration
void FMASource::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfig(configuration);

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    if (selectedGenres.count() < 1) {
        readySent = false;
        emit unready(id);
        return;
    }

    // start the "load more data" sequence
    QString      binds;
    QVariantList values;
    selectedGenresBinds(&binds, &values);
    emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_LOADMORE_ALBUMGENRES, "SELECT genre_id FROM albums WHERE genre_id IN (" + binds + ") GROUP BY genre_id", values);
}


// slot receiving configuration
void FMASource::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfigGlobal(configuration);
    //removeExpiredUnableToStartUrls();

    if (genresLoaded.daysTo(QDateTime::currentDateTime()) > GENRES_EXPIRY_DAYS) {
        genres.clear();
    }

    if (genres.count() < 1) {
        readySent = false;
        emit unready(id);

        setState(GenreList);

        QNetworkRequest request(QUrl("https://freemusicarchive.org/api/get/genres.xml?api_key=" + key + "&limit=50"));
        request.setRawHeader("User-Agent", userAgent.toUtf8());
        networkAccessManager->get(request);

        return;
    }

    emit loadConfiguration(id);
}


// configuration
void FMASource::sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void FMASource::globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);

    if (persistentUniqueId != id) {
        return;
    }

    if (clientSqlIdentifier == SQL_CREATETABLE_ALBUMS) {
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_CREATETABLE_TRACKS, "CREATE TABLE IF NOT EXISTS tracks (id INT UNIQUE, album_id INT, title, url, picture_url, track INT, banned INT DEFAULT 0, unable_to_start INT DEFAULT 0, playcount INT DEFAULT 0)", QVariantList());
        return;
    }

    if (clientSqlIdentifier == SQL_CREATETABLE_TRACKS) {
        emit loadGlobalConfiguration(id);
        return;
    }

    if (clientSqlIdentifier == SQL_LOADMORE_ALBUMGENRES) {
        QVector<int> chooseGenre;
        foreach (int selectedGenre, selectedGenres) {
            bool choose = true;
            foreach (QVariantHash result, results) {
                if (result.value("genre_id").toInt() == selectedGenre) {
                    choose = false;
                    break;
                }
            }
            if (choose) {
                chooseGenre.append(selectedGenre);
            }
        }
        if (chooseGenre.count() > 0) {
            genreSearchItems.append({ chooseGenre.at(qrand() % chooseGenre.count()), AlbumList });
            genreSearch();
        }
        else {
            QString      binds;
            QVariantList values;
            selectedGenresBinds(&binds, &values);
            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_LOADMORE_ALBUMSWITHOUTTRACKS, "SELECT id FROM albums WHERE (genre_id IN (" + binds + ")) AND (id NOT IN (SELECT album_id FROM tracks GROUP BY album_id)) ORDER BY RANDOM() LIMIT 3", values);
        }
        return;
    }

    if (clientSqlIdentifier == SQL_LOADMORE_ALBUMSLOADED) {
        QString      binds;
        QVariantList values;
        selectedGenresBinds(&binds, &values);
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_LOADMORE_ALBUMSWITHOUTTRACKS, "SELECT id FROM albums WHERE (genre_id IN (" + binds + ")) AND (id NOT IN (SELECT album_id FROM tracks GROUP BY album_id)) ORDER BY RANDOM() LIMIT 3", values);
        return;
    }

    if (clientSqlIdentifier == SQL_LOADMORE_ALBUMSWITHOUTTRACKS) {
        if (results.count() < 1) {
            if (!readySent) {
                readySent = true;
                emit ready(id);
            }
            return;
        }
        foreach (QVariantHash result, results) {
            albumSearchItems.append({ result.value("id").toInt(), TrackList });
        }
        albumSearch();
        return;
    }

    if (clientSqlIdentifier == SQL_LOADMORE_TRACKSLOADED) {
        if (!readySent) {
            readySent = true;
            emit ready(id);
        }
        return;
    }

    if (clientSqlIdentifier == SQL_GET_PLAYLIST) {
        TracksInfo tracksInfo;
        foreach (QVariantHash result, results) {
            TrackInfo trackInfo;
            trackInfo.album     = result.value("album").toString();
            trackInfo.cast      = false;
            trackInfo.performer = result.value("performer").toString();
            trackInfo.title     = result.value("title").toString();
            trackInfo.track     = result.value("track").toInt();
            trackInfo.url       = QUrl(result.value("url").toString());
            trackInfo.year      = result.value("year").toInt();
            trackInfo.pictures.append(QUrl(result.value("picture_url").toString().toUtf8()));

            tracksInfo.append(trackInfo);

            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "UPDATE tracks SET playcount = playcount + 1 WHERE id = ?", QVariantList({ result.value("id").toInt() }));
        }
        emit playlist(id, tracksInfo);

        // start the "load more data" sequence
        QString      binds;
        QVariantList values;
        selectedGenresBinds(&binds, &values);
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_LOADMORE_ALBUMGENRES, "SELECT genre_id FROM albums WHERE genre_id IN (" + binds + ") GROUP BY genre_id", values);
        return;
    }
}


// helper
void FMASource::selectedGenresBinds(QString *binds, QVariantList *values)
{
    binds->clear();
    values->clear();

    QStringList bindsList;

    foreach (int selectedGenre, selectedGenres) {
        bindsList.append("?");
        values->append(selectedGenre);
    }

    binds->append(bindsList.join(","));
}


// configuration
void FMASource::sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
{
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);

    if (persistentUniqueId != id) {
        return;
    }

    emit infoMessage(id, error);
}


// client wants to display this plugin's configuration dialog
void FMASource::getUiQml(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    QFile settingsFile("://FMA_CategorySettings.qml");
    settingsFile.open(QFile::ReadOnly);
    QString settings = settingsFile.readAll();
    settingsFile.close();

    QVector<GenreDisplay> sorted;
    sortGenres(0, &sorted, 0);

    int marginTotal = 0;
    foreach (GenreDisplay genreDisplay, sorted) {
        marginTotal += (genreDisplay.isTopLevel ? 36 : 6);
    }

    settings.replace("replace_content_height", QString("(%1 * cb_0.height) + %2").arg(genres.count()).arg(marginTotal - 30));

    QString checkboxes;
    QString checkboxesToAll;
    QString checkboxesToRetval;
    for (int i = 0; i < sorted.count(); i++) {
        checkboxes.append(
            QString("CheckBox { id: %1; text: \"%2\"; tristate: false; checked: %3; anchors.top: %4; anchors.topMargin: %5; anchors.left: parent.left; anchors.leftMargin: %6; foreignId: %7; property int foreignId; }")
            .arg(QString("cb_%1").arg(i))
            .arg(sorted.at(i).isTopLevel ? "<b>" + sorted.at(i).name + "</b>" : sorted.at(i).name)
            .arg(selectedGenres.contains(sorted.at(i).id) ? "true" : "false")
            .arg(i > 0 ? QString("cb_%1.bottom").arg(i - 1) : "parent.top")
            .arg((i > 0) && sorted.at(i).isTopLevel ? "36" : "6")
            .arg(6 + (sorted.at(i).indent * 36))
            .arg(sorted.at(i).id));
        checkboxesToAll.append(QString("%1.checked = allCheck.checked; ").arg(QString("cb_%1").arg(i)));
        checkboxesToRetval.append(QString("if (%1.checked) { retval.push(%1.foreignId); } ").arg(QString("cb_%1").arg(i)));
    }
    settings.replace("CheckBox {}", checkboxes);
    settings.replace("replace_checkboxes_to_all", checkboxesToAll);
    settings.replace("replace_checkboxes_to_retval", checkboxesToRetval);

    emit uiQml(id, settings);

}


// recursive method
void FMASource::sortGenres(int parentId, QVector<GenreDisplay> *sorted, int level)
{
    level++;

    QVector<Genre> withParentId;
    foreach (Genre genre, genres) {
        if (genre.parentId == parentId) {
            withParentId.append(genre);
        }
    }

    qSort(withParentId.begin(), withParentId.end(), [](Genre a, Genre b) {
        return (a.name.compare(b.name) < 0);
    });

    for (int i = 0; i < withParentId.count(); i++) {
        GenreDisplay genreDisplay;
        genreDisplay.id         = withParentId.at(i).id;
        genreDisplay.indent     = (level > 2 ? level - 2 : 0);
        genreDisplay.isTopLevel = (parentId == 0);
        genreDisplay.name       = withParentId.at(i).name;

        sorted->append(genreDisplay);

        sortGenres(withParentId.at(i).id, sorted, level);
    }
}


// slot receiving configuration dialog results
void FMASource::uiResults(QUuid uniqueId, QJsonDocument results)
{
    if (uniqueId != id) {
        return;
    }

    selectedGenres.clear();
    foreach (QJsonValue jsonValue, results.array()) {
        selectedGenres.append(jsonValue.toInt());
    }
    emit saveConfiguration(id, configToJson());
    emit requestRemoveTracks(id);
}


// client wants to receive updates of this plugin's diagnostic information
void FMASource::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = true;
    sendDiagnosticsData();
}


// client doesnt want to receive updates of this plugin's diagnostic information anymore
void FMASource::stopDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = false;
}


// server says unable to start a station
void FMASource::unableToStart(QUuid uniqueId, QUrl url)
{
    if (uniqueId != id) {
        return;
    }
}


// reuest for playlist entries
void FMASource::getPlaylist(QUuid uniqueId, int maxCount)
{
    if (uniqueId != id) {
        return;
    }


    // select tracks to return (these will be dealt with in the sql signal handler)
    emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_GET_PLAYLIST, "SELECT tracks.id, performer, album, title, url, picture_url, track, year FROM tracks LEFT JOIN albums ON tracks.album_id = albums.id WHERE (banned = 0) AND (unable_to_start = 0) ORDER BY playcount, RANDOM() LIMIT ?", QVariantList({ (qrand() % maxCount) + 1 }));

    return;
}


// client wants to dispaly open dialog
void FMASource::getOpenTracks(QUuid uniqueId, QString parentId)
{
    if (uniqueId != id) {
        return;
    }

    /*
        OpenTracks openTracks;

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
void FMASource::resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks)
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
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_OPEN_PLAYLIST, "SELECT id, base, name, genre, url FROM stations WHERE id IN (" + openBinds.join(",") + ")", openValues);
        }
        if (searchValues.count() > 0) {
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_STATION_SEARCH_PLAYLIST, "SELECT id, base, name, genre, url FROM search WHERE id IN (" + searchBinds.join(",") + ")", searchValues);
        }
    */
}


// request for playlist entries based on search criteria
void FMASource::search(QUuid uniqueId, QString criteria)
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
void FMASource::action(QUuid uniqueId, int actionKey, QUrl url)
{
    if (uniqueId != id) {
        return;
    }

    /*
        if (actionKey == 0) {
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "UPDATE stations SET banned = 1 WHERE url = ?", QVariantList({ url.toString() }));
        if (!bannedUrls.contains(url.toString())) {
            bannedUrls.append(url.toString());
            emit saveGlobalConfiguration(id, configToJsonGlobal());
        }
        emit requestRemoveTrack(id, url);
        }

        if (actionKey == 1) {
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "UPDATE stations SET banned = 0 WHERE url = ?", QVariantList({ url.toString() }));
        if (bannedUrls.contains(url.toString())) {
            bannedUrls.removeAll(url.toString());
            emit saveGlobalConfiguration(id, configToJsonGlobal());
        }
        }

        if (sendDiagnostics) {
        sendDiagnosticsData();
        }
    */
}


// get the key (this simple little "decryption" is here just so that the key is not included in the source code in plain text format)
//
// WARNING! The key is exempt from the terms of the GNU General Public License. Usage of the key is resticted to the developers of Waver.
//          Please get your own key if you need one, it's free. Check 'https://freemusicarchive.org/api' for details.
//
QString FMASource::getKey()
{
    QFile keyFile("://fma_key");
    keyFile.open(QFile::ReadOnly);
    QString keyEncrypted = keyFile.readAll();
    keyFile.close();

    if (!keyEncrypted.contains(QChar(30))) {
        return "";
    }
    keyEncrypted = keyEncrypted.mid(keyEncrypted.indexOf(QChar(30)) + 1);

    QString key;
    int     pos = 0;
    while (pos < keyEncrypted.length()) {
        QString temp = keyEncrypted.mid(pos, 2);
        pos = pos + 2;

        bool OK = false;
        int  number = temp.toInt(&OK, 16);
        if (!OK) {
            return "";
        }
        key.append(QChar(number));

        pos = pos + (number * 2);
    }

    return key;
}


// timer slot
void FMASource::genreSearch()
{
    if (genreSearchItems.count() < 1) {
        return;
    }

    // have to wait for Idle
    if (state != Idle) {
        QTimer::singleShot(NETWORK_WAIT_MS, this, SLOT(genreSearch()));
        return;
    }

    // figure out genre handle
    QString genreHandle;
    foreach (Genre genre, genres) {
        if (genre.id == genreSearchItems.first().genreId) {
            genreHandle = genre.handle;
            break;
        }
    }
    if (genreHandle.isEmpty()) {
        emit infoMessage(id, "Can not find genre handle");
        return;
    }

    // set state
    setState(genreSearchItems.first().state);

    // send request
    QNetworkRequest request(QUrl("https://freemusicarchive.org/api/get/albums.xml?api_key=" + key + "&genre_handle=" + genreHandle + "&limit=50"));
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    networkAccessManager->get(request);
}


// timer slot
void FMASource::albumSearch()
{
    if (albumSearchItems.count() < 1) {
        return;
    }

    // have to wait for Idle
    if (state != Idle) {
        QTimer::singleShot(NETWORK_WAIT_MS, this, SLOT(albumSearch()));
        return;
    }


    // set state
    setState(albumSearchItems.first().state);

    // send request
    QNetworkRequest request(QUrl("https://freemusicarchive.org/api/get/tracks.xml?api_key=" + key + "&album_id=" + QString("%1").arg(albumSearchItems.first().albumId) + "&limit=50"));
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    networkAccessManager->get(request);
}


// configuration conversion
QJsonDocument FMASource::configToJson()
{
    QVariantList variantList;
    foreach (int selectedGenre, selectedGenres) {
        variantList.append(selectedGenre);
    }

    QJsonObject jsonObject;
    jsonObject.insert("selected_genres", QJsonArray::fromVariantList(variantList));

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
QJsonDocument FMASource::configToJsonGlobal()
{
    QJsonArray jsonArrayGenres;
    foreach (Genre genre, genres) {
        jsonArrayGenres.append(QJsonArray({ genre.id, genre.parentId, genre.handle, genre.name }));
    }

    /*
        QJsonArray jsonArrayBanned;
        foreach (QUrl bannedUrl, bannedUrls) {
        jsonArrayBanned.append(bannedUrl.toString());
        }

        QJsonArray jsonArrayUnableToStart;
        foreach (UnableToStartUrl unableToStartUrl, unableToStartUrls) {
        jsonArrayUnableToStart.append(QJsonArray({ unableToStartUrl.url.toString(), unableToStartUrl.timestamp }));
        }
    */

    QJsonObject jsonObject;

    jsonObject.insert("genre_list", jsonArrayGenres);
    jsonObject.insert("genre_list_timestamp", genresLoaded.toMSecsSinceEpoch());
    /*
        jsonObject.insert("banned", jsonArrayBanned);
        jsonObject.insert("unable_to_start", jsonArrayUnableToStart);
    */

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
void FMASource::jsonToConfig(QJsonDocument jsonDocument)
{

    if (jsonDocument.object().contains("selected_genres")) {
        selectedGenres.clear();

        foreach (QJsonValue jsonValue, jsonDocument.object().value("selected_genres").toArray()) {
            selectedGenres.append(jsonValue.toInt());
        }
    }
}


// configuration conversion
void FMASource::jsonToConfigGlobal(QJsonDocument jsonDocument)
{
    if (jsonDocument.object().contains("genre_list")) {
        genres.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("genre_list").toArray()) {
            QJsonArray value = jsonValue.toArray();
            genres.append({ value.at(0).toInt(), value.at(1).toInt(), value.at(2).toString(), value.at(3).toString() });
        }
    }
    if (jsonDocument.object().contains("genre_list_timestamp")) {
        genresLoaded = QDateTime::fromMSecsSinceEpoch((qint64)jsonDocument.object().value("genre_list_timestamp").toDouble());
    }
    /*
        if (jsonDocument.object().contains("banned")) {
        bannedUrls.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("banned").toArray()) {
            bannedUrls.append(QUrl(jsonValue.toString()));
        }
        }
        if (jsonDocument.object().contains("unable_to_start")) {
        unableToStartUrls.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("unable_to_start").toArray()) {
            QJsonArray value = jsonValue.toArray();
            unableToStartUrls.append({ QUrl(value.at(0).toString()), (qint64)value.at(1).toDouble() });
        }
        }
    */
}


// network signal handler
void FMASource::networkFinished(QNetworkReply *reply)
{
    // make sure reply OK
    if (reply->error() != QNetworkReply::NoError) {
        emit infoMessage(id, QString("Network error: %1").arg(reply->errorString()));
        setState(Idle);
        reply->deleteLater();
        return;
    }
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 200) {
        emit infoMessage(id, QString("FMA returned status %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString()));
        setState(Idle);
        reply->deleteLater();
        return;
    }

    int page  = 0;
    int pages = 0;

    ParseElement parseElement = Unknown;
    Genre        genreTemp;
    Album        albumTemp;
    Track        trackTemp;
    QStringList  binds;
    QVariantList values;

    QXmlStreamReader xmlStreamReader(reply);
    while (!xmlStreamReader.atEnd()) {
        QXmlStreamReader::TokenType tokenType = xmlStreamReader.readNext();

        if (tokenType == QXmlStreamReader::StartElement) {
            if (xmlStreamReader.name().toString().compare("page") == 0) {
                parseElement = Page;
            }
            if (xmlStreamReader.name().toString().compare("total_pages") == 0) {
                parseElement = TotalPages;
            }

            if ((xmlStreamReader.name().toString().compare("value") == 0) && (parseElement == Unknown)) {
                genreTemp.id       = 0;
                genreTemp.parentId = 0;
                genreTemp.handle   = "";
                genreTemp.name     = "";

                albumTemp.id        = 0;
                albumTemp.genreId   = (genreSearchItems.count() > 0 ? genreSearchItems.first().genreId : 0);
                albumTemp.album      = "";
                albumTemp.performer = "";
                albumTemp.year      = 0;

                trackTemp.id         = 0;
                trackTemp.albumId    = (albumSearchItems.count() > 0 ? albumSearchItems.first().albumId : 0);
                trackTemp.title      = "";
                trackTemp.url        = "";
                trackTemp.pictureUrl = "";
                trackTemp.track     = 0;
            }

            if (state == GenreList) {
                if (xmlStreamReader.name().toString().compare("genre_id") == 0) {
                    parseElement = GenreId;
                }
                if (xmlStreamReader.name().toString().compare("genre_parent_id") == 0) {
                    parseElement = GenreParentId;
                }
                if (xmlStreamReader.name().toString().compare("genre_handle") == 0) {
                    parseElement = GenreHandle;
                }
                if (xmlStreamReader.name().toString().compare("genre_title") == 0) {
                    parseElement = GenreTitle;
                }
            }

            if (state == AlbumList) {
                if (xmlStreamReader.name().toString().compare("album_id") == 0) {
                    parseElement = AlbumId;
                }
                if (xmlStreamReader.name().toString().compare("album_title") == 0) {
                    parseElement = AlbumTitle;
                }
                if (xmlStreamReader.name().toString().compare("album_date_released") == 0) {
                    parseElement = AlbumDateReleased;
                }
                if (xmlStreamReader.name().toString().compare("artist_name") == 0) {
                    parseElement = ArtistName;
                }
                if (xmlStreamReader.name().toString().compare("album_images") == 0) {
                    parseElement = AlbumImages;
                }
            }

            if (state == TrackList) {
                if (xmlStreamReader.name().toString().compare("track_id") == 0) {
                    parseElement = TrackId;
                }
                if (xmlStreamReader.name().toString().compare("track_title") == 0) {
                    parseElement = TrackTitle;
                }
                if (xmlStreamReader.name().toString().compare("track_url") == 0) {
                    parseElement = TrackUrl;
                }
                if (xmlStreamReader.name().toString().compare("track_image_file") == 0) {
                    parseElement = TrackImageFile;
                }
                if (xmlStreamReader.name().toString().compare("artist_name") == 0) {
                    parseElement = ArtistName;
                }
                if (xmlStreamReader.name().toString().compare("album_title") == 0) {
                    parseElement = AlbumTitle;
                }
                if (xmlStreamReader.name().toString().compare("track_number") == 0) {
                    parseElement = TrackNumber;
                }
                if (xmlStreamReader.name().toString().compare("track_genres") == 0) {
                    parseElement = TrackGenres;
                }
            }
        }

        if (tokenType == QXmlStreamReader::Characters) {
            QString text = xmlStreamReader.text().toString();
            switch (parseElement) {
                case Page:
                    stringToInt(text, &page);
                    if ((state == GenreList) && (page == 1)) {
                        genres.clear();
                    }
                    break;
                case TotalPages:
                    stringToInt(text, &pages);
                    break;

                case GenreId:
                    stringToInt(text, &genreTemp.id);
                    break;
                case GenreHandle:
                    genreTemp.handle = text;
                    break;
                case GenreParentId:
                    stringToInt(text, &genreTemp.parentId);
                    break;
                case GenreTitle:
                    genreTemp.name = text;
                    break;

                case AlbumId:
                    stringToInt(text, &albumTemp.id);
                    break;
                case AlbumTitle:
                    albumTemp.album = text;
                    break;
                case AlbumDateReleased:
                    stringToInt(text.right(4), &albumTemp.year);
                    break;
                case ArtistName:
                    albumTemp.performer = text;
                    break;

                case TrackId:
                    stringToInt(text, &trackTemp.id);
                    break;
                case TrackTitle:
                    trackTemp.title = text;
                    break;
                case TrackUrl:
                    trackTemp.url = text.append("/download");
                    break;
                case TrackImageFile:
                    trackTemp.pictureUrl = text;
                    break;
                case TrackNumber:
                    stringToInt(text, &trackTemp.track);
                    break;
            }
        }

        if (tokenType == QXmlStreamReader::EndElement) {
            if (parseElement == TrackGenres) {
                if (xmlStreamReader.name().toString().compare("track_genres") == 0) {
                    parseElement = Unknown;
                }
            }
            else if (parseElement == AlbumImages) {
                if (xmlStreamReader.name().toString().compare("album_images") == 0) {
                    parseElement = Unknown;
                }
            }
            else {
                parseElement = Unknown;
            }

            if ((xmlStreamReader.name().toString().compare("value") == 0) && (parseElement == Unknown)) {
                if ((genreTemp.id > 0) && !genreTemp.name.isEmpty() && !genreTemp.handle.isEmpty()) {
                    genres.append(genreTemp);
                }

                if (state == AlbumList) {
                    binds.append("(?,?,?,?,?)");

                    values.append(albumTemp.id);
                    values.append(albumTemp.genreId);
                    values.append(albumTemp.album);
                    values.append(albumTemp.performer);
                    values.append(albumTemp.year);

                    // can't have too many in one single statement
                    if (values.count() >= 750) {
                        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO albums (id, genre_id, album, performer, year) VALUES " + binds.join(","), values);
                        binds.clear();
                        values.clear();
                    }
                }

                if (state == TrackList) {
                    binds.append("(?,?,?,?,?,?)");

                    values.append(trackTemp.id);
                    values.append(trackTemp.albumId);
                    values.append(trackTemp.title);
                    values.append(trackTemp.url);
                    values.append(trackTemp.pictureUrl);
                    values.append(trackTemp.track);

                    // can't have too many in one single statement
                    if (values.count() >= 750) {
                        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO tracks (id, album_id, title, url, picture_url, track) VALUES " + binds.join(","), values);
                        binds.clear();
                        values.clear();
                    }
                }
            }
        }
    }
    if (xmlStreamReader.hasError()) {
        emit infoMessage(id, "Error while parsing XML response from FMA");
        setState(Idle);
        reply->deleteLater();
        return;
    }

    // finish saving
    if (values.count() > 0) {
        if (state == AlbumList) {
            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO albums (id, genre_id, album, performer, year) VALUES " + binds.join(","), values);
        }
        if (state == TrackList) {
            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO tracks (id, album_id, title, url, picture_url, track) VALUES " + binds.join(","), values);
        }
    }

    // let's get every page
    if ((page > 0) && (pages > 0) && (page < pages)) {
        QString urlString = reply->request().url().toString();
        reply->deleteLater();
        urlString.remove(QRegExp("&page=\\d+"));
        urlString.append(QString("&page=%1").arg(page + 1));
        QUrl url(urlString);
        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", userAgent.toUtf8());
        networkAccessManager->get(request);
        return;
    }

    // genre list
    if (state == GenreList) {
        // save configuration
        genresLoaded = QDateTime::currentDateTime();
        emit saveGlobalConfiguration(id, configToJsonGlobal());

        // set state
        setState(Idle);
        reply->deleteLater();

        // this was initiated by nonexisting global configuration, so continue startup sequence
        emit loadConfiguration(id);

        return;
    }

    // albums list (genre query)
    if (state == AlbumList) {
        // saving already finished so let's fake it
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_LOADMORE_ALBUMSLOADED, "SELECT CURRENT_TIMESTAMP", QVariantList());

        genreSearchItems.removeFirst();

        setState(Idle);

        QTimer::singleShot(NETWORK_WAIT_MS, this, SLOT(genreSearch()));

        return;
    }

    // tracks list (album query)
    if (state == TrackList) {
        // saving already finished so let's fake it
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_LOADMORE_TRACKSLOADED, "SELECT CURRENT_TIMESTAMP", QVariantList());

        albumSearchItems.removeFirst();

        setState(Idle);

        QTimer::singleShot(NETWORK_WAIT_MS, this, SLOT(albumSearch()));

        return;
    }
}


// helper
bool FMASource::stringToInt(QString str, int *num)
{
    bool OK = false;
    int  temp;
    temp = str.toInt(&OK);
    if (OK) {
        *num = temp;
    }
    return OK;
}


// helper
void FMASource::setState(State state)
{
    this->state = state;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// diagnostics
void FMASource::sendDiagnosticsData()
{

}
