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


#include "radiosource.h"


// plugin factory
void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal)
{
    if (pluginTypesMask & PLUGIN_TYPE_SOURCE) {
        retVal->append((QObject *) new RadioSource());
    }
}


// constructor
RadioSource::RadioSource()
{
    id  = QUuid("{3CDF6F84-3B1B-4807-8AA0-089509FD4751}");
    key = getKey();

    networkAccessManager  = NULL;
    playlistAccessManager = NULL;
    readySent             = false;
    sendDiagnostics       = false;
    state                 = Idle;

    // TODO since I updated Qt, copy below does't work for some reason
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wp_radiosource.png");
    QFile::copy(":/wp_radiosource.png", QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wp_radiosource.png");
}


// destructor
RadioSource::~RadioSource()
{
    if (networkAccessManager != NULL) {
        networkAccessManager->deleteLater();
    }
    if (playlistAccessManager != NULL) {
        playlistAccessManager->deleteLater();
    }
}


// overridden virtual function
int RadioSource::pluginType()
{
    return PLUGIN_TYPE_SOURCE;
}


// overridden virtual function
QString RadioSource::pluginName()
{
    return "Online Radio Stations";
}


// overridden virtual function
int RadioSource::pluginVersion()
{
    return 4;
}


// overrided virtual function
QString RadioSource::waverVersionAPICompatibility()
{
    return "0.0.5";
}


// overridden virtual function
bool RadioSource::hasUI()
{
    return true;
}


// overridden virtual function
void RadioSource::setUserAgent(QString userAgent)
{
    this->userAgent = userAgent;
}


// overrided virtual function
QUrl RadioSource::menuImageURL()
{
    return QUrl();
}


// overridden virtual function
QUuid RadioSource::persistentUniqueId()
{
    return id;
}


// thread entry
void RadioSource::run()
{
    qsrand(QDateTime::currentDateTime().toTime_t());

    networkAccessManager = new QNetworkAccessManager();
    connect(networkAccessManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(networkFinished(QNetworkReply *)));

    playlistAccessManager = new QNetworkAccessManager();
    connect(playlistAccessManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(playlistFinished(QNetworkReply *)));

    // temporary db
    emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_CREATE_TABLE_STATIONS, "CREATE TABLE stations (id UNIQUE, base, name, genre, url DEFAULT NULL, logo, banned INT DEFAULT 0, loved INT DEFAULT 0, unable_to_start INT DEFAULT 0, playcount INT DEFAULT 0)", QVariantList());
}


// slot receiving configuration
void RadioSource::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "DELETE FROM stations", QVariantList());
    stationsLoaded.clear();

    jsonToConfig(configuration);

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    if (selectedGenres.count() < 1) {
        readySent = false;
        emit unready(id);
        return;
    }

    // get some stations to start with
    genreSearchItems.append({ selectedGenres.at(qrand() % selectedGenres.count()), StationList });
    genreSearch();
}


// slot receiving configuration
void RadioSource::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfigGlobal(configuration);
    removeExpiredUnableToStartUrls();

    if (genresLoaded.daysTo(QDateTime::currentDateTime()) > GENRES_EXPIRY_DAYS) {
        genres.clear();
    }

    if (genres.count() < 1) {
        readySent = false;
        emit unready(id);

        setState(GenreList);

        QNetworkRequest request(QUrl("http://api.shoutcast.com/genre/secondary?parentid=0&k=" + key + "&f=xml"));
        request.setRawHeader("User-Agent", userAgent.toUtf8());
        networkAccessManager->get(request);

        return;
    }

    emit loadConfiguration(id);
}


