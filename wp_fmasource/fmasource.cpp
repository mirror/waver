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
    id  = QUuid("{A41DEA06-4B1E-4C3F-8108-76EB29546089}");
    key = getKey();

    networkAccessManager = nullptr;
    readySent            = false;
    okToSendReady        = true;
    sendDiagnostics      = false;
    state                = Idle;
    unableToStartCount   = 0;
}


// destructor
FMASource::~FMASource()
{
    if (networkAccessManager != nullptr) {
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
    return "Free Music";
}


// overridden virtual function
int FMASource::pluginVersion()
{
    return 2;
}


// overrided virtual function
QString FMASource::waverVersionAPICompatibility()
{
    return "0.0.6";
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

    emit executeGlobalSql(id, false, "", SQL_CREATETABLE_GENRES, "CREATE TABLE IF NOT EXISTS genres (id INT PRIMARY KEY, parent_id INT, handle, name)", QVariantList());
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
        readySent     = false;
        okToSendReady = false;
        emit unready(id);
        return;
    }

    loadMore();
}


// slot receiving configuration
void FMASource::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
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

    if (clientSqlIdentifier == SQL_CREATETABLE_GENRES) {
        emit executeGlobalSql(id, false, "", SQL_CREATETABLE_ALBUMS, "CREATE TABLE IF NOT EXISTS albums (id INT PRIMARY KEY, genre_id INT, album, performer, year INT)", QVariantList());
        return;
    }

    if (clientSqlIdentifier == SQL_CREATETABLE_ALBUMS) {
        emit executeGlobalSql(id, false, "", SQL_CREATETABLE_TRACKS, "CREATE TABLE IF NOT EXISTS tracks (id INT PRIMARY KEY, album_id INT, title, performer, url, picture_url, track INT, playcount INT DEFAULT 0)", QVariantList());
        return;
    }

    if (clientSqlIdentifier == SQL_CREATETABLE_TRACKS) {
        emit executeGlobalSql(id, false, "", SQL_CREATETABLE_BANNED, "CREATE TABLE IF NOT EXISTS banned (track_id INT PRIMARY KEY)", QVariantList());
    }

    if (clientSqlIdentifier == SQL_CREATETABLE_BANNED) {
        emit executeGlobalSql(id, false, "", SQL_CREATETABLE_LOVED, "CREATE TABLE IF NOT EXISTS loved (track_id INT PRIMARY KEY)", QVariantList());
        return;
    }

    if (clientSqlIdentifier == SQL_CREATETABLE_LOVED) {
        emit executeGlobalSql(id, false, "", SQL_U1_CHECK, "SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'tracks_genres'", QVariantList());
        return;
    }

    if (clientSqlIdentifier == SQL_U1_CHECK) {
        if (results.count() > 0) {
            emit executeGlobalSql(id, false, "", SQL_STARTUPCHECK_GENRECOUNT, "SELECT count(*) AS counter FROM genres", QVariantList());
            return;
        }

        emit executeGlobalSql(id, false, "", SQL_U1_CREATETABLE_TRACKSGENRES, "CREATE TABLE tracks_genres (track_id INT, genre_id INT, PRIMARY KEY (track_id, genre_id))", QVariantList());
        return;
    }

    if (clientSqlIdentifier == SQL_U1_CREATETABLE_TRACKSGENRES) {
        emit executeGlobalSql(id, false, "", SQL_U1_EMPTY_ALBUMS, "DELETE FROM albums", QVariantList());
        return;
    }

    if (clientSqlIdentifier == SQL_U1_EMPTY_ALBUMS) {
        emit executeGlobalSql(id, false, "", SQL_U1_EMPTY_TRACKS, "DELETE FROM tracks", QVariantList());
        return;
    }

    if (clientSqlIdentifier == SQL_U1_EMPTY_TRACKS) {
        emit executeGlobalSql(id, false, "", SQL_STARTUPCHECK_GENRECOUNT, "SELECT count(*) AS counter FROM genres", QVariantList());
        return;
    }

    if (clientSqlIdentifier == SQL_STARTUPCHECK_GENRECOUNT) {
        if (results.at(0).value("counter").toInt() < 1) {
            setState(GenreList);

            QNetworkRequest request(QUrl("https://freemusicarchive.org/api/get/genres.xml?api_key=" + key + "&limit=50"));
            request.setRawHeader("User-Agent", userAgent.toUtf8());
            networkAccessManager->get(request);

            return;
        }

        emit loadConfiguration(id);
        return;
    }

    if (clientSqlIdentifier == SQL_STARTUPCHECK_GENRESLOADED) {
        emit loadConfiguration(id);
        return;
    }

    if (clientSqlIdentifier == SQL_COLLECTIONCHECK_GENRE) {
        if (results.count() > 0) {
            selectedGenres.append(results.at(0).value("id").toInt());
        }
    }

    if (clientSqlIdentifier == SQL_LOADMORE_GENRESWITHOUTALBUM) {
        if (results.count() > 0) {
            foreach (QVariantHash result, results) {
                genreSearchItems.append({ result.value("id").toInt(), result.value("handle").toString(), AlbumList });
            }
            genreSearch();
            return;
        }

        SqlResults fakeResults;
        globalSqlResults(id, false, "", SQL_LOADMORE_ALBUMSLOADED, fakeResults);
        return;
    }

    if (clientSqlIdentifier == SQL_LOADMORE_ALBUMSLOADED) {
        QString      binds;
        QVariantList values;
        selectedGenresBinds(&binds, &values);
        emit executeGlobalSql(id, false, "", SQL_LOADMORE_ALBUMSWITHOUTTRACKS, "SELECT id FROM albums WHERE (genre_id IN (" + binds + ")) AND (id NOT IN (SELECT album_id FROM tracks GROUP BY album_id)) ORDER BY RANDOM() LIMIT 3", values);
        return;
    }

    if (clientSqlIdentifier == SQL_LOADMORE_ALBUMSWITHOUTTRACKS) {
        if (results.count() > 0) {
            foreach (QVariantHash result, results) {
                albumSearchItems.append({ result.value("id").toInt(), TrackList });
            }
            albumSearch();
            return;
        }

        // TODO here should be something to load newly added albums from FMA

        SqlResults fakeResults;
        globalSqlResults(id, false, "", SQL_LOADMORE_TRACKSLOADED, fakeResults);
        return;
    }

    if (clientSqlIdentifier == SQL_LOADMORE_TRACKSLOADED) {
        if (!readySent && okToSendReady) {
            readySent = true;
            emit ready(id);
        }
        return;
    }

    if (clientSqlIdentifier == SQL_GET_LOVED) {
        if (results.count() > 0) {
            // it's always LIMIT 1, no loop needed
            TrackInfo lovedTrack = sqlResultToTrackInfo(results.at(0));

            TracksInfo tracksInfo;
            tracksInfo.append(lovedTrack);

            ExtraInfo  extraInfo;
            extraInfo.insert(lovedTrack.url, { { "loved", results.at(0).value("mode").toInt() } });

            emit playlist(id, tracksInfo, extraInfo);
        }
        else {
            getPlaylist(id, 1, PLAYLIST_MODE_NORMAL);
        }
        return;
    }

    if (clientSqlIdentifier == SQL_GET_PLAYLIST) {
        TracksInfo tracksInfo;
        ExtraInfo  extraInfo;

        foreach (QVariantHash result, results) {
            TrackInfo trackInfo = sqlResultToTrackInfo(result);
            tracksInfo.append(trackInfo);
            extraInfo.insert(trackInfo.url, { { "resolved_open_track", result.value("resolved_open_track").toInt() } });

            emit executeGlobalSql(id, false, "", SQL_NO_RESULTS, "UPDATE tracks SET playcount = playcount + 1 WHERE id = ?", QVariantList({ result.value("id").toInt() }));
        }

        emit playlist(id, tracksInfo, extraInfo);

        loadMore();

        return;
    }

    if (clientSqlIdentifier == SQL_GET_REPLACEMENT) {
        if (results.count() > 0) {
            TrackInfo trackInfo;
            trackInfo.album     = results.at(0).value("album").toString();
            trackInfo.cast      = false;
            trackInfo.performer = (results.at(0).value("performer").toString().compare(results.at(0).value("album_performer").toString()) == 0 ? results.at(0).value("performer").toString() : results.at(0).value("performer").toString() + "\n(" + results.at(0).value("album_performer").toString() + ")");
            trackInfo.title     = results.at(0).value("title").toString();
            trackInfo.track     = results.at(0).value("track").toInt();
            trackInfo.url       = QUrl(results.at(0).value("url").toString());
            trackInfo.year      = results.at(0).value("year").toInt();
            trackInfo.actions.append({ id, 0, "Ban"});
            trackInfo.actions.append({ id, 1, "Ban album" });
            if (results.at(0).value("loved_track_id").toString().isEmpty()) {
                trackInfo.actions.append({ id, 10, "Love" });
            }
            else  {
                trackInfo.actions.append({ id, 11, "Unlove" });
            }
            trackInfo.actions.append({ id, 20, "Track info" });
            trackInfo.pictures.append(QUrl(results.at(0).value("picture_url").toString().toUtf8()));

            emit executeGlobalSql(id, false, "", SQL_NO_RESULTS, "UPDATE tracks SET playcount = playcount + 1 WHERE id = ?", QVariantList({ results.at(0).value("id").toInt() }));

            emit replacement(id, trackInfo);
        }
    }

    if (clientSqlIdentifier == SQL_OPEN_TOPLEVEL) {
        OpenTracks openTracks;
        foreach (QVariantHash result, results) {
            OpenTrack openTrack;
            openTrack.hasChildren = true;
            openTrack.id          = QString("G~%1").arg(result.value("id").toInt());
            openTrack.label       = result.value("name").toString();
            openTrack.selectable = false;
            openTracks.append(openTrack);
        }
        emit openTracksResults(id, openTracks);
        return;
    }

    if (clientSqlIdentifier == SQL_OPEN_PERFORMERS) {
        if (results.count() > 0) {
            OpenTracks openTracks;
            foreach (QVariantHash result, results) {
                OpenTrack openTrack;
                openTrack.hasChildren = true;
                openTrack.id          = "P~" + result.value("performer").toString();
                openTrack.label       = result.value("performer").toString();
                openTrack.selectable  = false;
                openTracks.append(openTrack);
            }
            emit openTracksResults(id, openTracks);
            return;
        }
        if (openingId > 0) {
            emit executeGlobalSql(id, false, "", SQL_OPEN_PERFORMERS_GENRESEARCH, "SELECT id, handle FROM genres WHERE id = ?", QVariantList({ openingId }));
        }
        return;
    }

    if (clientSqlIdentifier == SQL_OPEN_PERFORMERS_GENRESEARCH) {
        genreSearchItems.append({ results.at(0).value("id").toInt(), results.at(0).value("handle").toString(), OpeningPerformersAlbums });
        genreSearch();
        return;
    }

    if (clientSqlIdentifier == SQL_OPEN_PERFORMERS_LOADED) {
        int genreId = openingId;
        openingId = 0;
        emit executeGlobalSql(id, false, "", SQL_OPEN_PERFORMERS, "SELECT performer FROM albums WHERE genre_id = ? GROUP BY performer", QVariantList({ genreId }));
        return;
    }

    if (clientSqlIdentifier == SQL_OPEN_ALBUMS) {
        OpenTracks openTracks;
        foreach (QVariantHash result, results) {
            OpenTrack openTrack;
            openTrack.hasChildren = true;
            openTrack.id          = QString("A~%1").arg(result.value("id").toInt());
            openTrack.label       = result.value("album").toString();
            openTrack.selectable  = false;
            openTracks.append(openTrack);
        }
        emit openTracksResults(id, openTracks);
        return;
    }

    if (clientSqlIdentifier == SQL_OPEN_TRACKS) {
        if (results.count() > 0) {
            OpenTracks openTracks;
            foreach (QVariantHash result, results) {
                OpenTrack openTrack;
                openTrack.hasChildren = false;
                openTrack.id          = QString("T~%1").arg(result.value("id").toInt());
                openTrack.label       = result.value("title").toString();
                openTrack.selectable  = true;
                openTracks.append(openTrack);
            }
            emit openTracksResults(id, openTracks);
            return;
        }
        if (openingId > 0) {
            albumSearchItems.append({ openingId, OpeningTracks });
            albumSearch();
        }
        return;
    }

    if (clientSqlIdentifier == SQL_OPEN_TRACKS_LOADED) {
        int albumId = openingId;
        openingId = 0;
        emit executeGlobalSql(id, false, "", SQL_OPEN_TRACKS, "SELECT id, title FROM tracks WHERE album_id = ? ORDER BY track, title", QVariantList({ albumId }));
        return;
    }

    if (clientSqlIdentifier == SQL_SEARCH) {
        if (results.count() > 0) {
            OpenTracks openTracks;
            foreach (QVariantHash result, results) {
                OpenTrack openTrack;
                openTrack.hasChildren = false;
                openTrack.id          = QString("T~%1").arg(result.value("id").toInt());
                openTrack.label       = result.value("title").toString() + " (" + result.value("performer").toString() + ")";
                openTrack.selectable  = true;
                openTracks.append(openTrack);
            }
            emit searchResults(id, openTracks);
        }
        return;
    }

    if (clientSqlIdentifier == SQL_UIQML_GENRELIST) {
        QVector<Genre> genres;
        foreach (QVariantHash result, results) {
            Genre genre;
            genre.id       = result.value("id").toInt();
            genre.parentId = result.value("parent_id").toInt();
            genre.handle   = result.value("handle").toString();
            genre.name     = result.value("name").toString();

            genres.append(genre);
        }

        QVector<GenreDisplay> sorted;
        sortGenres(genres, 0, &sorted, 0);

        int marginTotal = 0;
        foreach (GenreDisplay genreDisplay, sorted) {
            marginTotal += (genreDisplay.isTopLevel ? 36 : 6);
        }

        QFile settingsFile("://FMASettings.qml");
        settingsFile.open(QFile::ReadOnly);
        QString settings = settingsFile.readAll();
        settingsFile.close();

        settings.replace("replace_content_height", QString("(%1 * cb_0.height) + %2").arg(genres.count()).arg(marginTotal - 30));

        QString checkboxes;
        QString checkboxesToAll;
        QString checkboxesToRetval;
        for (int i = 0; i < sorted.count(); i++) {
            checkboxes.append(
                QString("CheckBox { id: %1; text: \"%2\"; tristate: true; checkState: %3; anchors.top: %4; anchors.topMargin: %5; anchors.left: parent.left; anchors.leftMargin: %6; foreignId: %7; property int foreignId; }")
                .arg(QString("cb_%1").arg(i))
                .arg(sorted.at(i).isTopLevel ? "<b>" + sorted.at(i).name + "</b>" : sorted.at(i).name)
                .arg(selectedGenres.contains(sorted.at(i).id) ? "Qt.Checked" : deniedGenres.contains(sorted.at(i).id) ? "Qt.PartiallyChecked" : "Qt.Unchecked")
                .arg(i > 0 ? QString("cb_%1.bottom").arg(i - 1) : "parent.top")
                .arg((i > 0) && sorted.at(i).isTopLevel ? "36" : "6")
                .arg(6 + (sorted.at(i).indent * 36))
                .arg(sorted.at(i).id));
            checkboxesToAll.append(QString("%1.checked = allCheck.checked; ").arg(QString("cb_%1").arg(i)));
            checkboxesToRetval.append(QString("if (%1.checkState == Qt.Checked) { retval.selected.push(%1.foreignId); }; if (%1.checkState == Qt.PartiallyChecked) { retval.denied.push(%1.foreignId); };").arg(QString("cb_%1").arg(i)));
        }
        settings.replace("CheckBox {}", checkboxes);
        settings.replace("replace_checkboxes_to_all", checkboxesToAll);
        settings.replace("replace_checkboxes_to_retval", checkboxesToRetval);

        emit uiQml(id, settings);

        return;
    }

    if (clientSqlIdentifier == SQL_TO_BE_REMOVED) {
        foreach (QVariantHash result, results) {
            emit requestRemoveTrack(id, QUrl(result.value("url").toString()));
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
            case AlbumList:
                diagnosticData.append({ "Status", "Getting albums data..." });
                break;
            case TrackList:
                diagnosticData.append({ "Status", "Getting tracks data..." });
                break;
            case OpeningPerformersAlbums:
                diagnosticData.append({ "Status", "Opening performers and albums..." });
                break;
            case OpeningTracks:
                diagnosticData.append({ "Status", "Opening tracks..." });
                break;
        }

        foreach (QVariantHash result, results) {
            DiagnosticItem stationCount;
            stationCount.label   = result.value("genre").toString();
            if (result.value("album_count").toInt() > 0) {
                stationCount.message = QString("Albums: %1, Tracks loaded: %2, banned: %3, not yet loaded: %4 albums").arg(result.value("album_count").toInt(), 4).arg(result.value("track_count").toInt(), 5).arg(result.value("banned_count").toInt(), 4).arg(result.value("albums_not_yet_loaded").toInt(), 4);
            }
            else {
                stationCount.message = "No albums loaded yet";
            }
            diagnosticData.append(stationCount);
        }

        emit diagnostics(id, diagnosticData);
        return;
    }
}


