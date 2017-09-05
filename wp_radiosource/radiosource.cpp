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

    jsonToConfig(configuration);

    if (selectedGenres.count() < 1) {
        emit unready(id);
        return;
    }

    if ((genres.count() > 0) && (selectedGenres.count() > 0)) {
        emit ready(id);
    }
}


// slot receiving configuration
void RadioSource::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfigGlobal(configuration);

    // TODO clear genres list once a month
    if (genres.count() < 1) {
        emit unready(id);

        QNetworkRequest request(QUrl("http://api.shoutcast.com/legacy/genrelist?k=" + key));
        request.setRawHeader("User-Agent", userAgent.toUtf8());
        networkAccessManager->get(request);

        return;
    }

    if ((genres.count() > 0) && (selectedGenres.count() > 0)) {
        emit ready(id);
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

    settings.replace("replace_content_height", QString("%1 * (cb_0.height + 6)").arg(genres.count()));

    QString checkboxes;
    QString checkboxesToAll;
    QString checkboxesToRetval;
    for (int i = 0; i < genres.count(); i++) {
        checkboxes.append(
            QString("CheckBox { id: %1; text: \"%2\"; tristate: false; checked: %3; anchors.top: %4; anchors.topMargin: 6; anchors.left: parent.left; anchors.leftMargin: 6 }")
            .arg(QString("cb_%1").arg(i))
            .arg(genres.at(i))
            .arg(selectedGenres.contains(genres.at(i)) ? "true" : "false")
            .arg(i > 0 ? QString("cb_%1.bottom").arg(i - 1) : "parent.top"));
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

    QVariantHash variantHash;
    if (results.isObject()) {
        variantHash = results.object().toVariantHash();
    }

    // not a command, same as loaded configuration (because that's how the UI returns it)
    loadedConfiguration(id, results);

    // make sure no more tracks in the playlist from previous configuration
    emit requestRemoveTracks(id);

    // must save new configuration
    emit saveConfiguration(id, configToJson());
}


// client wants to receive updates of this plugin's diagnostic information
void RadioSource::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = true;
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

    // increase counter
    /*  int i = 0;
        while (i < stations.count()) {
        Station station = stations.at(i);
        if (station.url == url) {
            station.unableToStartCount++;
            stations.replace(i, station);
        }
        i++;
        }

        // this updates selected stations
        jsonToConfig(configToJson());

        // save configuration
        emit saveConfiguration(id, configToJson());
    */
}


// reuest for playlist entries
void RadioSource::getPlaylist(QUuid uniqueId, int maxCount)
{
    if (uniqueId != id) {
        return;
    }

    // this should never happen, but just to be on the safe side
    if (selectedGenres.count() < 1) {
        emit unready(id);
        return;
    }

    /*
        // check if a station failed to download three or more times, also this helps to make sure no station is selected twice during this request
        QVector<Station> selectableStations;
        foreach (Station station, selectedStations) {
        if (station.unableToStartCount < 3) {
            selectableStations.append(station);
        }
        }

        // maybe too many stations got banned due to network errors
        if ((selectableStations.count() < 1) || (selectableStations.count() < (selectedStations.count() / 10))) {
        // let's get the selected categories, because only these have to be reset
        QStringList selectedCategories;
        foreach (Station station, selectedStations) {
            if (!selectedCategories.contains(station.category)) {
                selectedCategories.append(station.category);
            }
        }

        // reset counter in the selected categories
        int i = 0;
        while (i < stations.count()) {
            if (!selectedCategories.contains(stations.at(i).category)) {
                continue;
            }

            Station station;
            station.category           = stations.at(i).category;
            station.name               = stations.at(i).name;
            station.url                = stations.at(i).url;
            station.unableToStartCount = 0;

            stations.replace(i, station);

            i++;
        }

        // this updates selected stations
        jsonToConfig(configToJson());

        // save configuration
        emit saveConfiguration(id, configToJson());

        // do it again
        selectableStations.clear();
        QVector<Station> selectableStations;
        foreach (Station station, selectedStations) {
            if (station.unableToStartCount < 3) {
                selectableStations.append(station);
            }
        }
        }
    */

    // random select some stations
    int     count = (qrand() % maxCount) + 1;
    QString genre = selectedGenres.at(qrand() % selectedGenres.count());

    QNetworkRequest request(QUrl("http://api.shoutcast.com/station/randomstations?k=" + key + "&f=xml&genre=" + genre + "&limit=" + QString("%1").arg(count)));
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    networkAccessManager->get(request);
}