// configuration
void RadioSource::sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// temporary storage
void RadioSource::globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);

    if (persistentUniqueId != id) {
        return;
    }

    if (clientSqlIdentifier == SQL_CREATE_TABLE_STATIONS) {
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_CREATE_TABLE_SEARCH, "CREATE TABLE search (id, base, name, genre, url DEFAULT NULL, criteria)", QVariantList());
        return;
    }

    if (clientSqlIdentifier == SQL_CREATE_TABLE_SEARCH) {
        emit loadGlobalConfiguration(id);
        return;
    }

    if (clientSqlIdentifier == SQL_GENRE_SEARCH_STATION_LIST) {
        if (!readySent) {
            readySent = true;
            emit ready(id);
        }
        return;
    }

    if (clientSqlIdentifier == SQL_GET_PLAYLIST) {
        if (results.count() == 0) {
            maintenance();
            return;
        }

        foreach (QVariantHash result, results) {
            StationTemp stationTemp;
            stationTemp.id          = result.value("id").toString();
            stationTemp.base        = result.value("base").toString();
            stationTemp.name        = result.value("name").toString();
            stationTemp.genre       = result.value("genre").toString();
            stationTemp.url         = QUrl(result.value("url").toString());
            stationTemp.logo        = QUrl(result.value("logo").toString());
            stationTemp.lovedMode   = PLAYLIST_MODE_NORMAL;
            stationTemp.destination = Playlist;

            tuneInTemp.append(stationTemp);
        }

        tuneInStarter();
        return;
    }

    if (clientSqlIdentifier == SQL_GET_LOVED) {
        if (results.count() == 0) {
            QUrl lovedUrl = lovedUrls.at(qrand() % lovedUrls.count());

            TrackInfo trackInfo;
            trackInfo.album     = "";
            trackInfo.cast      = true;
            trackInfo.performer = "Online Radio";
            trackInfo.title     = "Station You Love\n(name currently not available)";
            trackInfo.track     = 0;
            trackInfo.url       = lovedUrl;
            trackInfo.year      = 0;
            trackInfo.pictures.append(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wp_radiosource.png");
            if (bannedUrls.contains(lovedUrl)) {
                trackInfo.actions.append({ id, 1, "Remove ban" });
            }
            else {
                trackInfo.actions.append({ id, 0, "Ban" });
            }
            trackInfo.actions.append({ id, 11, "Unlove" });

            TracksInfo tracksInfo;
            ExtraInfo  extraInfo;
            tracksInfo.append(trackInfo);
            extraInfo.insert(trackInfo.url, { { "loved", PLAYLIST_MODE_LOVED }, { "loved_longplay", 1 } });
            emit playlist(id, tracksInfo, extraInfo);

            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "UPDATE stations SET playcount = playcount + 1 WHERE url = ?", QVariantList({ lovedUrl.toString() }));

            return;
        }

        foreach (QVariantHash result, results) {
            StationTemp stationTemp;
            stationTemp.id          = result.value("id").toString();
            stationTemp.base        = result.value("base").toString();
            stationTemp.name        = result.value("name").toString();
            stationTemp.genre       = result.value("genre").toString();
            stationTemp.url         = QUrl(result.value("url").toString());
            stationTemp.logo        = QUrl(result.value("logo").toString());
            stationTemp.lovedMode   = PLAYLIST_MODE_LOVED;
            stationTemp.destination = Playlist;

            tuneInTemp.append(stationTemp);
        }

        tuneInStarter();
        return;
    }

    if (clientSqlIdentifier == SQL_GET_REPLACEMENT) {
        if (results.count() == 0) {
            maintenance();
            return;
        }

        foreach (QVariantHash result, results) {
            StationTemp stationTemp;
            stationTemp.id          = result.value("id").toString();
            stationTemp.base        = result.value("base").toString();
            stationTemp.name        = result.value("name").toString();
            stationTemp.genre       = result.value("genre").toString();
            stationTemp.url         = QUrl(result.value("url").toString());
            stationTemp.logo        = QUrl(result.value("logo").toString());
            stationTemp.lovedMode   = PLAYLIST_MODE_NORMAL;
            stationTemp.destination = Replacement;

            tuneInTemp.append(stationTemp);
        }

        tuneInStarter();
        return;
    }

    if (clientSqlIdentifier == SQL_GENRE_SEARCH_OPENING) {
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_OPEN_GENRE_STATIONS, "SELECT id, name FROM stations WHERE genre = ?", QVariantList({ openGenreSearchGenre }));
        return;
    }

    if (clientSqlIdentifier == SQL_OPEN_GENRE_STATIONS) {
        OpenTracks openTracks;
        foreach (QVariantHash result, results) {
            OpenTrack openTrack;
            openTrack.hasChildren = false;
            openTrack.id          = result.value("id").toString();
            openTrack.label       = result.value("name").toString();
            openTrack.selectable  = true;
            openTracks.append(openTrack);
        }

        qSort(openTracks.begin(), openTracks.end(), [](OpenTrack a, OpenTrack b) {
            return (a.label.compare(b.label) < 0);
        });

        emit openTracksResults(id, openTracks);

        return;
    }

    if (clientSqlIdentifier == SQL_OPEN_PLAYLIST) {
        foreach (QVariantHash result, results) {
            StationTemp stationTemp;
            stationTemp.id          = result.value("id").toString();
            stationTemp.base        = result.value("base").toString();
            stationTemp.name        = result.value("name").toString();
            stationTemp.genre       = result.value("genre").toString();
            stationTemp.url         = QUrl(result.value("url").toString());
            stationTemp.logo        = QUrl(result.value("logo").toString());
            stationTemp.lovedMode   = PLAYLIST_MODE_NORMAL;
            stationTemp.destination = Open;

            tuneInTemp.append(stationTemp);
        }

        tuneInStarter();
        return;
    }

    if (clientSqlIdentifier == SQL_STATION_SEARCH_OPENING) {
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_STATION_SEARCH_STATIONS, "SELECT * FROM search WHERE criteria = ?", QVariantList({ stationSearchCriteria }));
        return;
    }

    if (clientSqlIdentifier == SQL_STATION_SEARCH_STATIONS) {
        if (results.count() < 1) {
            stationSearch();
            return;
        }
        if ((results.count() == 1) && (results.at(0).value("name").toString().compare("NOTHING") == 0)) {
            return;
        }

        OpenTracks openTracks;
        foreach (QVariantHash result, results) {
            OpenTrack openTrack;
            openTrack.hasChildren = false;
            openTrack.id          = "SEARCH_" + result.value("id").toString();
            openTrack.label       = result.value("name").toString();
            openTrack.selectable  = true;
            openTracks.append(openTrack);
        }

        qSort(openTracks.begin(), openTracks.end(), [](OpenTrack a, OpenTrack b) {
            return (a.label.compare(b.label) < 0);
        });

        emit searchResults(id, openTracks);
    }

    if (clientSqlIdentifier == SQL_STATION_SEARCH_PLAYLIST) {
        foreach (QVariantHash result, results) {
            StationTemp stationTemp;
            stationTemp.id          = result.value("id").toString();
            stationTemp.base        = result.value("base").toString();
            stationTemp.name        = result.value("name").toString();
            stationTemp.genre       = result.value("genre").toString();
            stationTemp.url         = QUrl(result.value("url").toString());
            stationTemp.logo        = "";
            stationTemp.lovedMode   = PLAYLIST_MODE_NORMAL;
            stationTemp.destination = Search;

            tuneInTemp.append(stationTemp);
        }

        tuneInStarter();
        return;
    }

    if (clientSqlIdentifier == SQL_STATION_UPDATED_PLAYLIST) {
        QStringList notPlayed = clientIdentifier.split(',');

        int playlistNotPlayed    = QString(notPlayed.at(0)).toInt();
        int replacementNotPlayed = QString(notPlayed.at(1)).toInt();

        if (playlistNotPlayed) {
            getPlaylist(id, playlistNotPlayed, PLAYLIST_MODE_NORMAL);
        }
        for (int i = 0; i < replacementNotPlayed; i++) {
            getReplacement(id);
        }

        return;
    }

    if (clientSqlIdentifier == SQL_SEARCH_COUNT) {
        int counter = results.at(0).value("counter").toString().toInt();
        if (counter > SEARCH_TABLE_LIMIT) {
            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "DELETE FROM search", QVariantList());
        }
        return;
    }

    if (clientSqlIdentifier == SQL_DIAGNOSTICS) {
        DiagnosticData diagnosticData;

        switch (state) {
            case Idle:
                diagnosticData.append({ "Status", "Idle"});
                break;
            case GenreList:
                diagnosticData.append({ "Status", "Getting genre list..." });
                break;
            case StationList:
                diagnosticData.append({ "Status", "Caching station data..." });
                break;
            case Opening:
                diagnosticData.append({ "Status", "Getting station data..." });
                break;
            case Searching:
                diagnosticData.append({ "Status", "Searching stations..." });
                break;
            case TuneIn:
                diagnosticData.append({ "Status", "Tuning in to stations..." });
                break;
        }


        diagnosticData.append({ "Banned stations", QString("%1").arg(bannedUrls.count()) });
        diagnosticData.append({ "Loved stations", QString("%1").arg(lovedUrls.count()) });

        foreach (QVariantHash result, results) {
            DiagnosticItem stationCount;
            stationCount.label   = result.value("genre").toString();
            stationCount.message = result.value("counter").toString();
            diagnosticData.append(stationCount);
        }

        emit diagnostics(id, diagnosticData);
        return;
    }
}