// recursive method
void FMASource::sortGenres(QVector<Genre> genres, int parentId, QVector<GenreDisplay> *sorted, int level)
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

        sortGenres(genres, withParentId.at(i).id, sorted, level);
    }
}


// private method
void FMASource::loadMore()
{
    // start the "load more data" sequence
    QString      binds;
    QVariantList values;
    selectedGenresBinds(&binds, &values);
    emit executeGlobalSql(id, false, "", SQL_LOADMORE_GENRESWITHOUTALBUM, "SELECT id, handle FROM genres WHERE (id IN (" + binds + ")) AND (id NOT IN (SELECT genre_id FROM albums GROUP BY genre_id)) LIMIT 1", values);
}


// helper
void FMASource::selectedGenresBinds(QString *binds, QVariantList *values)
{
    QStringList bindsList;

    foreach (int selectedGenre, selectedGenres) {
        bindsList.append("?");
        values->append(selectedGenre);
    }

    binds->append(bindsList.join(","));
}


// helper
void FMASource::deniedGenresBinds(QString *binds, QVariantList *values)
{
    QStringList bindsList;

    foreach (int deniedGenre, deniedGenres) {
        bindsList.append("?");
        values->append(deniedGenre);
    }

    binds->append(bindsList.join(","));
}


// helper
TrackInfo FMASource::sqlResultToTrackInfo(QVariantHash sqlResult)
{
    TrackInfo returnValue;

    returnValue.album     = sqlResult.value("album").toString();
    returnValue.cast      = false;
    returnValue.performer = (sqlResult.value("performer").toString().compare(sqlResult.value("album_performer").toString()) == 0 ? sqlResult.value("performer").toString() : sqlResult.value("performer").toString() + "\n(" + sqlResult.value("album_performer").toString() + ")");
    returnValue.title     = sqlResult.value("title").toString();
    returnValue.track     = sqlResult.value("track").toInt();
    returnValue.url       = QUrl(sqlResult.value("url").toString());
    returnValue.year      = sqlResult.value("year").toInt();
    returnValue.actions.append({ id, 0, "Ban" });
    returnValue.actions.append({ id, 1, "Ban album" });
    if (sqlResult.value("loved_track_id").toString().isEmpty()) {
        returnValue.actions.append({ id, 10, "Love" });
    }
    else  {
        returnValue.actions.append({ id, 11, "Unlove" });
    }
    returnValue.actions.append({ id, 20, "Track info" });
    returnValue.pictures.append(QUrl(sqlResult.value("picture_url").toString().toUtf8()));

    return returnValue;
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


// message handler
void FMASource::messageFromPlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(sourceUniqueId);
    Q_UNUSED(messageId);
    Q_UNUSED(value);
}


