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


#include "albumart.h"


// plugin factory
void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal)
{
    if (pluginTypesMask & PLUGIN_TYPE_INFO) {
        retVal->append((QObject *) new AlbumArt());
    }
}


// global method
int AlbumArt::pluginType()
{
    return PLUGIN_TYPE_INFO;
}


// global method
QString AlbumArt::pluginName()
{
    return "Album Art Finder";
}


// global method
int AlbumArt::pluginVersion()
{
    return 1;
}


// overrided virtual function
QString AlbumArt::waverVersionAPICompatibility()
{
    return "0.0.4";
}


// global method
bool AlbumArt::hasUI()
{
    return true;
}


// global method
QUuid AlbumArt::persistentUniqueId()
{
    return id;
}


// global method
void AlbumArt::setUrl(QUrl url)
{
    Q_UNUSED(url);
}


// global method
void AlbumArt::setUserAgent(QString userAgent)
{
    this->userAgent = userAgent;
}


// constructor
AlbumArt::AlbumArt()
{
    id = QUuid("{1AEC5C13-454B-48BB-AA2A-93246243EC87}");

    networkAccessManager              = NULL;
    state                             = NotStartedYet;
    sendDiagnostics                   = false;
    checkAlways                       = true;
    allowLooseMatch                   = true;
    allowLooseMatchOnlyIfNoOtherExist = true;
}


// destructor
AlbumArt::~AlbumArt()
{
    if (networkAccessManager != NULL) {
        networkAccessManager->deleteLater();
    }
}


// thread entry point
void AlbumArt::run()
{
    networkAccessManager = new QNetworkAccessManager();
    connect(networkAccessManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(networkFinished(QNetworkReply *)));

    emit loadGlobalConfiguration(id);
}


// configuration
void AlbumArt::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// configuration
void AlbumArt::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfigGlobal(configuration);

    // have to check again if tackInfo was set (signal might came from another track)
    if (trackInfo.performer.isEmpty() || trackInfo.album.isEmpty()) {
        return;
    }

    // prevent repeated runs
    requestedTrackInfo = trackInfo;
    TrackInfo eraser;
    eraser.track = 0;
    eraser.year  = 0;
    trackInfo = eraser;

    // check against already failed to limit queries
    bool found = false;
    int  i     = 0;
    while (!found && (i < alreadyFailed.count())) {
        if ((requestedTrackInfo.performer.compare(alreadyFailed.at(i).performer, Qt::CaseInsensitive) == 0) && (requestedTrackInfo.album.compare(alreadyFailed.at(i).album, Qt::CaseInsensitive) == 0)) {
            found = true;
        }
        i++;
    }
    if (found) {
        // diagnostics
        state = InAlreadyFailed;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        return;
    }

    // diagnostics
    state = CheckStarted;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    // send request
    QNetworkRequest request(QUrl("http://musicbrainz.org/ws/2/release-group/?query=artist:\"" + requestedTrackInfo.performer + "\"release:\"" + requestedTrackInfo.album + "\""));
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    networkAccessManager->get(request);
}


// UI
void AlbumArt::getUiQml(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    QFile settingsFile("://AA_Settings.qml");
    settingsFile.open(QFile::ReadOnly);
    QString settings = settingsFile.readAll();
    settingsFile.close();

    settings.replace("check_always_value",           checkAlways                       ? "true" : "false");
    settings.replace("allow_loose_match_value",      allowLooseMatch                   ? "true" : "false");
    settings.replace("only_if_no_other_exist_value", allowLooseMatchOnlyIfNoOtherExist ? "true" : "false");

    emit uiQml(id, settings);
}


// UI
void AlbumArt::uiResults(QUuid uniqueId, QJsonDocument results)
{
    if (uniqueId != id) {
        return;
    }

    // going from strict to loose, have to give a chance to re-check
    if (!allowLooseMatch && results.object().value("allow_loose_match").toBool()) {
        alreadyFailed.clear();
    }
    if (allowLooseMatchOnlyIfNoOtherExist && !results.object().value("only_if_no_other_exist").toBool()) {
        alreadyFailed.clear();
    }

    checkAlways                       = results.object().value("check_always").toBool();
    allowLooseMatch                   = results.object().value("allow_loose_match").toBool();
    allowLooseMatchOnlyIfNoOtherExist = results.object().value("only_if_no_other_exist").toBool();

    emit saveGlobalConfiguration(id, configToJsonGlobal());
}


// client wants to receive updates of this plugin's diagnostic information
void AlbumArt::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = true;
    sendDiagnosticsData();
}


// client doesnt want to receive updates of this plugin's diagnostic information anymore
void AlbumArt::stopDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = false;
}