// configuration
void RadioSource::sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
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
void RadioSource::getUiQml(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    QFile settingsFile("://RSSettings.qml");
    settingsFile.open(QFile::ReadOnly);
    QString settings = settingsFile.readAll();
    settingsFile.close();

    int marginTotal = 0;
    foreach (Genre genre, genres) {
        marginTotal += (genre.isPrimary ? 36 : 6);
    }

    settings.replace("replace_content_height", QString("(%1 * cb_0.height) + %2").arg(genres.count()).arg(marginTotal - 30));

    QString checkboxes;
    QString checkboxesToAll;
    QString checkboxesToRetval;
    for (int i = 0; i < genres.count(); i++) {
        checkboxes.append(
            QString("CheckBox { id: %1; text: \"%2\"; tristate: false; checked: %3; anchors.top: %4; anchors.topMargin: %5; anchors.left: parent.left; anchors.leftMargin: 6 }")
            .arg(QString("cb_%1").arg(i))
            .arg(genres.at(i).isPrimary ? "<b>" + genres.at(i).name + "</b>" : genres.at(i).name)
            .arg(selectedGenres.contains(genres.at(i).name) ? "true" : "false")
            .arg(i > 0 ? QString("cb_%1.bottom").arg(i - 1) : "parent.top")
            .arg((i > 0) && genres.at(i).isPrimary ? "36" : "6"));
        checkboxesToAll.append(QString("%1.checked = allCheck.checked; ").arg(QString("cb_%1").arg(i)));
        checkboxesToRetval.append(QString("if (%1.checked) { retval.push(%1.text); } ").arg(QString("cb_%1").arg(i)));
    }
    settings.replace("CheckBox {}", checkboxes);
    settings.replace("replace_checkboxes_to_all", checkboxesToAll);
    settings.replace("replace_checkboxes_to_retval", checkboxesToRetval);

    emit uiQml(id, settings);
}