// client wants to display this plugin's configuration dialog
void FMASource::getUiQml(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    emit executeGlobalSql(id, false, "", SQL_UIQML_GENRELIST, "SELECT * FROM genres", QVariantList());
}


// slot receiving configuration dialog results
void FMASource::uiResults(QUuid uniqueId, QJsonDocument results)
{
    if (uniqueId != id) {
        return;
    }

    selectedGenres.clear();
    deniedGenres.clear();

    QJsonArray selected = results.object().value("selected").toArray();
    QJsonArray denied   = results.object().value("denied").toArray();

    foreach (QJsonValue jsonValue, selected) {
        selectedGenres.append(jsonValue.toInt());
    }
    foreach (QJsonValue jsonValue, denied) {
        deniedGenres.append(jsonValue.toInt());
    }

    emit saveConfiguration(id, configToJson());
    emit requestRemoveTracks(id);

    loadMore();
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


// server says unable to start a track
void FMASource::unableToStart(QUuid uniqueId, QUrl url)
{
    Q_UNUSED(url);

    if (uniqueId != id) {
        return;
    }

    unableToStartCount++;
    if (readySent && (unableToStartCount >= 4)) {
        readySent     = false;
        okToSendReady = false;
        emit unready(id);

        QTimer::singleShot(10 * 60 * 1000, this, SLOT(resumeAfterTooManyUnableToStart()));
    }
}


// timer slot
void FMASource::resumeAfterTooManyUnableToStart()
{
    unableToStartCount = 0;
    readySent          = true;
    okToSendReady      = true;
    emit ready(id);
}


// this should never happen because of no casts
void FMASource::castFinishedEarly(QUuid uniqueId, QUrl url, int playedSeconds)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(url);
    Q_UNUSED(playedSeconds);
}


