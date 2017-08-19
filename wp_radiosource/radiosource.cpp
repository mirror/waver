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
    id = QUuid("{3CDF6F84-3B1B-4807-8AA0-089509FD4751}");
}


// destructor
RadioSource::~RadioSource()
{
    // nothing to do here
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
    return 2;
}


// overrided virtual function
QString RadioSource::waverVersionAPICompatibility()
{
    return "0.0.1";
}


// overridden virtual function
bool RadioSource::hasUI()
{
    return true;
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

    emit loadConfiguration(id);
}


// slot receiving configuration
void RadioSource::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    if (configuration.isEmpty()) {
        if (stations.count() < 1) {
            Station station;
            station.category           = "Public";
            station.name               = "C-SPAN";
            station.url                = QUrl("http://15723.live.streamtheworld.com/CSPANRADIO.mp3");
            station.unableToStartCount = 0;
            stations.append(station);

            selectedStations.clear();
            selectedStations.append(stations);

            emit saveConfiguration(id, configToJson());
            emit ready(id);
        }
        return;
    }

    jsonToConfig(configuration);

    if (selectedStations.count() < 1) {
        emit unready(id);
        return;
    }
    emit ready(id);
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

    QStringList categories;
    QStringList selectedCategories;
    foreach (Station station, stations) {
        if (!categories.contains(station.category)) {
            categories.append(station.category);
        }
    }
    foreach (Station station, selectedStations) {
        if (!selectedCategories.contains(station.category)) {
            selectedCategories.append(station.category);
        }
    }
    qSort(categories);

    settings.replace("replace_content_height", QString("%1 * (cb_0.height + 6)").arg(categories.count()));

    QString checkboxes;
    QString checkboxesToAll;
    QString checkboxesToRetval;
    for (int i = 0; i < categories.count(); i++) {
        checkboxes.append(
            QString("CheckBox { id: %1; text: \"%2\"; tristate: false; checked: %3; anchors.top: %4; anchors.topMargin: 6; anchors.left: parent.left; anchors.leftMargin: 6 }")
            .arg(QString("cb_%1").arg(i))
            .arg(categories.at(i))
            .arg(selectedCategories.contains(categories.at(i)) ? "true" : "false")
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

    if (variantHash.contains("command")) {
        // export
        if (variantHash.value("command").toString().compare("export") == 0) {
            QFile outputFile(QUrl(variantHash.value("file").toString()).toLocalFile());
            if (!outputFile.open(QFile::WriteOnly)) {
                emit infoMessage(id, outputFile.errorString());
                return;
            }
            foreach (Station station, stations) {
                QString output = QString("%1;%2;%3\n").arg(station.category).arg(station.name).arg(station.url.toString());
                outputFile.write(output.toUtf8());
            }
            outputFile.close();
            emit infoMessage(id, "Export completed");
        }

        // return UI for import buttons
        if (variantHash.value("command").toString().compare("import") == 0) {
            QFile settingsFile("://RS_Import.qml");
            settingsFile.open(QFile::ReadOnly);
            QString settings = settingsFile.readAll();
            settingsFile.close();

            // must give time for the UI vusual transition to finish
            QThread::currentThread()->msleep(500);
            emit uiQml(id, settings);
        }

        // Rhythmbox import
        if (variantHash.value("command").toString().compare("import_rhythmbox") == 0) {
            QFile inputFile(QDir::homePath() + "/.local/share/rhythmbox/rhythmdb.xml");
            if (!inputFile.exists()) {
                emit infoMessage(id, "Cannot find Rhythmbox database");
                return;
            }
            inputFile.open(QFile::ReadOnly);
            QXmlStreamReader xmlReader(&inputFile);

            bool    insideRadio       = false;
            QString category          = "";
            QString name              = "";
            QString url               = "";
            bool    expectingCategory = false;
            bool    expectingName     = false;
            bool    expectingUrl      = false;
            int     found             = 0;
            int     imported          = 0;
            while (!xmlReader.atEnd()) {
                QXmlStreamReader::TokenType tokenType = xmlReader.readNext();

                if (insideRadio && (tokenType == QXmlStreamReader::EndElement) && (xmlReader.name().toString().compare("entry") == 0)) {
                    found++;
                    if ((category.length() > 0) && (name.length() > 0) && (url.length() > 0)) {
                        QUrl realUrl(url);
                        if (realUrl.isValid()) {
                            int i       = 0;
                            int already = false;
                            while ((i < stations.count()) && !already) {
                                if (stations.at(i).url == realUrl) {
                                    already = true;
                                }
                                i++;
                            }
                            if (!already) {
                                imported++;

                                Station station;
                                station.category           = category;
                                station.name               = name;
                                station.url                = realUrl;
                                station.unableToStartCount = 0;

                                stations.append(station);
                                selectedStations.append(station);
                            }
                        }
                    }

                    insideRadio = false;
                    continue;
                }

                if (!insideRadio && (tokenType == QXmlStreamReader::StartElement)) {
                    if (!xmlReader.attributes().hasAttribute("type") || (xmlReader.attributes().hasAttribute("type") &&
                            (xmlReader.attributes().value("type").toString().compare("iradio") != 0))) {
                        if (xmlReader.name().toString().compare("rhythmdb") != 0) {
                            xmlReader.skipCurrentElement();
                        }
                    }
                    else {
                        insideRadio = true;
                    }
                    continue;
                }

                if (tokenType == QXmlStreamReader::StartElement) {
                    if (xmlReader.name().toString().compare("genre") == 0) {
                        expectingCategory = true;
                        expectingName     = false;
                        expectingUrl      = false;
                    }
                    else if (xmlReader.name().toString().compare("title") == 0) {
                        expectingCategory = false;
                        expectingName     = true;
                        expectingUrl      = false;
                    }
                    else if (xmlReader.name().toString().compare("location") == 0) {
                        expectingCategory = false;
                        expectingName     = false;
                        expectingUrl      = true;
                    }
                    else {
                        expectingCategory = false;
                        expectingName     = false;
                        expectingUrl      = false;
                    }
                }

                if ((tokenType == QXmlStreamReader::Characters) && !xmlReader.isWhitespace()) {
                    if (expectingCategory) {
                        category = xmlReader.text().toString();
                    }
                    else if (expectingName) {
                        name = xmlReader.text().toString();
                    }
                    else if (expectingUrl) {
                        url = xmlReader.text().toString();
                    }
                }
            }
            inputFile.close();

            if (imported > 0) {
                emit saveConfiguration(id, configToJson());
            }
            emit infoMessage(id, QString("Import completed\nFound %1, imported %2").arg(found).arg(imported));
        }

        // semicolon separated values import
        if (variantHash.value("command").toString().compare("import_scsv") == 0) {
            QFile inputFile(QUrl(variantHash.value("file").toString()).toLocalFile());
            if (!inputFile.open(QFile::ReadOnly)) {
                emit infoMessage(id, inputFile.errorString());
                return;
            }
            int found    = 0;
            int imported = 0;
            int updated  = 0;
            while (!inputFile.atEnd()) {
                QStringList input = QString(inputFile.readLine(16384)).split(";");
                if (input.count() >= 3) {
                    found++;

                    QString category = input.at(0);
                    input.removeFirst();

                    QString name = input.at(0);
                    input.removeFirst();

                    QUrl realUrl(QString(input.join(";")).simplified());

                    int i       = 0;
                    int already = false;
                    while ((i < stations.count()) && !already) {
                        if (stations.at(i).url == realUrl) {
                            if ((stations.at(i).category.compare(category) != 0) || (stations.at(i).name.compare(name) != 0) ||
                                (stations.at(i).unableToStartCount > 0)) {
                                updated++;

                                Station station;
                                station.category           = category;
                                station.name               = name;
                                station.url                = realUrl;
                                station.unableToStartCount = 0;

                                stations.replace(i, station);
                            }
                            already = true;
                        }
                        i++;
                    }

                    if (!already) {
                        imported++;

                        Station station;
                        station.category           = category;
                        station.name               = name;
                        station.url                = realUrl;
                        station.unableToStartCount = 0;

                        stations.append(station);
                        selectedStations.append(station);
                    }
                }
            }
            inputFile.close();

            if ((imported > 0) || (updated > 0)) {
                emit saveConfiguration(id, configToJson());
            }
            emit infoMessage(id, QString("Import completed\nFound %1, imported %2, updated %3").arg(found).arg(imported).arg(updated));
        }

        return;
    }

    // not a command, same as loaded configuration (because that's how the UI returns it)
    loadedConfiguration(id, results);

    // make sure no more tracks in the playlist from previous configuration
    emit requestRemoveTracks(id);

    // must save new configuration
    emit saveConfiguration(id, configToJson());
}


// server says unable to start a station
void RadioSource::unableToStart(QUuid uniqueId, QUrl url)
{
    if (uniqueId != id) {
        return;
    }

    // increase counter
    int i = 0;
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
}


// reuest for playlist entries
void RadioSource::getPlaylist(QUuid uniqueId, int maxCount)
{
    if (uniqueId != id) {
        return;
    }

    TracksInfo returnValue;

    // can't do anything if there aren't any stations
    if (selectedStations.count() < 1) {
        // let everybody know
        emit unready(id);
        return;
    }

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

    // random select some stations
    int count = (qrand() % maxCount) + 1;
    for (int i = 1; i <= count; i++) {
        if (selectableStations.count() < 1) {
            break;
        }

        int trackIndex = qrand() % selectableStations.count();

        TrackInfo trackInfo;
        trackInfo.album     = selectableStations.at(trackIndex).category;
        trackInfo.cast      = true;
        trackInfo.performer = "Online Radio";
        trackInfo.title     = selectableStations.at(trackIndex).name;
        trackInfo.track     = 0;
        trackInfo.url       = selectableStations.at(trackIndex).url;
        trackInfo.year      = 0;
        trackInfo.actions.insert(0, "Ban");

        returnValue.append(trackInfo);

        selectableStations.remove(trackIndex);
    }

    // send them back to the server
    emit playlist(id, returnValue);
}


// client wants to dispaly open dialog
void RadioSource::getOpenTracks(QUuid uniqueId, QString parentId)
{
    if (uniqueId != id) {
        return;
    }


    OpenTracks openTracks;

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
    emit openTracksResults(id, openTracks);
}


// turn open dialog selections to playlist entries
void RadioSource::resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks)
{
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
            //trackInfo.actions.insert(0, "Ban");

            returnValue.append(trackInfo);
        }
    }

    emit playlist(id, returnValue);
}