// get the album art of a track
void AlbumArt::getInfo(QUuid uniqueId, TrackInfo trackInfo)
{
    if (uniqueId != id) {
        return;
    }

    // check if picture saved by this plugin exists
    bool foundOurPicture = false;
    if (checkAlways) {
        QUrl lookingFor = QUrl::fromLocalFile(pictureFileName(trackInfo));
        foreach (QUrl url, trackInfo.pictures) {
            if (url == lookingFor) {
                foundOurPicture = true;
                break;
            }
        }
    }

    // things that prevent checking
    if (!trackInfo.url.isLocalFile() || (checkAlways && foundOurPicture) || (!checkAlways && (trackInfo.pictures.count() > 0))) {
        // diagnostics
        state = NotToBeChecked;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        return;
    }
    if (trackInfo.performer.isEmpty() || trackInfo.album.isEmpty()) {
        // diagnostics
        state = CanNotCheck;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        return;
    }

    // technically it's possible to overwrite trackInfo while another request is being processed, but the idea is that each track has its own set of info plugins
    this->trackInfo = trackInfo;

    // actual work takes place after global config is loaded to prevent querying the same thing many times
    emit loadGlobalConfiguration(id);
}


// network signal
void AlbumArt::networkFinished(QNetworkReply *reply)
{
    // got reply from musicbrains
    if (reply->url().host().compare("musicbrainz.org") == 0) {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 200) {
            // update configuration
            alreadyFailed.append({ requestedTrackInfo.performer, requestedTrackInfo.album });
            emit saveGlobalConfiguration(id, configToJsonGlobal());

            // diagnostics
            state = NotFound;
            if (sendDiagnostics) {
                sendDiagnosticsData();
            }
            return;
        }

        // parse and process the xml

        QString releaseGroupId;
        QString currentReleaseGroupId;
        bool    titleMatch;
        bool    found               = false;
        bool    inReleaseGroup      = false;
        bool    inReleaseGroupTitle = false;
        bool    inArtist            = false;
        bool    inArtistName        = false;

        QXmlStreamReader xmlStreamReader(reply);
        while (!xmlStreamReader.atEnd() && !found) {
            QXmlStreamReader::TokenType tokenType = xmlStreamReader.readNext();

            if (tokenType == QXmlStreamReader::StartElement) {
                if (xmlStreamReader.name().toString().compare("release-group") == 0) {
                    QXmlStreamAttributes attributes = xmlStreamReader.attributes();
                    if (attributes.hasAttribute("id")) {
                        currentReleaseGroupId = attributes.value("id").toString();
                        inReleaseGroup = true;
                    }
                }
                if (inReleaseGroup && (xmlStreamReader.name().toString().compare("title") == 0)) {
                    inReleaseGroupTitle = true;
                }
                if (inReleaseGroup && (xmlStreamReader.name().toString().compare("artist") == 0)) {
                    inArtist = true;
                }
                if (inArtist && (xmlStreamReader.name().toString().compare("name") == 0)) {
                    inArtistName = true;
                }
            }

            if (tokenType == QXmlStreamReader::Characters) {
                if (inReleaseGroupTitle) {
                    titleMatch = (xmlStreamReader.text().toString().compare(requestedTrackInfo.album.trimmed(), Qt::CaseInsensitive) == 0);
                }
                if (inArtistName) {
                    if (xmlStreamReader.text().toString().compare(requestedTrackInfo.performer.trimmed(), Qt::CaseInsensitive) == 0) {
                        if (titleMatch) {
                            releaseGroupId = currentReleaseGroupId;
                            found = true;
                        }
                        else if (releaseGroupId.isEmpty() && allowLooseMatch && (!allowLooseMatchOnlyIfNoOtherExist || (allowLooseMatchOnlyIfNoOtherExist && requestedTrackInfo.pictures.count() < 1))) {
                            releaseGroupId = currentReleaseGroupId;
                        }
                    }
                }
            }

            if (tokenType == QXmlStreamReader::EndElement) {
                if (xmlStreamReader.name().toString().compare("release-group") == 0) {
                    inReleaseGroup      = false;
                    inReleaseGroupTitle = false;
                }
                if (xmlStreamReader.name().toString().compare("title") == 0) {
                    inReleaseGroupTitle = false;
                }
                if (xmlStreamReader.name().toString().compare("artist") == 0) {
                    inArtist     = false;
                    inArtistName = false;
                }
                if (xmlStreamReader.name().toString().compare("name") == 0) {
                    inArtistName = false;
                }
            }
        }
        reply->readAll();

        // not found?
        if (releaseGroupId.isEmpty()) {
            // update configuration
            alreadyFailed.append({ requestedTrackInfo.performer, requestedTrackInfo.album });
            emit saveGlobalConfiguration(id, configToJsonGlobal());

            // diagnostics
            state = NotFound;
            if (sendDiagnostics) {
                sendDiagnosticsData();
            }
            return;
        }

        // construct request for image, this must follow redirections because that's how cover art archive answers
        QNetworkRequest request(QUrl("http://coverartarchive.org//release-group/" + releaseGroupId + "/front"));
        request.setRawHeader("User-Agent", userAgent.toUtf8());
        request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        request.setMaximumRedirectsAllowed(12);

        // send request
        networkAccessManager->get(request);
    }

    // can not check the host because of the redirection, but must check the status
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 200) {
        // update configuration
        alreadyFailed.append({ requestedTrackInfo.performer, requestedTrackInfo.album });
        emit saveGlobalConfiguration(id, configToJsonGlobal());

        // diagnostics
        state = NotFound;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        return;
    }

    // load the image from the data received (this takes care of different formats)
    QImage picture = QImage::fromData(reply->readAll());

    // let's just be on the safe side
    if (picture.format() == QImage::Format_Invalid) {
        return;
    }

    // save
    picture.save(pictureFileName(requestedTrackInfo));

    // let the world know after a bit of a delay, experienced problems with being too quick here
    QTimer::singleShot(500, this, SLOT(signalTimer()));

    // diagnostics
    state = Success;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// slot for timer