// server is done with a track
void FMASource::done(QUuid uniqueId, QUrl url, bool wasError)
{
    Q_UNUSED(url);

    if (uniqueId != id) {
        return;
    }

    if (!wasError) {
        unableToStartCount = 0;
    }
}


// reuest for playlist entries
void FMASource::getPlaylist(QUuid uniqueId, int trackCount, int mode)
{
    if (uniqueId != id) {
        return;
    }

    if (mode == PLAYLIST_MODE_LOVED) {
        QString      selectedBinds;
        QString      deniedBinds;
        QVariantList values;
        selectedGenresBinds(&selectedBinds, &values);
        deniedGenresBinds(&deniedBinds, &values);
        values.prepend(mode);

        emit executeGlobalSql(id, false, "", SQL_GET_LOVED, "SELECT ? AS mode, tracks.id, albums.performer AS album_performer, tracks.performer, album, title, url, picture_url, track, year, loved.track_id AS loved_track_id FROM tracks LEFT JOIN albums ON tracks.album_id = albums.id LEFT JOIN loved ON tracks.id = loved.track_id WHERE (genre_id IN (" + selectedBinds + ")) AND (tracks.id NOT IN (SELECT track_id FROM banned)) AND (tracks.id IN (SELECT track_id FROM loved)) AND (tracks.id NOT IN (SELECT track_id FROM tracks_genres WHERE (track_id = tracks.id) AND (genre_id IN (" + deniedBinds + ")))) ORDER BY RANDOM() LIMIT 1", values);
        trackCount--;
    }
    else if (mode == PLAYLIST_MODE_LOVED_SIMILAR) {
        QString      selectedBinds;
        QString      deniedBinds;
        QVariantList values;
        selectedGenresBinds(&selectedBinds, &values);
        deniedGenresBinds(&deniedBinds, &values);
        values.prepend(mode);

        emit executeGlobalSql(id, false, "", SQL_GET_LOVED, "SELECT ? AS mode, tracks.id, albums.performer AS album_performer, tracks.performer, album, title, url, picture_url, track, year, loved.track_id AS loved_track_id FROM tracks LEFT JOIN albums ON tracks.album_id = albums.id LEFT JOIN loved ON tracks.id = loved.track_id WHERE (genre_id IN (" + selectedBinds + ")) AND (tracks.id NOT IN (SELECT track_id FROM banned)) AND (tracks.id NOT IN (SELECT track_id FROM loved)) AND (tracks.album_id IN (SELECT DISTINCT album_id FROM tracks AS album_lookup WHERE album_lookup.id IN (SELECT track_id FROM loved))) AND (tracks.id NOT IN (SELECT track_id FROM tracks_genres WHERE (track_id = tracks.id) AND (genre_id IN (" + deniedBinds + ")))) ORDER BY RANDOM() LIMIT 1", values);
        trackCount--;
    }

    if (trackCount > 0)    {
        QString      selectedBinds;
        QString      deniedBinds;
        QVariantList values;
        selectedGenresBinds(&selectedBinds, &values);
        deniedGenresBinds(&deniedBinds, &values);
        values.append(trackCount);

        emit executeGlobalSql(id, false, "", SQL_GET_PLAYLIST, "SELECT 0 AS resolved_open_track, tracks.id, albums.performer AS album_performer, tracks.performer, album, title, url, picture_url, track, year, loved.track_id AS loved_track_id FROM tracks LEFT JOIN albums ON tracks.album_id = albums.id LEFT JOIN loved ON tracks.id = loved.track_id WHERE (genre_id IN (" + selectedBinds + ")) AND (tracks.id NOT IN (SELECT track_id FROM banned)) AND (tracks.id NOT IN (SELECT track_id FROM tracks_genres WHERE (track_id = tracks.id) AND (genre_id IN (" + deniedBinds + ")))) ORDER BY playcount, RANDOM() LIMIT ?", values);
    }

    return;
}


