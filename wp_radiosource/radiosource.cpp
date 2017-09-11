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

    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wp_radiosource.png");
    QFile::copy(":/images/wp_radiosource.png", QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wp_radiosource.png");
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
    return 3;
}


// overrided virtual function
QString RadioSource::waverVersionAPICompatibility()
{
    return "0.0.4";
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

    emit loadGlobalConfiguration(id);
    emit loadConfiguration(id);
}


// slot receiving configuration
void RadioSource::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    stationsCache.clear();

    jsonToConfig(configuration);

    if (selectedGenres.count() < 1) {
        readySent = false;
        emit unready(id);
        return;
    }

    if ((genres.count() > 0) && (selectedGenres.count() > 0)) {
        cacheRetries = 0;
        cache();
    }
}


// slot receiving configuration
void RadioSource::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfigGlobal(configuration);

    if (genresLoaded.daysTo(QDateTime::currentDateTime()) >= 15) {
        genres.clear();
    }

    if (genres.count() < 1) {
        readySent = false;
        emit unready(id);

        state = GenreList;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }

        QNetworkRequest request(QUrl("http://api.shoutcast.com/genre/secondary?parentid=0&k=" + key + "&f=xml"));
        request.setRawHeader("User-Agent", userAgent.toUtf8());
        latestReply = networkAccessManager->get(request);

        return;
    }

    if ((genres.count() > 0) && (selectedGenres.count() > 0)) {
        cacheRetries = 0;
        cache();
    }
}