// slot receiving configuration dialog results
void RadioSource::uiResults(QUuid uniqueId, QJsonDocument results)
{
    if (uniqueId != id) {
        return;
    }

    emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "DELETE FROM stations", QVariantList());
    stationsLoaded.clear();

    selectedGenres.clear();
    foreach (QJsonValue jsonValue, results.array()) {
        selectedGenres.append(jsonValue.toString().replace(QRegExp("<\\/?b>"), ""));
    }
    emit saveConfiguration(id, configToJson());
    emit requestRemoveTracks(id);

    if (selectedGenres.count() < 1) {
        readySent = false;
        emit unready(id);
        return;
    }

    genreSearchItems.append({ selectedGenres.at(qrand() % selectedGenres.count()), StationList });
    genreSearch();
}


// client wants to receive updates of this plugin's diagnostic information
void RadioSource::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = true;
    sendDiagnosticsData();
}


// client doesnt want to receive updates of this plugin's diagnostic information anymore
void RadioSource::stopDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = false;
}


// server says unable to start a station
void RadioSource::unableToStart(QUuid uniqueId, QUrl url)
{
    if (uniqueId != id) {
        return;
    }

    emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "UPDATE stations SET unable_to_start = 1 WHERE url = ?", QVariantList({ url.toString() }));

    bool found = false;
    foreach (UnableToStartUrl unableToStartUrl, unableToStartUrls) {
        if (unableToStartUrl.url == url) {
            found = true;
            break;
        }
    }
    if (!found) {
        unableToStartUrls.append({ url, QDateTime::currentMSecsSinceEpoch() });
        emit saveGlobalConfiguration(id, configToJsonGlobal());
    }
}


// server says station stopped playing too early
void RadioSource::castFinishedEarly(QUuid uniqueId, QUrl url, int playedSeconds)
{
    if (uniqueId != id) {
        return;
    }

    if (playedSeconds < 30) {
        unableToStart(id, url);
    }
}


// reuest for playlist entries
void RadioSource::getPlaylist(QUuid uniqueId, int trackCount, int mode)
{
    if (uniqueId != id) {
        return;
    }

    // loved and similar is handled the same, after all these are radio stations
    if ((mode != PLAYLIST_MODE_NORMAL) && (lovedUrls.count() > 0)) {
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_GET_LOVED, "SELECT id, base, name, genre, url, logo FROM stations WHERE (banned = 0) AND (unable_to_start = 0) AND (loved <> 0) ORDER BY playcount, RANDOM() LIMIT 1", QVariantList());
        trackCount--;
    }

    // select stations to return (these will be dealt with in the sql signal handler)
    if (trackCount > 0) {
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_GET_PLAYLIST, "SELECT id, base, name, genre, url, logo FROM stations WHERE (banned = 0) AND (unable_to_start = 0) ORDER BY playcount, RANDOM() LIMIT ?", QVariantList({ trackCount }));
    }

    return;
}


// get replacement for a track that could not start or ended prematurely
void RadioSource::getReplacement(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_GET_REPLACEMENT, "SELECT id, base, name, genre, url, logo FROM stations WHERE (banned = 0) AND (unable_to_start = 0) ORDER BY playcount, RANDOM() LIMIT 1", QVariantList());
}


// client wants to dispaly open dialog
void RadioSource::getOpenTracks(QUuid uniqueId, QString parentId)
{
    if (uniqueId != id) {
        return;
    }

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
}


// turn open dialog selections to playlist entries
void RadioSource::resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks)
{
    if (uniqueId != id) {
        return;
    }

    if (selectedTracks.count() < 1) {
        return;
    }

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
}


// request for playlist entries based on search criteria
void RadioSource::search(QUuid uniqueId, QString criteria)
{
    if (uniqueId != id) {
        return;
    }

    stationSearchCriteria = criteria.toLower();

    emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_STATION_SEARCH_STATIONS, "SELECT * FROM search WHERE criteria = ?", QVariantList({ stationSearchCriteria }));
}