// client wants to dispaly open dialog
void RadioSource::getOpenTracks(QUuid uniqueId, QString parentId)
{
    if (uniqueId != id) {
        return;
    }


    OpenTracks openTracks;
    /*
        // top level
        if (parentId.length() < 1) {
            QStringList allCategories;
            foreach (Station station, stations) {
                if (!allCategories.contains(station.category)) {
                    allCategories.append(station.category);
                }
            }

            qSort(allCategories);

            foreach (QString category, allCategories) {
                OpenTrack openTrack;
                openTrack.hasChildren = true;
                openTrack.id          = category;
                openTrack.label       = category;
                openTrack.selectable = false;
                openTracks.append(openTrack);
            }
            emit openTracksResults(id, openTracks);
            return;
        }

        QVector<Station> categoryStations;
        foreach (Station station, stations) {
            if (station.category.compare(parentId) == 0) {
                categoryStations.append(station);
            }
        }

        qSort(categoryStations.begin(), categoryStations.end(), [](Station a, Station b) {
            return (a.name.compare(b.name) < 0);
        });

        foreach (Station categoryStation, categoryStations) {
            OpenTrack openTrack;

            openTrack.hasChildren = false;
            openTrack.id          = categoryStation.url.toString();
            openTrack.label       = categoryStation.name;
            openTrack.selectable  = true;
            openTracks.append(openTrack);
        }
        emit openTracksResults(id, openTracks); */
}


// turn open dialog selections to playlist entries
void RadioSource::resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks)
{
    /*
        if (uniqueId != id) {
        return;
        }

        // this doesn't keep the order, but that's a reasonable tradeoff for now
        TracksInfo returnValue;
        foreach (Station station, stations) {
        if (selectedTracks.contains(station.url.toString())) {
          TrackInfo trackInfo;
          trackInfo.album     = station.category;
          trackInfo.cast      = true;
          trackInfo.performer = "Online Radio";
          trackInfo.title     = station.name;
          trackInfo.track     = 0;
          trackInfo.url       = station.url;
          trackInfo.year      = 0;
          trackInfo.actions.insert(0, "Ban");

          returnValue.append(trackInfo);
        }
        }

        emit playlist(id, returnValue);
    */
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
        bannedUrls.append(url.toString());
        emit saveGlobalConfiguration(id, configToJsonGlobal());
        emit requestRemoveTrack(id, url);
        return;
    }
}