// client wants to display this plugin's configuration dialog
void RadioSource::getUiQml(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    QFile settingsFile("://RS_CategorySettings.qml");
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
        bool isSelected = false;
        int  j          = 0;
        while (!isSelected && (j < selectedGenres.count())) {
            if (genres.at(i).name.compare(selectedGenres.at(j).name) == 0) {
                isSelected = true;
            }
            j++;
        }

        checkboxes.append(
            QString("CheckBox { id: %1; text: \"%2\"; tristate: false; checked: %3; anchors.top: %4; anchors.topMargin: %5; anchors.left: parent.left; anchors.leftMargin: 6 }")
            .arg(QString("cb_%1").arg(i))
            .arg(genres.at(i).isPrimary ? "<b>" + genres.at(i).name + "</b>" : genres.at(i).name)
            .arg(isSelected ? "true" : "false")
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

    QVector<SelectedGenre> uiSelectedList;
    foreach (QJsonValue jsonValue, results.array()) {
        SelectedGenre uiSelected;
        uiSelected.name  = jsonValue.toString();
        uiSelected.limit = 0;

        uiSelected.name.replace(QRegExp("</?b>"), "");

        foreach (SelectedGenre selectedGenre, selectedGenres) {
            if (uiSelected.name.compare(selectedGenre.name) == 0) {
                uiSelected.limit = selectedGenre.limit;
            }
        }

        uiSelectedList.append(uiSelected);
    }

    selectedGenres = uiSelectedList;

    loadedConfiguration(id, configToJson());

    emit requestRemoveTracks(id);

    emit saveConfiguration(id, configToJson());
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

    if (!unableToStartUrls.contains(url.toString())) {
        unableToStartUrls.append(url.toString());
    }

    emit saveGlobalConfiguration(id, configToJsonGlobal());
}


// reuest for playlist entries
void RadioSource::getPlaylist(QUuid uniqueId, int maxCount)
{
    if (uniqueId != id) {
        return;
    }

    // just to make sure, this should never happen
    if ((stationsCache.count() < 1) && (state == Idle)) {
        cacheRetries = 0;
        cache();
        return;
    }

    // how many to return
    int count = (qrand() % maxCount) + 1;

    // select stations to return
    TracksInfo   tracksInfo;
    bool         added = true;
    while (added && (tracksInfo.count() < count)) {
        added = false;
        int i = 0;
        while (!added && (i < stationsCache.count())) {
            if (!stationsCache.at(i).url.isEmpty() && (!bannedUrls.contains(stationsCache.at(i).url.toString()) && !unableToStartUrls.contains(stationsCache.at(i).url.toString()))) {
                TrackInfo trackInfo;

                trackInfo.album     = stationsCache.at(i).genre;
                trackInfo.cast      = true;
                trackInfo.performer = "Online Radio";
                trackInfo.title     = stationsCache.at(i).name;
                trackInfo.track     = 0;
                trackInfo.url       = stationsCache.at(i).url;
                trackInfo.year      = 0;
                trackInfo.actions.insert(0, "Ban");
                trackInfo.pictures.append(QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wp_radiosource.png"));

                tracksInfo.append(trackInfo);

                stationsCache.remove(i);
                added = true;
            }
            i++;
        }
    }

    // diagnostics
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    // send stations
    if (tracksInfo.count() > 0) {
        emit playlist(id, tracksInfo);
    }

    // maybe cache is low
    if ((stationsCache.count() < CACHE_REQUEST_COUNT) && (state == Idle)) {
        cacheRetries = 0;
        cache();
    }
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
        foreach (SelectedGenre genre, selectedGenres) {
            OpenTrack openTrack;
            openTrack.hasChildren = true;
            openTrack.id          = genre.name;
            openTrack.label       = genre.name;
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
    if (openData.contains(parentId)) {
        foreach (Station station, openData.value(parentId)) {
            OpenTrack openTrack;
            openTrack.hasChildren = false;
            openTrack.id          = station.id;
            openTrack.label       = station.name;
            openTrack.selectable = true;
            openTracks.append(openTrack);
        }

        qSort(openTracks.begin(), openTracks.end(), [](OpenTrack a, OpenTrack b) {
            return (a.label.compare(b.label) < 0);
        });

        emit openTracksResults(id, openTracks);

        return;
    }

    // must wait if another network operation is in progress
    if (state != Idle) {
        getOpenTracksWaitParentId = parentId;
        QTimer::singleShot(CACHE_REQUEST_COUNT * PLAYLIST_REQUEST_DELAY * 2, this, SLOT(getOpenTracksWait()));
        return;
    }

    state = Opening;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    // send request
    QNetworkRequest request(QUrl("http://api.shoutcast.com/legacy/genresearch?k=" + key + "&genre=" + parentId));
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    latestReply = networkAccessManager->get(request);
}


// timer slot
void RadioSource::getOpenTracksWait()
{
    // somthing's taking too long, better cancel it
    if (state != Idle) {
        emit infoMessage(id, "Network operation takes too long, cancelling");
        latestReply->abort();
    }

    state = Opening;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    // send request
    QNetworkRequest request(QUrl("http://api.shoutcast.com/legacy/genresearch?k=" + key + "&genre=" + getOpenTracksWaitParentId));
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    latestReply = networkAccessManager->get(request);
}


// turn open dialog selections to playlist entries
void RadioSource::resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks)
{
    if (uniqueId != id) {
        return;
    }

    // find the stations to open
    stationsToOpen.clear();
    foreach (QString genre, openData.keys()) {
        foreach (Station station, openData.value(genre)) {
            if (selectedTracks.contains(station.id)) {
                stationsToOpen.append(station);
            }
        }
    }

    // make sure there's something to open
    if (stationsToOpen.count() < 1) {
        return;
    }

    // must wait if another network operation is in progress
    if (state != Idle) {
        QTimer::singleShot(CACHE_REQUEST_COUNT * PLAYLIST_REQUEST_DELAY * 2, this, SLOT(resolveOpenTracksWait()));
        return;
    }

    // state and diagnostics
    state = Opening;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    // start to get the urls
    tuneIn();
}


// timer slot
void RadioSource::resolveOpenTracksWait()
{
    // something's taking too long, better cancel it
    if (state != Idle) {
        emit infoMessage(id, "Network operation takes too long, cancelling");
        latestReply->abort();
    }

    state = Opening;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    // start to get the urls
    tuneIn();
}


// request for playlist entries based on search criteria
void RadioSource::search(QUuid uniqueId, QString criteria)
{
    if (uniqueId != id) {
        return;
    }
    /*
        criteria.replace("_", " ").replace(" ", "");

        QVector<Station> matches;
        foreach (Station station, stations) {
            QString modifying(station.name);
            modifying.replace("_", " ").replace(" ", "");

            if (modifying.contains(criteria, Qt::CaseInsensitive)) {
                matches.append(station);
            }
        }

        qSort(matches.begin(), matches.end(), [](Station a, Station b) {
            return (a.name.compare(b.name) < 0);
        });

        OpenTracks openTracks;
        foreach (Station match, matches) {
            OpenTrack openTrack;

            openTrack.hasChildren = false;
            openTrack.id          = match.url.toString();
            openTrack.label       = match.name;
            openTrack.selectable  = true;
            openTracks.append(openTrack);
        }

        emit searchResults(id, openTracks);
    */
}


// user clicked action that was included in track info
void RadioSource::action(QUuid uniqueId, int actionKey, QUrl url)
{
    if (uniqueId != id) {
        return;
    }

    if (actionKey == 0) {
        if (!bannedUrls.contains(url.toString())) {
            bannedUrls.append(url.toString());
        }
        emit saveGlobalConfiguration(id, configToJsonGlobal());
        emit requestRemoveTrack(id, url);
        return;
    }

    if (actionKey == 1) {
        if (bannedUrls.contains(url.toString())) {
            bannedUrls.removeAll(url.toString());
            emit saveGlobalConfiguration(id, configToJsonGlobal());
            emit requestRemoveTrack(id, url);
        }
        return;
    }
}


// private method
void RadioSource::cache()
{
    // this should never happen, but just to be on the safe side
    if (selectedGenres.count() < 1) {
        readySent = false;
        emit unready(id);
        return;
    }

    // must wait if another network operation is in progress
    if (state != Idle) {
        QTimer::singleShot(CACHE_REQUEST_COUNT * PLAYLIST_REQUEST_DELAY * 2, this, SLOT(cacheWait()));
        return;
    }

    // set state
    state = Caching;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    // should select from those that aren't done yet
    QVector<SelectedGenre> notDoneSelected;
    foreach (SelectedGenre selectedGenre, selectedGenres) {
        if (selectedGenre.limit >= 0) {
            notDoneSelected.append(selectedGenre);
        }
    }
    if (notDoneSelected.count() < 1) {
        // all are done, use all
        for (int i = 0; i < selectedGenres.count(); i++) {
            selectedGenres[i].limit = 0;
        }
        emit saveConfiguration(id, configToJson());
        notDoneSelected.append(selectedGenres);
    }

    // random select a genre
    SelectedGenre genre = notDoneSelected.at(qrand() % notDoneSelected.count());

    // maybe it's in open data if user opened from this genre earlier
    if (openData.contains(genre.name)) {
        // find genre
        int genreIndex = findSelectedGenreIndex(genre.name);
        if (genreIndex < 0) {
            emit infoMessage(id, "Genre not found in selected genre list");
            return;
        }

        // get stations from open data
        QVector<Station> stationsTemp;
        int i = selectedGenres.at(genreIndex).limit;
        while ((i < openData.value(selectedGenres.at(genreIndex).name).count()) && (stationsTemp.count() < CACHE_REQUEST_COUNT)) {
            stationsTemp.append(openData.value(selectedGenres.at(genreIndex).name).at(i));
            i++;
        }

        // save new limit
        selectedGenres[genreIndex].limit = (stationsTemp.count() < CACHE_REQUEST_COUNT ? 0 : selectedGenres.at(genreIndex).limit + stationsTemp.count());
        emit saveConfiguration(id, configToJson());

        // use stations
        stationsCache.append(stationsTemp);
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }

        // start to get the urls
        tuneIn();

        return;
    }

    // have to query now
    QNetworkRequest request(QUrl("http://api.shoutcast.com/legacy/genresearch?k=" + key + "&limit=" + QString("%1,%2").arg(genre.limit).arg(CACHE_REQUEST_COUNT) + "&genre=" + genre.name));
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    latestReply = networkAccessManager->get(request);
}


// timer slot
void RadioSource::cacheWait()
{
    // somthing's taking too long, better cancel it
    if (state != Idle) {
        emit infoMessage(id, "Network operation takes too long, cancelling");

        latestReply->abort();

        state = Idle;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
    }

    cache();
}


// configuration conversion
QJsonDocument RadioSource::configToJson()
{
    QJsonArray jsonArray;
    foreach (SelectedGenre selectedGenre, selectedGenres) {
        jsonArray.append(QJsonArray({ selectedGenre.name, selectedGenre.limit }));
    }

    QJsonObject jsonObject;
    jsonObject.insert("selected_genre_list", jsonArray);

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
QJsonDocument RadioSource::configToJsonGlobal()
{
    QJsonArray jsonArray;
    foreach (Genre genre, genres) {
        jsonArray.append(QJsonArray({ genre.name, genre.isPrimary }));
    }

    QJsonObject jsonObject;

    jsonObject.insert("genre_list", jsonArray);
    jsonObject.insert("genre_list_loaded", genresLoaded.toString());
    jsonObject.insert("banned_urls", QJsonArray::fromStringList(bannedUrls));
    jsonObject.insert("unable_to_start_urls", QJsonArray::fromStringList(unableToStartUrls));

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
void RadioSource::jsonToConfig(QJsonDocument jsonDocument)
{
    if (jsonDocument.object().contains("selected_genre_list")) {
        selectedGenres.clear();

        foreach (QJsonValue jsonValue, jsonDocument.object().value("selected_genre_list").toArray()) {
            QJsonArray value = jsonValue.toArray();

            // this is more for the UI results - can't have the same twice but some subgenres are listed under multiple primary genres
            int i      = 0;
            bool found = false;
            while ((i < selectedGenres.count()) && !found) {
                if (selectedGenres.at(i).name.compare(value.at(0).toString()) == 0) {
                    found = true;
                }
                i++;
            }

            if (!found) {
                selectedGenres.append({ value.at(0).toString(), value.at(1).toInt() });
            }
        }
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
    if (jsonDocument.object().contains("genre_list_loaded")) {
        genresLoaded = QDateTime::fromString(jsonDocument.object().value("genre_list_loaded").toString());
    }
    if (jsonDocument.object().contains("banned_urls")) {
        bannedUrls.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("banned_urls").toArray()) {
            bannedUrls.append(jsonValue.toString());
        }
    }
    if (jsonDocument.object().contains("unable_to_start_urls")) {
        unableToStartUrls.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("unable_to_start_urls").toArray()) {
            unableToStartUrls.append(jsonValue.toString());
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
    QFile keyFile("://key");
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


// network signal handler
void RadioSource::networkFinished(QNetworkReply *reply)
{
    // make sure reply OK
    if (reply->error() != QNetworkReply::NoError) {
        emit infoMessage(id, QString("Network error"));
        state = Idle;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        reply->deleteLater();
        return;
    }
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 200) {
        emit infoMessage(id, QString("SHOUTcast Radio Directory service returned status %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString()));
        state = Idle;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        reply->deleteLater();
        return;
    }

    bool             genresChanged   = false;
    bool             stationsChanged = false;
    QString          base;
    QVector<Station> stationsTemp;

    QXmlStreamReader xmlStreamReader(reply);
    while (!xmlStreamReader.atEnd()) {
        QXmlStreamReader::TokenType tokenType = xmlStreamReader.readNext();

        if (tokenType == QXmlStreamReader::StartElement) {
            // genre query
            if (!genresChanged && (xmlStreamReader.name().toString().compare("genrelist") == 0)) {
                genres.clear();
                genresChanged = true;
            }
            if (xmlStreamReader.name().toString().compare("genre") == 0) {
                Genre genre;
                genre.isPrimary = false;

                QXmlStreamAttributes attributes = xmlStreamReader.attributes();
                if (attributes.hasAttribute("name")) {
                    genre.name = attributes.value("name").toString();
                }
                if (attributes.hasAttribute("parentid")) {
                    genre.isPrimary = (attributes.value("parentid").toString().compare("0") == 0);
                }
                if (!genre.name.isEmpty()) {
                    genres.append(genre);
                }
            }

            // stations query
            if (!stationsChanged && (xmlStreamReader.name().toString().compare("stationlist") == 0)) {
                stationsChanged = true;
            }
            if (xmlStreamReader.name().toString().compare("tunein") == 0) {
                QXmlStreamAttributes attributes = xmlStreamReader.attributes();
                if (attributes.hasAttribute("base")) {
                    base = attributes.value("base").toString();
                }
            }
            if (xmlStreamReader.name().toString().compare("station") == 0) {
                Station     station;
                QStringList stationsGenres;

                QXmlStreamAttributes attributes = xmlStreamReader.attributes();
                if (attributes.hasAttribute("id")) {
                    station.id = attributes.value("id").toString();
                }
                if (attributes.hasAttribute("name")) {
                    station.name = attributes.value("name").toString();
                }

                if (attributes.hasAttribute("genre")) {
                    stationsGenres.append(attributes.value("genre").toString());
                }
                int i = 1;
                while (attributes.hasAttribute(QString("genre%1").arg(i))) {
                    stationsGenres.append(attributes.value(QString("genre%1").arg(i)).toString());
                    i++;
                }
                station.genre = stationsGenres.join(' ');

                if (!station.id.isEmpty() && !station.name.isEmpty() && !station.genre.isEmpty()) {
                    station.base = base;
                    stationsTemp.append(station);
                }
            }
        }
    }
    if (xmlStreamReader.hasError()) {
        emit infoMessage(id, "Error while parsing XML response from SHOUTcast Radio Directory service");
        state = Idle;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        reply->deleteLater();
        return;
    }

    // genre query
    if (genresChanged) {
        // save configuration
        genresLoaded = QDateTime::currentDateTime();
        emit saveGlobalConfiguration(id, configToJsonGlobal());

        state = Idle;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }

        // this was initiated by nonexisting configuration, so let's act now
        if ((genres.count() > 0) && (selectedGenres.count() > 0)) {
            cacheRetries = 0;
            cache();
        }

        reply->deleteLater();
        return;
    }

    // stations query
    if (stationsChanged) {
        // get the genre
        QString genre;
        QString query = reply->url().query();
        if (query.contains("genre=")) {
            // genre was passed last
            genre = query.mid(query.indexOf("genre=") + QString("genre=").length());
        }
        reply->deleteLater();
        if (genre.isEmpty()) {
            emit infoMessage(id, "Genre not found in URL query");
            state = Idle;
            if (sendDiagnostics) {
                sendDiagnosticsData();
            }
            return;
        }

        // find the genre
        int genreIndex = findSelectedGenreIndex(genre);
        if (genreIndex < 0) {
            emit infoMessage(id, "Genre not found in selected genre list");
            state = Idle;
            if (sendDiagnostics) {
                sendDiagnosticsData();
            }
            return;
        }

        if (state == Caching) {
            // maybe no station was returned, probably query limit passed the number of available stations
            if (stationsTemp.count() < 1) {
                // reset limit
                selectedGenres[genreIndex].limit = -1;
                emit saveConfiguration(id, configToJson());

                state = Idle;
                if (sendDiagnostics) {
                    sendDiagnosticsData();
                }

                // retry
                if (cacheRetries <= 3) {
                    cacheRetries++;
                    cache();
                    return;
                }

                return;
            }

            // save new limit
            selectedGenres[genreIndex].limit = (stationsTemp.count() < CACHE_REQUEST_COUNT ? -1 : selectedGenres.at(genreIndex).limit + stationsTemp.count());
            emit saveConfiguration(id, configToJson());

            // use stations
            stationsCache.append(stationsTemp);

            if (sendDiagnostics) {
                sendDiagnosticsData();
            }

            // start to get the urls
            tuneIn();
        }

        if (state == Opening) {
            state = Idle;
            if (sendDiagnostics) {
                sendDiagnosticsData();
            }

            // must prevent infinite loop
            if (stationsTemp.count() < 1) {
                return;
            }

            // remember the stations
            openData.remove(selectedGenres.at(genreIndex).name);
            openData.insert(selectedGenres.at(genreIndex).name, stationsTemp);

            // this will send it back to the client
            getOpenTracks(id, selectedGenres.at(genreIndex).name);
        }
    }
}


// timer signal handler
void RadioSource::tuneIn()
{
    // just to be on the safe side, this should never happen
    if ((state != Caching) && (state != Opening) && (state != Searching)) {
        emit infoMessage(id, "Invalid state");
        state = Idle;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        return;
    }

    if (state == Caching) {
        // find next station to get url for
        int stationIndex = findStationWithoutUrl(stationsCache);

        // all stations have url (can't be here if there are no stations)
        if (stationIndex < 0) {
            state = Idle;
            if (sendDiagnostics) {
                sendDiagnosticsData();
            }

            // maybe too many stations were unable to start before or are banned
            if (stationsCache.count() < CACHE_REQUEST_COUNT) {
                cacheRetries++;
                cache();
            }

            // let the world know
            if (!readySent) {
                emit ready(id);
                readySent = true;
            }

            return;
        }

        // request the playlist for this station
        QNetworkRequest request(QUrl("http://yp.shoutcast.com" + stationsCache.at(stationIndex).base + "?id=" + stationsCache.at(stationIndex).id));
        request.setRawHeader("User-Agent", userAgent.toUtf8());
        latestReply = playlistAccessManager->get(request);
    }

    if (state == Opening) {
        // find next station to get url for
        int stationIndex = findStationWithoutUrl(stationsToOpen);

        // all stations have url (can't be here if there are no stations)
        if (stationIndex < 0) {
            state = Idle;
            if (sendDiagnostics) {
                sendDiagnosticsData();
            }

            TracksInfo tracksInfo;
            foreach (Station station, stationsToOpen) {
                TrackInfo trackInfo;

                trackInfo.album     = station.genre;
                trackInfo.cast      = true;
                trackInfo.performer = "Online Radio";
                trackInfo.title     = station.name;
                trackInfo.track     = 0;
                trackInfo.url       = station.url;
                trackInfo.year      = 0;
                trackInfo.pictures.append(QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wp_radiosource.png"));

                if (bannedUrls.contains(station.url.toString())) {
                    trackInfo.actions.insert(1, "Remove ban");
                }
                else {
                    trackInfo.actions.insert(0, "Ban");
                }

                tracksInfo.append(trackInfo);
            }
            emit playlist(id, tracksInfo);

            return;
        }

        // request the playlist for this station
        QNetworkRequest request(QUrl("http://yp.shoutcast.com" + stationsToOpen.at(stationIndex).base + "?id=" + stationsToOpen.at(stationIndex).id));
        request.setRawHeader("User-Agent", userAgent.toUtf8());
        latestReply = playlistAccessManager->get(request);
    }
}


// network signal handler
void RadioSource::playlistFinished(QNetworkReply *reply)
{
    // make sure reply OK
    if (reply->error() != QNetworkReply::NoError) {
        emit infoMessage(id, QString("Network error"));
        state = Idle;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }

        reply->deleteLater();
        return;
    }
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 200) {
        emit infoMessage(id, QString("SHOUTcast Radio Directory service returned status %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString()));
        state = Idle;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }

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
        state = Idle;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        reply->deleteLater();
        return;
    }

    // find it in the stations
    int stationIndex = -1;
    if (state == Caching) {
        stationIndex = findStationById(stationsCache, stationId, true);
    }
    else if (state == Opening) {
        stationIndex = findStationById(stationsToOpen, stationId, true);
    }
    if (stationIndex < 0) {
        emit infoMessage(id, "Station id not found in stations list");
        state = Idle;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        reply->deleteLater();
        return;
    }

    // get the station's playlist
    QString stationPlaylist(reply->readAll());
    reply->deleteLater();

    // see if there's a good URL
    int i = 1;
    QUrl    url;
    QRegExp regExp;
    regExp.setMinimal(true);
    regExp.setPattern(QString("File%1=(.+)\n").arg(i));
    while ((stationPlaylist.contains(regExp)) && url.isEmpty()) {
        url = QUrl(regExp.capturedTexts().at(1));
        if ((state == Caching) && (bannedUrls.contains(url.toString()) || unableToStartUrls.contains(url.toString()))) {
            url.clear();
        }
        i++;
        regExp.setPattern(QString("File%1=(.+)\n").arg(i));
    }
    if (url.isEmpty()) {
        // this station is probably either banned or was unable to be started before
        if (state == Caching) {
            stationsCache.remove(stationIndex);
        }
        else if (state == Opening) {
            stationsToOpen.remove(stationIndex);
        }
    }
    else {
        if (state == Caching) {
            stationsCache[stationIndex].url = url;
        }
        else if (state == Opening) {
            stationsToOpen[stationIndex].url = url;
        }
    }

    // check the next station's URL after a bit of a delay so not to run too many requests rapidly
    QTimer::singleShot(PLAYLIST_REQUEST_DELAY, this, SLOT(tuneIn()));
}


// helper
int RadioSource::findSelectedGenreIndex(QString genreName)
{
    int returnValue = -1;
    int i           = 0;
    while ((i < selectedGenres.count()) && (returnValue < 0)) {
        if (selectedGenres.at(i).name.compare(genreName) == 0) {
            returnValue = i;
        }
        i++;
    }

    return returnValue;
}


// helper
int RadioSource::findStationWithoutUrl(QVector<Station> stations)
{
    int returnValue = -1;
    int i            = 0;
    while ((i < stations.count()) && (returnValue < 0)) {
        if (stations.at(i).url.isEmpty()) {
            returnValue = i;
        }
        i++;
    }

    return returnValue;
}


// helper
int RadioSource::findStationById(QVector<Station> stations, QString id, bool emptyUrlOnly)
{
    int returnValue = -1;
    int i           = 0;
    while ((i < stations.count()) && (returnValue < 0)) {
        if ((stations.at(i).id.compare(id) == 0) && (!emptyUrlOnly || (stations.at(i).url.isEmpty()))) {
            returnValue = i;
        }
        i++;
    }

    return returnValue;
}


// helper
void RadioSource::sendDiagnosticsData()
{
    DiagnosticData diagnosticData;

    switch (state) {
        case Idle:
            diagnosticData.append({ "Status", "Idle"});
            break;
        case GenreList:
            diagnosticData.append({ "Status", "Getting genre list..." });
            break;
        case Caching:
            diagnosticData.append({ "Status", "Caching station data..." });
            break;
        case Opening:
            diagnosticData.append({ "Status", "Getting station data..." });
            break;
        case Searching:
            diagnosticData.append({ "Status", "Searching stations..." });
            break;
    }

    int noUrl = 0;
    foreach (Station station, stationsCache) {
        if (station.url.isEmpty()) {
            noUrl++;
        }
    }

    DiagnosticItem diagnosticItem;
    diagnosticItem.label   = "Stations' data in cache";
    diagnosticItem.message = QString("%1 (Incomplete: %2)").arg(stationsCache.count()).arg(noUrl);
    diagnosticData.append(diagnosticItem);

    diagnosticItem.label   = "Banned stations";
    diagnosticItem.message = QString("%1").arg(bannedUrls.count());
    diagnosticData.append(diagnosticItem);

    QStringList openDataAvailable;
    foreach (QString genre, openData.keys()) {
        openDataAvailable.append(genre);
    }
    if (openDataAvailable.count() > 0) {
        diagnosticItem.label   = (openDataAvailable.count() > 1 ? "Buffered lists" : "Buffered list");
        diagnosticItem.message = openDataAvailable.join(", ");
        diagnosticData.append(diagnosticItem);
    }

    emit diagnostics(id, diagnosticData);
}