// user clicked action that was included in track info
void RadioSource::action(QUuid uniqueId, int actionKey, TrackInfo trackInfo)
{
    if (uniqueId != id) {
        return;
    }

    if (actionKey == 0) {
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "UPDATE stations SET banned = 1 WHERE url = ?", QVariantList({ trackInfo.url.toString() }));
        if (!bannedUrls.contains(trackInfo.url)) {
            bannedUrls.append(trackInfo.url);
            emit saveGlobalConfiguration(id, configToJsonGlobal());
        }
        emit requestRemoveTrack(id, trackInfo.url);
    }

    if (actionKey == 1) {
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "UPDATE stations SET banned = 0 WHERE url = ?", QVariantList({ trackInfo.url.toString() }));
        if (bannedUrls.contains(trackInfo.url)) {
            bannedUrls.removeAll(trackInfo.url);
            emit saveGlobalConfiguration(id, configToJsonGlobal());
        }
    }

    if (actionKey == 10) {
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "UPDATE stations SET loved = 1 WHERE url = ?", QVariantList({ trackInfo.url.toString() }));
        if (!lovedUrls.contains(trackInfo.url)) {
            lovedUrls.append(trackInfo.url);
            emit saveGlobalConfiguration(id, configToJsonGlobal());
        }

        TrackInfo trackInfoTemp;
        trackInfoTemp.track = 0;
        trackInfoTemp.year  = 0;
        trackInfoTemp.url   = trackInfo.url;
        if (bannedUrls.contains(trackInfo.url)) {
            trackInfoTemp.actions.append({ id, 1, "Remove ban" });
        }
        else {
            trackInfoTemp.actions.append({ id, 0, "Ban" });
        }
        trackInfoTemp.actions.append({ id, 11, "Unlove" });
        emit updateTrackInfo(id, trackInfoTemp);
    }

    if (actionKey == 11) {
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "UPDATE stations SET loved = 0 WHERE url = ?", QVariantList({ trackInfo.url.toString() }));
        if (lovedUrls.contains(trackInfo.url)) {
            lovedUrls.removeAll(trackInfo.url);
            emit saveGlobalConfiguration(id, configToJsonGlobal());
        }

        TrackInfo trackInfoTemp;
        trackInfoTemp.track = 0;
        trackInfoTemp.year  = 0;
        trackInfoTemp.url   = trackInfo.url;
        if (bannedUrls.contains(trackInfo.url)) {
            trackInfoTemp.actions.append({ id, 1, "Remove ban" });
        }
        else {
            trackInfoTemp.actions.append({ id, 0, "Ban" });
        }
        trackInfoTemp.actions.append({ id, 10, "Love" });
        emit updateTrackInfo(id, trackInfoTemp);
    }

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// configuration conversion
QJsonDocument RadioSource::configToJson()
{
    QJsonObject jsonObject;
    jsonObject.insert("selected_genres", QJsonArray::fromStringList(selectedGenres));

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
QJsonDocument RadioSource::configToJsonGlobal()
{
    QJsonArray jsonArrayGenres;
    foreach (Genre genre, genres) {
        jsonArrayGenres.append(QJsonArray({ genre.name, genre.isPrimary }));
    }

    QJsonArray jsonArrayBanned;
    foreach (QUrl bannedUrl, bannedUrls) {
        jsonArrayBanned.append(bannedUrl.toString());
    }

    QJsonArray jsonArrayLoved;
    foreach (QUrl lovedUrl, lovedUrls) {
        jsonArrayLoved.append(lovedUrl.toString());
    }

    QJsonArray jsonArrayUnableToStart;
    foreach (UnableToStartUrl unableToStartUrl, unableToStartUrls) {
        jsonArrayUnableToStart.append(QJsonArray({ unableToStartUrl.url.toString(), unableToStartUrl.timestamp }));
    }

    QJsonObject jsonObject;

    jsonObject.insert("genre_list", jsonArrayGenres);
    jsonObject.insert("genre_list_timestamp", genresLoaded.toMSecsSinceEpoch());
    jsonObject.insert("banned", jsonArrayBanned);
    jsonObject.insert("loved", jsonArrayLoved);
    jsonObject.insert("unable_to_start", jsonArrayUnableToStart);

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
void RadioSource::jsonToConfig(QJsonDocument jsonDocument)
{
    selectedGenres.clear();

    if (jsonDocument.object().contains("selected_genres")) {
        foreach (QJsonValue jsonValue, jsonDocument.object().value("selected_genres").toArray()) {
            QString value = jsonValue.toString();

            // make sure to use only existing
            bool found = false;
            foreach (Genre genre, genres) {
                if (genre.name.compare(value) == 0) {
                    found = true;
                }
            }
            if (!found) {
                continue;
            }

            // some subgenres appear more than once
            if (!selectedGenres.contains(value)) {
                selectedGenres.append(value);
            }
        }
    }
    else {
        selectedGenres.append(genres.at(qrand() % genres.count()).name);
    }
}


// configuration conversion
void RadioSource::jsonToConfigGlobal(QJsonDocument jsonDocument)
{
    if (jsonDocument.object().contains("genre_list")) {
        genres.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("genre_list").toArray()) {
            QJsonArray value = jsonValue.toArray();
            genres.append({ value.at(0).toString(), value.at(1).toBool() });
        }
    }
    if (jsonDocument.object().contains("genre_list_timestamp")) {
        genresLoaded = QDateTime::fromMSecsSinceEpoch((qint64)jsonDocument.object().value("genre_list_timestamp").toDouble());   // TODO wrote in normal format to cfg file
    }
    if (jsonDocument.object().contains("banned")) {
        bannedUrls.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("banned").toArray()) {
            bannedUrls.append(QUrl(jsonValue.toString()));
        }
    }
    if (jsonDocument.object().contains("loved")) {
        lovedUrls.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("loved").toArray()) {
            lovedUrls.append(QUrl(jsonValue.toString()));
        }
    }
    if (jsonDocument.object().contains("unable_to_start")) {
        unableToStartUrls.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("unable_to_start").toArray()) {
            QJsonArray value = jsonValue.toArray();
            unableToStartUrls.append({ QUrl(value.at(0).toString()), (qint64)value.at(1).toDouble() });
        }
    }
}