void AlbumArt::signalTimer()
{
    TrackInfo resultsTrackInfo;
    resultsTrackInfo.track = 0;
    resultsTrackInfo.url   = requestedTrackInfo.url;
    resultsTrackInfo.year  = 0;
    resultsTrackInfo.pictures.append(QUrl::fromLocalFile(pictureFileName(requestedTrackInfo)));
    emit updateTrackInfo(id, resultsTrackInfo);
}


// configuration conversion
QJsonDocument AlbumArt::configToJsonGlobal()
{
    QJsonArray jsonArray;
    foreach (PerformerAlbum performerAlbum, alreadyFailed) {
        jsonArray.append(QJsonArray({ performerAlbum.performer, performerAlbum.album }));
    }

    QJsonObject jsonObject;
    jsonObject.insert("already_failed", jsonArray);
    jsonObject.insert("check_always", checkAlways);
    jsonObject.insert("allow_loose_match", allowLooseMatch);
    jsonObject.insert("only_if_no_other_exist", allowLooseMatchOnlyIfNoOtherExist);

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
void AlbumArt::jsonToConfigGlobal(QJsonDocument jsonDocument)
{
    if (jsonDocument.object().contains("already_failed")) {
        alreadyFailed.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("already_failed").toArray()) {
            QJsonArray value = jsonValue.toArray();
            alreadyFailed.append({ value.at(0).toString(), value.at(1).toString() });
        }
    }
    if (jsonDocument.object().contains("check_always")) {
        checkAlways = jsonDocument.object().value("check_always").toBool();
    }
    if (jsonDocument.object().contains("allow_loose_match")) {
        allowLooseMatch = jsonDocument.object().value("allow_loose_match").toBool();
    }
    if (jsonDocument.object().contains("only_if_no_other_exist")) {
        allowLooseMatchOnlyIfNoOtherExist = jsonDocument.object().value("only_if_no_other_exist").toBool();
    }
}


// helper
void AlbumArt::sendDiagnosticsData()
{
    DiagnosticData diagnosticData;

    switch (state) {
        case NotStartedYet:
            diagnosticData.append({ "Status", "Not started yet"});
            break;
        case NotToBeChecked:
            diagnosticData.append({ "Status", "Not needed to be checked" });
            break;
        case CanNotCheck:
            diagnosticData.append({ "Status", "Not enough info, can not check" });
            break;
        case InAlreadyFailed:
            diagnosticData.append({ "Status", "Already checked and not found" });
            break;
        case CheckStarted:
            diagnosticData.append({ "Status", "Checking..." });
            break;
        case Success:
            diagnosticData.append({ "Status", "Success" });
            break;
        case NotFound:
            diagnosticData.append({ "Status", "Not found" });
            break;
    }

    emit diagnostics(id, diagnosticData);
}


// helper
QString AlbumArt::pictureFileName(TrackInfo pictureTrackInfo)
{
    QString performer = pictureTrackInfo.performer;
    QString album     = pictureTrackInfo.album;

    performer.replace(QRegExp("\\W"), "");
    performer.replace(QRegExp("_"), "");
    album.replace(QRegExp("\\W"), "");
    album.replace(QRegExp("_"), "");

    performer = performer.toLower();
    album     = album.toLower();

    return pictureTrackInfo.url.adjusted(QUrl::RemoveFilename).toLocalFile() + performer + "_" + album + "_caa_waver.jpg";
}
