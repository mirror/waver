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
    return false;
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

    networkAccessManager = NULL;
    state                = NotYetChecked;
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

    // have to check again if tackInfo was set (the emitting of this signal might have been initiated by some other stuff)
    if (trackInfo.performer.isEmpty() || trackInfo.album.isEmpty()) {
        return;
    }

    // prevent repeated runs
    requestedTrackInfo = trackInfo;
    TrackInfo eraser;
    trackInfo = eraser;

    // check against already failed to limit queries
    bool found = false;
    int  i     = 0;
    while (!found && (i < alreadyFailed.count())) {
        if ((requestedTrackInfo.performer.compare(alreadyFailed.at(i).performer) == 0) && (requestedTrackInfo.album.compare(alreadyFailed.at(i).album) == 0)) {
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

    // construct request
    QNetworkRequest request(QUrl("http://musicbrainz.org/ws/2/release-group/?query=artist:\"" + requestedTrackInfo.performer + "\"release:\"" + requestedTrackInfo.album + "\"&limit=1"));
    request.setRawHeader("User-Agent", userAgent.toUtf8());

    // send request
    networkAccessManager->get(request);
}


// this plugin has no UI
void AlbumArt::getUiQml(QUuid uniqueId)
{
    Q_UNUSED(uniqueId);
}


// this plugin has no UI
void AlbumArt::uiResults(QUuid uniqueId, QJsonDocument results)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(results);
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

    // things that prevent checking
    if (trackInfo.cast || !trackInfo.url.isLocalFile() || (trackInfo.pictures.count() > 0) || trackInfo.performer.isEmpty() || trackInfo.album.isEmpty()) {
        // diagnostics
        state = NotChecked;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        return;
    }

    // technically it's possible to overwrite trackInfo while another request is being processed, but the idea is that each track has its own set of info plugins
    this->trackInfo = trackInfo;

    // actual work takes place after global config is loaded so not trying to query the same thing many times
    emit loadGlobalConfiguration(id);

    // TODO delete alreadyFailed from configuration once each month (not on a fixed date but after interval) to have a chance to pick up stuff that's newly added to musicbrainz
}


// network signal
void AlbumArt::networkFinished(QNetworkReply *reply)
{
    // got reply from musicbrains
    if (reply->url().host().compare("musicbrainz.org") == 0) {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) == 200) {
            QString replyData = reply->readAll();

            // get the id (could parse the XML but simple regular expression is satisfactory here because it was called with limit 1 anyways)
            QRegExp regExp("release-group id=\"(.+)\"");
            regExp.setMinimal(true);
            if (replyData.contains(regExp)) {
                // construct request for image, this must follow redirections because that's how cover art archive answers
                QNetworkRequest request(QUrl("http://coverartarchive.org//release-group/" + regExp.capturedTexts().at(1) + "/front"));
                request.setRawHeader("User-Agent", userAgent.toUtf8());
                request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
                request.setMaximumRedirectsAllowed(12);

                // send request
                networkAccessManager->get(request);
            }
            else {
                // diagnostics
                state = NotFound;
                if (sendDiagnostics) {
                    sendDiagnosticsData();
                }
            }
        }
        else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) == 404) {
            // update configuration
            alreadyFailed.append({ requestedTrackInfo.performer, requestedTrackInfo.album });
            emit saveGlobalConfiguration(id, configToJsonGlobal());

            // diagnostics
            state = NotFound;
            if (sendDiagnostics) {
                sendDiagnosticsData();
            }
        }
        return;
    }

    // can not check the host because of the redirection, but must check the status
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) == 200) {
        // load the image from the data received (this takes care of different formats)
        QImage picture = QImage::fromData(reply->readAll());

        // only checking local files anyways
        QString saveLocation = requestedTrackInfo.url.adjusted(QUrl::RemoveFilename).toLocalFile() + "coverartarchive_waver.jpg";

        // save
        picture.save(saveLocation);

        // let the world know
        TrackInfo resultsTrackInfo;
        resultsTrackInfo.url = requestedTrackInfo.url;
        resultsTrackInfo.pictures.append(QUrl::fromLocalFile(saveLocation));
        emit updateTrackInfo(id, resultsTrackInfo);

        // diagnostics
        state = Success;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
    }
    else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) == 404) {
        // update configuration
        alreadyFailed.append({ requestedTrackInfo.performer, requestedTrackInfo.album });
        emit saveGlobalConfiguration(id, configToJsonGlobal());

        // diagnostics
        state = NotFound;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
    }
}


// configuration conversion
QJsonDocument AlbumArt::configToJsonGlobal()
{
    QJsonArray jsonArray;
    foreach (PerformerAlbum performerAlbum, alreadyFailed) {
        jsonArray.append(QJsonArray({ performerAlbum.performer, performerAlbum.album }));
    }

    QJsonObject jsonObject;
    jsonObject.insert("alreadyFailed", jsonArray);

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
void AlbumArt::jsonToConfigGlobal(QJsonDocument jsonDocument)
{
    if (jsonDocument.object().contains("alreadyFailed")) {
        alreadyFailed.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("alreadyFailed").toArray()) {
            QJsonArray value = jsonValue.toArray();
            alreadyFailed.append({ value.at(0).toString(), value.at(1).toString() });
        }
    }
}


// helper
void AlbumArt::sendDiagnosticsData()
{
    DiagnosticData diagnosticData;

    switch (state) {
        case NotYetChecked:
            diagnosticData.append({ "Status", "Not started yet" });
            break;
        case NotChecked:
            diagnosticData.append({ "Status", "Not checked" });
            break;
        case Success:
            diagnosticData.append({ "Status", "Success" });
            break;
        case InAlreadyFailed:
            diagnosticData.append({ "Status", "Already checked and not found" });
            break;
        case NotFound:
            diagnosticData.append({ "Status", "Not found" });
            break;
    }

    emit diagnostics(id, diagnosticData);
}