// get the key (this simple little "decryption" is here just so that the key is not included in the source code in plain text format)
//
// WARNING! The key is exempt from the terms of the GNU General Public License. Usage of the key is resticted to the developers of Waver.
//          Please get your own key if you need one, it's free. Check 'https://www.shoutcast.com/Developer' for details.
//
QString RadioSource::getKey()
{
    QFile keyFile("://rs_key");
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
void RadioSource::genreSearch()
{
    if (genreSearchItems.count() < 1) {
        return;
    }

    // have to wait for Idle
    if (state != Idle) {
        QTimer::singleShot(NETWORK_WAIT_MS, this, SLOT(genreSearch()));
        return;
    }

    setState(genreSearchItems.first().state);

    // send request
    QNetworkRequest request(QUrl("http://api.shoutcast.com/legacy/genresearch?k=" + key + "&genre=" + genreSearchItems.first().genreName));
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    networkAccessManager->get(request);
}


// timer slot
void RadioSource::stationSearch()
{
    // have to wait for Idle
    if (state != Idle) {
        QTimer::singleShot(NETWORK_WAIT_MS, this, SLOT(stationSearch()));
        return;
    }

    setState(Searching);

    // send request
    QNetworkRequest request(QUrl("http://api.shoutcast.com/legacy/stationsearch?k=" + key + "&search=" + stationSearchCriteria));
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    networkAccessManager->get(request);
}


// network signal handler
void RadioSource::networkFinished(QNetworkReply *reply)
{
    // make sure reply OK
    if (reply->error() != QNetworkReply::NoError) {
        emit infoMessage(id, QString("Network error: %1").arg(reply->errorString()));
        setState(Idle);
        reply->deleteLater();
        return;
    }
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 200) {
        emit infoMessage(id, QString("SHOUTcast Radio Directory service returned status %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString()));
        setState(Idle);
        reply->deleteLater();
        return;
    }

    bool         genresCleaned = false;
    bool         searchFind    = false;
    QString      base;
    QStringList  binds;
    QVariantList values;

    QXmlStreamReader xmlStreamReader(reply);
    while (!xmlStreamReader.atEnd()) {
        QXmlStreamReader::TokenType tokenType = xmlStreamReader.readNext();

        if (tokenType == QXmlStreamReader::StartElement) {
            // genre query
            if (!genresCleaned && (xmlStreamReader.name().toString().compare("genrelist") == 0)) {
                genresCleaned = true;
                genres.clear();
            }
            if (xmlStreamReader.name().toString().compare("genre") == 0) {
                QXmlStreamAttributes attributes = xmlStreamReader.attributes();
                if ((attributes.hasAttribute("parentid")) && (attributes.hasAttribute("name"))) {
                    genres.append({ attributes.value("name").toString(), attributes.value("parentid").toString().compare("0") == 0 });
                }
            }

            // stations query or search
            if (xmlStreamReader.name().toString().compare("tunein") == 0) {
                QXmlStreamAttributes attributes = xmlStreamReader.attributes();
                if (attributes.hasAttribute("base")) {
                    base = attributes.value("base").toString();
                }
            }
            if (xmlStreamReader.name().toString().compare("station") == 0) {
                QXmlStreamAttributes attributes = xmlStreamReader.attributes();

                if (state == Searching) {
                    if (attributes.hasAttribute("id") && attributes.hasAttribute("name")) {
                        QString genre = "Unknown";
                        if (attributes.hasAttribute("genre")) {
                            genre = attributes.value("genre").toString();
                        }

                        binds.append("(?,?,?,?,?)");

                        values.append(attributes.value("id").toString());
                        values.append(base);
                        values.append(attributes.value("name").toString());
                        values.append(genre);
                        values.append(stationSearchCriteria);

                        // can't have too many in one single statement
                        if (values.count() >= 750) {
                            searchFind = true;
                            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "INSERT INTO search (id, base, name, genre, criteria) VALUES " + binds.join(","), values);
                            binds.clear();
                            values.clear();
                        }
                    }
                    continue;
                }

                if (attributes.hasAttribute("id") && attributes.hasAttribute("name") && attributes.hasAttribute("genre")) {
                    QString genre = attributes.value("genre").toString();
                    QString logo  = "";
                    if (attributes.hasAttribute("logo")) {
                        logo = attributes.value("logo").toString();
                    }
                    if (genre.compare(genreSearchItems.first().genreName) == 0) {
                        binds.append("(?,?,?,?,?)");

                        values.append(attributes.value("id").toString());
                        values.append(base);
                        values.append(attributes.value("name").toString());
                        values.append(genre);
                        values.append(logo);

                        // can't have too many in one single statement
                        if (values.count() >= 750) {
                            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO stations (id, base, name, genre, logo) VALUES " + binds.join(","), values);
                            binds.clear();
                            values.clear();
                        }
                    }
                }
            }
        }
    }
    if (xmlStreamReader.hasError()) {
        emit infoMessage(id, "Error while parsing XML response from SHOUTcast Radio Directory service");
        setState(Idle);
        reply->deleteLater();
        return;
    }

    // genre query
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

    // stations query
    if ((state == StationList) || (state == Opening)) {
        // finish saving or fake it
        int clientSqlIdentifier = (state == Opening ? SQL_GENRE_SEARCH_OPENING : SQL_GENRE_SEARCH_STATION_LIST);
        if (values.count() > 0) {
            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", clientSqlIdentifier, "INSERT OR IGNORE INTO stations (id, base, name, genre, logo) VALUES " + binds.join(","), values);
        }
        else {
            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", clientSqlIdentifier, "SELECT CURRENT_TIMESTAMP", QVariantList());
        }

        // maintenance
        if (state == Opening) {
            openGenreSearchGenre = genreSearchItems.first().genreName;
        }
        stationsLoaded.insert(genreSearchItems.first().genreName, QDateTime::currentDateTime());
        genreSearchItems.removeFirst();

        setState(Idle);

        QTimer::singleShot(NETWORK_WAIT_MS, this, SLOT(genreSearch()));

        return;
    }

    // stations search query
    if (state == Searching) {
        // finish saving or fake it
        if (values.count() > 0) {
            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_STATION_SEARCH_OPENING, "INSERT INTO search (id, base, name, genre, criteria) VALUES " + binds.join(","), values);
        }
        else {
            if (searchFind) {
                emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_STATION_SEARCH_OPENING, "SELECT CURRENT_TIMESTAMP", QVariantList());
            }
            else {
                values.append("NOTHING");
                values.append("NOTHING");
                values.append("NOTHING");
                values.append("NOTHING");
                values.append(stationSearchCriteria);
                emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_STATION_SEARCH_OPENING, "INSERT INTO search (id, base, name, genre, criteria) VALUES (?,?,?,?,?)", values);
            }
        }
        setState(Idle);
        return;
    }
}