// get replacement for a track that could not start
void FMASource::getReplacement(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    QString      binds;
    QVariantList values;
    selectedGenresBinds(&binds, &values);
    emit executeGlobalSql(id, false, "", SQL_GET_REPLACEMENT, "SELECT tracks.id, albums.performer AS album_performer, tracks.performer, album, title, url, picture_url, track, year, loved.track_id AS loved_track_id FROM tracks LEFT JOIN albums ON tracks.album_id = albums.id LEFT JOIN loved ON tracks.id = loved.track_id WHERE (genre_id IN (" + binds + ")) AND (tracks.id NOT IN (SELECT track_id FROM banned)) ORDER BY playcount, RANDOM() LIMIT 1", values);
}


// client wants to dispaly open dialog
void FMASource::getOpenTracks(QUuid uniqueId, QString parentId)
{
    if (uniqueId != id) {
        return;
    }

    // top level
    if (parentId == 0) {
        // get selected genres from database
        QString      binds;
        QVariantList values;
        selectedGenresBinds(&binds, &values);
        emit executeGlobalSql(id, false, "", SQL_OPEN_TOPLEVEL, "SELECT id, name FROM genres WHERE id IN (" + binds + ") ORDER BY name", values);
        return;
    }

    // genre selected, check performers
    if (parentId.startsWith("G~")) {
        int genreId;
        if (stringToInt(parentId.mid(2), &genreId)) {
            openingId = genreId;
            emit executeGlobalSql(id, false, "", SQL_OPEN_PERFORMERS, "SELECT performer FROM albums WHERE genre_id = ? GROUP BY performer", QVariantList({ genreId }));
        }
        return;
    }

    // performer selected, albums must be there already (been loaded when opening performers)
    if (parentId.startsWith("P~")) {
        emit executeGlobalSql(id, false, "", SQL_OPEN_ALBUMS, "SELECT id, album FROM albums WHERE performer = ? ORDER BY album", QVariantList({ parentId.mid(2) }));
        return;
    }

    // album selected, check tracks
    if (parentId.startsWith("A~")) {
        int albumId;
        if (stringToInt(parentId.mid(2), &albumId)) {
            openingId = albumId;
            emit executeGlobalSql(id, false, "", SQL_OPEN_TRACKS, "SELECT id, title FROM tracks WHERE album_id = ? ORDER BY track, title", QVariantList({ albumId }));
        }
    }
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

    QStringList  binds;
    QVariantList values;
    foreach (QString selectedTrack, selectedTracks) {
        binds.append("?");
        values.append(selectedTrack.mid(2));
    }

    if (values.count() > 0) {
        emit executeGlobalSql(id, false, "", SQL_GET_PLAYLIST, "SELECT 1 AS resolved_open_track, tracks.id, albums.performer AS album_performer, tracks.performer, album, title, url, picture_url, track, year, loved.track_id AS loved_track_id FROM tracks LEFT JOIN albums ON tracks.album_id = albums.id LEFT JOIN loved ON tracks.id = loved.track_id WHERE tracks.id IN (" + binds.join(",") + ")", values);
    }
}