// request for playlist entries based on search criteria
void RadioSource::search(QUuid uniqueId, QString criteria)
{
    if (uniqueId != id) {
        return;
    }

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
}


// user clicked action that was included in track info
void RadioSource::action(QUuid uniqueId, int actionKey, QUrl url)
{
    if (uniqueId != id) {
        return;
    }
}


// configuration conversion
QJsonDocument RadioSource::configToJson()
{
    QJsonArray jsonArray;
    foreach (Station station, stations) {
        QVariantHash data;
        data.insert("category",           station.category);
        data.insert("name",               station.name);
        data.insert("url",                station.url.toString());
        data.insert("unableToStartCount", station.unableToStartCount);

        jsonArray.append(QJsonValue(QJsonObject::fromVariantHash(data)));
    }

    QStringList selectedCategories;
    foreach (Station station, selectedStations) {
        if (!selectedCategories.contains(station.category)) {
            selectedCategories.append(station.category);
        }
    }

    QJsonObject jsonObject;
    jsonObject.insert("stations", QJsonValue(jsonArray));
    jsonObject.insert("selected_categories", QJsonValue(QJsonArray::fromStringList(selectedCategories)));

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
void RadioSource::jsonToConfig(QJsonDocument jsonDocument)
{
    if (jsonDocument.object().contains("stations")) {
        stations.clear();

        foreach (QJsonValue jsonValue, jsonDocument.object().value("stations").toArray()) {
            QVariantHash data = jsonValue.toObject().toVariantHash();

            Station station;
            station.category           = data.value("category").toString();
            station.name               = data.value("name").toString();
            station.url                = QUrl(data.value("url").toString());
            station.unableToStartCount = data.value("unableToStartCount").toInt();

            if (station.url.isValid() && !station.url.isLocalFile()) {
                stations.append(station);
            }
        }
    }

    if (jsonDocument.object().contains("selected_categories")) {
        selectedStations.clear();

        QStringList selectedCategories;
        foreach (QJsonValue jsonValue, jsonDocument.object().value("selected_categories").toArray()) {
            selectedCategories.append(jsonValue.toString());
        }

        foreach (Station station, stations) {
            if (selectedCategories.contains(station.category)) {
                selectedStations.append(station);
            }
        }
    }
}