// timer signal handler
void RadioSource::tuneInStarter()
{
    // already tuning in?
    if (state == TuneIn) {
        return;
    }

    // have to wait for Idle
    if (state != Idle) {
        QTimer::singleShot(NETWORK_WAIT_MS, this, SLOT(tuneInStarter()));
        return;
    }

    setState(TuneIn);
    tuneIn();
}


// timer signal handler
void RadioSource::tuneIn()
{
    // just to be on the safe side, this should never happen
    if (state != TuneIn) {
        emit infoMessage(id, "Invalid state");
        setState(Idle);
        return;
    }

    // check if there's anything without URL
    int tuneInTempIndex = -1;
    int i = 0;
    while ((tuneInTempIndex < 0) && (i < tuneInTemp.count())) {
        if (tuneInTemp.at(i).url.isEmpty()) {
            tuneInTempIndex = i;
        }
        i++;
    }

    // no more stations to tune in to
    if (tuneInTempIndex < 0) {
        // send stations to player
        TracksInfo tracksInfo;
        ExtraInfo  extraInfo;
        foreach (StationTemp station, tuneInTemp) {
            int lovedLongplay = 0;

            TrackInfo trackInfo;
            trackInfo.album     = station.genre;
            trackInfo.cast      = true;
            trackInfo.performer = "Online Radio";
            trackInfo.title     = station.name;
            trackInfo.track     = 0;
            trackInfo.url       = station.url;
            trackInfo.year      = 0;
            trackInfo.pictures.append(station.logo.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wp_radiosource.png" : station.logo);
            if (bannedUrls.contains(station.url)) {
                trackInfo.actions.append({ id, 1, "Remove ban" });
            }
            else {
                trackInfo.actions.append({ id, 0, "Ban" });
            }
            if (lovedUrls.contains(station.url)) {
                trackInfo.actions.append({ id, 11, "Unlove" });
                lovedLongplay = 1;
            }
            else {
                trackInfo.actions.append({ id, 10, "Love" });
            }

            emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "UPDATE stations SET playcount = playcount + 1 WHERE url = ?", QVariantList({ station.url.toString() }));

            if ((station.destination == Playlist) || (station.destination == Open) || (station.destination == Search)) {
                tracksInfo.append(trackInfo);
                extraInfo.insert(trackInfo.url, { {"loved", station.lovedMode}, {"loved_longplay", lovedLongplay} });
            }
            if (station.destination == Replacement) {
                emit replacement(id, trackInfo);
            }
        }
        if (tracksInfo.count() > 0) {
            emit playlist(id, tracksInfo, extraInfo);
        }

        tuneInTemp.clear();

        setState(Idle);

        // this is a good place to do some maintenance because here it's reasonable to expect that the plugin will be idle for a while
        maintenance();

        return;
    }

    // request the playlist for this station
    QNetworkRequest request(QUrl("http://yp.shoutcast.com" + tuneInTemp.at(tuneInTempIndex).base + "?id=" + tuneInTemp.at(tuneInTempIndex).id));
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    playlistAccessManager->get(request);

}