// request for playlist entries based on search criteria
void FMASource::search(QUuid uniqueId, QString criteria)
{
    if (uniqueId != id) {
        return;
    }

    // TODO change this to use FMA's search service
    criteria = "%" + criteria.toLower() + "%";
    emit executeGlobalSql(id, false, "", SQL_SEARCH, "SELECT id, performer, title FROM tracks WHERE (performer LIKE ?) OR (title LIKE ?) ORDER BY title", QVariantList({ criteria, criteria }));
}


// user clicked action that was included in track info
void FMASource::action(QUuid uniqueId, int actionKey, TrackInfo trackInfo)
{
    if (uniqueId != id) {
        return;
    }

    if (actionKey == 0) {
        emit executeGlobalSql(id, false, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO banned (track_id) SELECT id FROM tracks WHERE url = ?", QVariantList({ trackInfo.url.toString() }));
        emit requestRemoveTrack(id, trackInfo.url);
    }

    if (actionKey == 1) {
        emit executeGlobalSql(id, false, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO banned (track_id) SELECT id FROM tracks WHERE album_id = (SELECT album_id FROM tracks WHERE url = ?)", QVariantList({ trackInfo.url.toString() }));
        emit executeGlobalSql(id, false, "", SQL_TO_BE_REMOVED, "SELECT url FROM tracks WHERE album_id = (SELECT album_id FROM tracks WHERE url = ?)", QVariantList({ trackInfo.url.toString() }));
    }

    if (actionKey == 10) {
        emit executeGlobalSql(id, false, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO loved (track_id) SELECT id FROM tracks WHERE url = ?", QVariantList({ trackInfo.url.toString() }));

        TrackInfo trackInfoTemp;
        trackInfoTemp.track = 0;
        trackInfoTemp.year  = 0;
        trackInfoTemp.url   = trackInfo.url;
        trackInfoTemp.actions.append({ id, 0, "Ban" });
        trackInfoTemp.actions.append({ id, 1, "Ban album" });
        trackInfoTemp.actions.append({ id, 11, "Unlove" });
        trackInfoTemp.actions.append({ id, 20, "Track info" });
        emit updateTrackInfo(id, trackInfoTemp);
    }

    if (actionKey == 11) {
        emit executeGlobalSql(id, false, "", SQL_NO_RESULTS, "DELETE FROM loved WHERE track_id IN (SELECT id FROM tracks WHERE url = ?)", QVariantList({ trackInfo.url.toString() }));

        TrackInfo trackInfoTemp;
        trackInfoTemp.track = 0;
        trackInfoTemp.year  = 0;
        trackInfoTemp.url   = trackInfo.url;
        trackInfoTemp.actions.append({ id, 0, "Ban" });
        trackInfoTemp.actions.append({ id, 1, "Ban album" });
        trackInfoTemp.actions.append({ id, 10, "Love" });
        trackInfoTemp.actions.append({ id, 20, "Track info" });
        emit updateTrackInfo(id, trackInfoTemp);
    }

    if (actionKey == 20) {
        emit openUrl(QUrl(trackInfo.url.toString().replace(QRegExp("/download$"), "")));
    }

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
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

    // set state
    setState(genreSearchItems.first().state);

    // send request
    QNetworkRequest request(QUrl("https://freemusicarchive.org/api/get/albums.xml?api_key=" + key + "&genre_handle=" + genreSearchItems.first().genreHandle + "&limit=50"));
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
    QJsonObject jsonObject;

    QVariantList variantList;
    foreach (int selectedGenre, selectedGenres) {
        variantList.append(selectedGenre);
    }
    jsonObject.insert("selected_genres", QJsonArray::fromVariantList(variantList));

    variantList.clear();
    foreach (int deniedGenre, deniedGenres) {
        variantList.append(deniedGenre);
    }
    jsonObject.insert("denied_genres", QJsonArray::fromVariantList(variantList));

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
void FMASource::jsonToConfig(QJsonDocument jsonDocument)
{
    selectedGenres.clear();
    deniedGenres.clear();

    if (jsonDocument.object().contains("selected_genres")) {
        foreach (QJsonValue jsonValue, jsonDocument.object().value("selected_genres").toArray()) {
            selectedGenres.append(jsonValue.toInt());
        }
    }
    else {
        emit executeGlobalSql(id, false, "", SQL_COLLECTIONCHECK_GENRE, "SELECT id FROM genres ORDER BY RANDOM() LIMIT 1", QVariantList());
    }

    if (jsonDocument.object().contains("denied_genres")) {
        foreach (QJsonValue jsonValue, jsonDocument.object().value("denied_genres").toArray()) {
            deniedGenres.append(jsonValue.toInt());
        }
    }
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
    QStringList  trackGenreBinds;
    QVariantList trackGenreValues;

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

            if ((state == AlbumList) || (state == OpeningPerformersAlbums)) {
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

            if ((state == TrackList) || (state == OpeningTracks)) {
                if (xmlStreamReader.name().toString().compare("track_id") == 0) {
                    parseElement = TrackId;
                }
                if (xmlStreamReader.name().toString().compare("track_title") == 0) {
                    parseElement = TrackTitle;
                }
                if (xmlStreamReader.name().toString().compare("artist_name") == 0) {
                    parseElement = TrackArtistName;
                }
                if (xmlStreamReader.name().toString().compare("track_url") == 0) {
                    parseElement = TrackUrl;
                }
                if (xmlStreamReader.name().toString().compare("track_image_file") == 0) {
                    parseElement = TrackImageFile;
                }
                if (xmlStreamReader.name().toString().compare("track_number") == 0) {
                    parseElement = TrackNumber;
                }
                if (xmlStreamReader.name().toString().compare("track_genres") == 0) {
                    parseElement = TrackGenres;
                }
                if ((parseElement == TrackGenres) && (xmlStreamReader.name().toString().compare("genre_id") == 0)) {
                    parseElement = TrackGenre;
                }
            }
        }

        if (tokenType == QXmlStreamReader::Characters) {
            QString text = xmlStreamReader.text().toString();
            switch (parseElement) {
                case Page:
                    stringToInt(text, &page);
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
                case TrackArtistName:
                    trackTemp.performer = text;
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
                case TrackGenre:
                    // this assumes by now track id is known, which is the case in every XML seen
                    int temp;
                    if ((trackTemp.id != 0) && (stringToInt(text, &temp))) {
                        trackGenreBinds.append("(?,?)");
                        trackGenreValues.append(trackTemp.id);
                        trackGenreValues.append(temp);

                        if (trackGenreValues.count() > 750) {
                            emit executeGlobalSql(id, false, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO tracks_genres (track_id, genre_id) VALUES " + trackGenreBinds.join(","), trackGenreValues);
                        }
                    }
                    break;

                default:
                    break;
            }
        }

        if (tokenType == QXmlStreamReader::EndElement) {
            if (parseElement == TrackGenres) {
                if (xmlStreamReader.name().toString().compare("track_genres") == 0) {
                    parseElement = Unknown;
                }
            }
            else if (parseElement == TrackGenre) {
                if (xmlStreamReader.name().toString().compare("genre_id") == 0) {
                    parseElement = TrackGenres;
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
                if (state == GenreList) {
                    binds.append("(?,?,?,?)");

                    values.append(genreTemp.id);
                    values.append(genreTemp.parentId);
                    values.append(genreTemp.handle);
                    values.append(genreTemp.name);

                    // can't have too many in one single statement
                    if (values.count() >= 750) {
                        emit executeGlobalSql(id, false, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO genres (id, parent_id, handle, name) VALUES " + binds.join(","), values);
                        binds.clear();
                        values.clear();
                    }
                }

                if ((state == AlbumList) || (state == OpeningPerformersAlbums)) {
                    binds.append("(?,?,?,?,?)");

                    values.append(albumTemp.id);
                    values.append(albumTemp.genreId);
                    values.append(albumTemp.album);
                    values.append(albumTemp.performer);
                    values.append(albumTemp.year);

                    // can't have too many in one single statement
                    if (values.count() >= 750) {
                        emit executeGlobalSql(id, false, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO albums (id, genre_id, album, performer, year) VALUES " + binds.join(","), values);
                        binds.clear();
                        values.clear();
                    }
                }

                if ((state == TrackList) || (state == OpeningTracks)) {
                    binds.append("(?,?,?,?,?,?,?)");

                    values.append(trackTemp.id);
                    values.append(trackTemp.albumId);
                    values.append(trackTemp.title);
                    values.append(trackTemp.performer);
                    values.append(trackTemp.url);
                    values.append(trackTemp.pictureUrl);
                    values.append(trackTemp.track);

                    // can't have too many in one single statement
                    if (values.count() >= 750) {
                        emit executeGlobalSql(id, false, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO tracks (id, album_id, title, performer, url, picture_url, track) VALUES " + binds.join(","), values);
                        binds.clear();
                        values.clear();
                    }
                }
            }
        }
    }
    if (xmlStreamReader.hasError()) {
        emit infoMessage(id, "Error while parsing XML response from FMA");
        #ifdef QT_DEBUG
        qWarning() << reply->url().toString();
        #endif
        setState(Idle);
        reply->deleteLater();
        return;
    }

    // finish saving
    if (values.count() > 0) {
        if (state == GenreList) {
            emit executeGlobalSql(id, false, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO genres (id, parent_id, handle, name) VALUES " + binds.join(","), values);
        }
        if ((state == AlbumList) || (state == OpeningPerformersAlbums)) {
            emit executeGlobalSql(id, false, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO albums (id, genre_id, album, performer, year) VALUES " + binds.join(","), values);
        }
        if ((state == TrackList) || (state == OpeningTracks)) {
            emit executeGlobalSql(id, false, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO tracks (id, album_id, title, performer, url, picture_url, track) VALUES " + binds.join(","), values);
        }
    }
    if (trackGenreValues.count() > 0) {
        emit executeGlobalSql(id, false, "", SQL_NO_RESULTS, "INSERT OR IGNORE INTO tracks_genres (track_id, genre_id) VALUES " + trackGenreBinds.join(","), trackGenreValues);
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
    reply->deleteLater();

    // genre list
    if (state == GenreList) {
        // saving is already finished so let's fake it (this will give a chance to the settingshandler thread to complete everything before continuing here)
        emit executeGlobalSql(id, false, "", SQL_STARTUPCHECK_GENRESLOADED, "SELECT CURRENT_TIMESTAMP", QVariantList());

        setState(Idle);
        return;
    }

    // albums list (genre query)
    if ((state == AlbumList) || (state == OpeningPerformersAlbums)) {
        // saving is already finished so let's fake it
        emit executeGlobalSql(id, false, "", state == AlbumList ? SQL_LOADMORE_ALBUMSLOADED : SQL_OPEN_PERFORMERS_LOADED, "SELECT CURRENT_TIMESTAMP", QVariantList());

        genreSearchItems.removeFirst();

        setState(Idle);

        QTimer::singleShot(NETWORK_WAIT_MS, this, SLOT(genreSearch()));

        return;
    }

    // tracks list (album query)
    if ((state == TrackList) || (state == OpeningTracks)) {
        // saving is already finished so let's fake it
        emit executeGlobalSql(id, false, "", state == TrackList ? SQL_LOADMORE_TRACKSLOADED : SQL_OPEN_TRACKS_LOADED, "SELECT CURRENT_TIMESTAMP", QVariantList());

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
    QString      binds;
    QVariantList values;
    selectedGenresBinds(&binds, &values);

    QString diagnosticsSql(
        " SELECT name AS genre,                                                                               "
        "        (SELECT COUNT(*)                                                                             "
        "         FROM albums                                                                                 "
        "         WHERE genre_id = genres.id) AS album_count,                                                 "
        "        (SELECT COUNT(*)                                                                             "
        "         FROM tracks LEFT JOIN albums ON tracks.album_id = albums.id                                 "
        "         WHERE genre_id = genres.id) AS track_count,                                                 "
        "        (SELECT COUNT(*)                                                                             "
        "         FROM banned                                                                                 "
        "              LEFT JOIN tracks ON banned.track_id = tracks.id                                        "
        "              LEFT JOIN albums ON tracks.album_id = albums.id                                        "
        "         WHERE genre_id = genres.id) AS banned_count,	                                              "
        "        (SELECT COUNT(*)                                                                             "
        "         FROM albums                                                                                 "
        "         WHERE (genre_id = genres.id) AND                                                            "
        "               (id NOT IN (SELECT album_id FROM tracks GROUP BY album_id))) AS albums_not_yet_loaded "
        " FROM genres                                                                                         "
        " WHERE id IN (" + binds + ")                                                                         "
        " ORDER BY name                                                                                       "
    );

    emit executeGlobalSql(id, false, "", SQL_DIAGNOSTICS, diagnosticsSql, values);
}