// configuration conversion
QJsonDocument RadioSource::configToJson()
{

    QJsonObject jsonObject;
    jsonObject.insert("selected_genres", QJsonValue(QJsonArray::fromStringList(selectedGenres)));

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
QJsonDocument RadioSource::configToJsonGlobal()
{
    QJsonObject jsonObject;

    jsonObject.insert("genres", QJsonValue(QJsonArray::fromStringList(genres)));
    jsonObject.insert("banned_urls", QJsonValue(QJsonArray::fromStringList(bannedUrls)));

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
void RadioSource::jsonToConfig(QJsonDocument jsonDocument)
{
    if (jsonDocument.object().contains("selected_genres")) {
        selectedGenres.clear();

        foreach (QJsonValue jsonValue, jsonDocument.object().value("selected_genres").toArray()) {
            selectedGenres.append(jsonValue.toString());
        }
    }
}


// configuration conversion
void RadioSource::jsonToConfigGlobal(QJsonDocument jsonDocument)
{
    if (jsonDocument.object().contains("genres")) {
        genres.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("genres").toArray()) {
            genres.append(jsonValue.toString());
        }
    }

    if (jsonDocument.object().contains("banned_urls")) {
        bannedUrls.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("banned_urls").toArray()) {
            bannedUrls.append(jsonValue.toString());
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
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 200) {
        return;
    }

    bool genresChanged   = false;
    bool stationsChanged = false;

    QXmlStreamReader xmlStreamReader(reply);
    while (!xmlStreamReader.atEnd()) {
        QXmlStreamReader::TokenType tokenType = xmlStreamReader.readNext();

        if (tokenType == QXmlStreamReader::StartElement) {
            if (xmlStreamReader.name().toString().compare("genrelist") == 0) {
                genres.clear();
                genresChanged = true;
            }
            if (xmlStreamReader.name().toString().compare("genre") == 0) {
                QXmlStreamAttributes attributes = xmlStreamReader.attributes();
                if (attributes.hasAttribute("name")) {
                    genres.append(attributes.value("name").toString());
                }
            }

            if (xmlStreamReader.name().toString().compare("stationlist") == 0) {
                base         = "";
                stationIndex = 0;
                stations.clear();
                tracksInfo.clear();
                stationsChanged = true;
            }
            if (xmlStreamReader.name().toString().compare("tunein") == 0) {
                QXmlStreamAttributes attributes = xmlStreamReader.attributes();
                if (attributes.hasAttribute("base")) {
                    base = attributes.value("base").toString();
                }
            }
            if (xmlStreamReader.name().toString().compare("station") == 0) {
                Station station;
                QXmlStreamAttributes attributes = xmlStreamReader.attributes();
                if (attributes.hasAttribute("id")) {
                    station.id = attributes.value("id").toString();
                }
                if (attributes.hasAttribute("name")) {
                    station.name = attributes.value("name").toString();
                }
                if (attributes.hasAttribute("genre")) {
                    station.genre = attributes.value("genre").toString();
                }
                if (!station.genre.isEmpty() && !station.id.isEmpty() && !station.name.isEmpty()) {
                    stations.append(station);
                }
            }
        }
    }
    if (xmlStreamReader.hasError()) {
        emit infoMessage(id, "Error while parsing XML response from SHOUTcast Radio Directory service");
        return;
    }

    if (genresChanged) {
        emit saveGlobalConfiguration(id, configToJsonGlobal());
        if ((genres.count() > 0) && (selectedGenres.count() > 0)) {
            emit ready(id);
        }
    }

    if (stationsChanged) {
        tuneIn();
    }
}


// timer signal handler
void RadioSource::tuneIn()
{
    if ((stationIndex >= stations.count()) || base.isEmpty()) {
        if (tracksInfo.count() > 0) {
            // TODO check banned, unable_to_start
            emit playlist(id, tracksInfo);
        }
        return;
    }

    QNetworkRequest request(QUrl("http://yp.shoutcast.com" + base + "?id=" + stations.at(stationIndex).id));
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    playlistAccessManager->get(request);

    stationIndex++;
}


// network signal handler
void RadioSource::playlistFinished(QNetworkReply *reply)
{
    QTimer::singleShot(250, this, SLOT(tuneIn()));

    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 200) {
        return;
    }

    // get the id of the station
    QString stationId;
    QString query = reply->url().query();
    if (query.contains("id=")) {
        stationId = query.mid(query.indexOf("id=") + 3);
    }
    if (stationId.isEmpty()) {
        return;
    }

    // find it in the stations
    Station station;
    int i = 0;
    while ((i < stations.count()) && (station.id.isEmpty())) {
        if (stations.at(i).id.compare(stationId) == 0) {
            station = stations.at(i);
        }
        i++;
    }
    if (station.id.isEmpty()) {
        return;
    }

    // get the station's playlist
    QString stationPlaylist(reply->readAll());

    // see if there's an address in it
    QRegExp regExp("File1=(.+)\n");
    regExp.setMinimal(true);
    if (stationPlaylist.contains(regExp)) {
        // gotcha!
        TrackInfo trackInfo;
        trackInfo.album      = station.genre;
        trackInfo.cast       = true;
        trackInfo.performer = "Online Radio";
        trackInfo.title     = station.name;
        trackInfo.track     = 0;
        trackInfo.url       = regExp.capturedTexts().at(1);
        trackInfo.year      = 0;

        tracksInfo.append(trackInfo);
    }
}