// network signal handler
void RadioSource::playlistFinished(QNetworkReply *reply)
{
    // make sure reply OK
    if (reply->error() != QNetworkReply::NoError) {
        emit infoMessage(id, QString("Network error"));
        setState(Idle);
        reply->deleteLater();
        return;
    }
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 200) {
        emit infoMessage(id, QString("SHOUTcast Radio Directory service returned status %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString()));
        setState(Idle);
        reply->deleteLater();
        return;
    }

    // get the id of the station
    QString stationId;
    QString query = reply->url().query();
    if (query.contains("id=")) {
        // id was passed last
        stationId = query.mid(query.indexOf("id=") + QString("id=").length());
    }
    if (stationId.isEmpty()) {
        emit infoMessage(id, "Station id not found in URL query");
        setState(Idle);
        reply->deleteLater();
        return;
    }

    // get the playlist
    QString stationPlaylist(reply->readAll());
    reply->deleteLater();

    // get the urls
    int i = 1;
    QVector<QUrl> urls;
    QRegExp       regExp;
    regExp.setMinimal(true);
    regExp.setPattern(QString("File%1=(.+)\n").arg(i));
    while (stationPlaylist.contains(regExp)) {
        urls.append(QUrl(regExp.capturedTexts().at(1)));

        i++;
        regExp.setPattern(QString("File%1=(.+)\n").arg(i));
    }

    // select one url
    QUrl selectedUrl;
    foreach (QUrl url, urls) {
        if (selectedUrl.isEmpty()) {
            selectedUrl = url;
        }
        else if (!isUnableToStartUrl(url)) {
            selectedUrl = url;
        }
    }

    bool isBanned        = bannedUrls.contains(selectedUrl);
    bool isUnableToStart = selectedUrl.isEmpty() || isUnableToStartUrl(selectedUrl);

    // update list
    i                      = 0;
    int deletedPlaylist    = 0;
    int deletedReplacement = 0;
    while (i < tuneInTemp.count()) {
        if (tuneInTemp.at(i).id.compare(stationId) == 0) {
            // must delete and if url can not be used
            if (isBanned || isUnableToStart) {
                if (tuneInTemp.at(i).destination == Playlist) {
                    deletedPlaylist++;
                    tuneInTemp.remove(i);
                    continue;
                }
                if (tuneInTemp.at(i).destination == Replacement) {
                    deletedReplacement++;
                    tuneInTemp.remove(i);
                    continue;
                }
            }

            // otherwise it can be used
            tuneInTemp[i].url = selectedUrl;
        }
        i++;
    }

    // update database
    QVariantList values;
    values.append(selectedUrl.isEmpty() ? NULL : selectedUrl.toString());
    values.append(isBanned);
    values.append(lovedUrls.contains(selectedUrl));
    values.append(isUnableToStart);
    values.append(stationId);
    emit executeGlobalSql(id, SQL_TEMPORARY_DB, QString("%1,%2").arg(deletedPlaylist).arg(deletedReplacement), SQL_STATION_UPDATED_PLAYLIST, "UPDATE stations SET url = ?, banned = ?, loved = ?, unable_to_start = ? WHERE id = ?", values);
    QVariantList searchValues;
    searchValues.append(selectedUrl.isEmpty() ? NULL : selectedUrl.toString());
    searchValues.append(stationId);
    emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "UPDATE search SET url = ? WHERE id = ?", searchValues);

    // check the next station's URL after a bit of a delay so not to run too many requests rapidly
    QTimer::singleShot(PLAYLIST_REQUEST_DELAY_MS, this, SLOT(tuneIn()));
}


// helper
void RadioSource::setState(State state)
{
    this->state = state;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// helper
void RadioSource::removeExpiredUnableToStartUrls()
{
    QVector<UnableToStartUrl> cleaned;
    foreach (UnableToStartUrl unableToStartUrl, unableToStartUrls) {
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(unableToStartUrl.timestamp);
        if (dateTime.daysTo(QDateTime::currentDateTime()) < UNABLE_TO_START_EXPIRY_DAYS) {
            cleaned.append(unableToStartUrl);
        }
    }
    unableToStartUrls = cleaned;
}


// helper
bool RadioSource::isUnableToStartUrl(QUrl url)
{
    bool returnValue = false;

    int i = 0;
    while (!returnValue && (i < unableToStartUrls.count())) {
        if (url == unableToStartUrls.at(i).url) {
            returnValue = true;
        }
        i++;
    }

    return returnValue;
}


// helper
void RadioSource::maintenance()
{
    // make sure search table doesn't grow out of control
    emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_SEARCH_COUNT, "SELECT COUNT(*) AS counter FROM search", QVariantList());

    // let's see if there are expired genres
    QStringList stationsToBeReloaded;
    foreach (QString genreName, stationsLoaded.keys()) {
        if (stationsLoaded.value(genreName).secsTo(QDateTime::currentDateTime()) > (GENRE_EXPIRY_HOURS * 60 * 60)) {
            stationsToBeReloaded.append(genreName);
        }
    }
    foreach (QString genreName, stationsToBeReloaded) {
        emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_NO_RESULTS, "DELETE FROM stations WHERE genre = ?", QVariantList({ genreName }));
        stationsLoaded.remove(genreName);
    }

    // let's see if there's a genre that needs to be loaded
    QStringList genresToBeLoaded;
    foreach (QString selectedGenre, selectedGenres) {
        if (!stationsLoaded.contains(selectedGenre)) {
            genresToBeLoaded.append(selectedGenre);
        }
    }
    if (genresToBeLoaded.count() > 0) {
        genreSearchItems.append({ genresToBeLoaded.at(qrand() % genresToBeLoaded.count()), StationList });
        genreSearch();
    }
}


// helper
void RadioSource::sendDiagnosticsData()
{
    emit executeGlobalSql(id, SQL_TEMPORARY_DB, "", SQL_DIAGNOSTICS, "SELECT genre, COUNT(*) AS counter FROM stations GROUP BY genre", QVariantList());
}
