/*
    This file is part of Waver

    Copyright (C) 2017-2020 Peter Papp <peter.papp.p@gmail.com>

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


#include "acoustid.h"


// global method
int Acoustid::pluginType()
{
    return PLUGIN_TYPE_INFO;
}


// global method
QString Acoustid::pluginName()
{
    return "AcoustID";
}


// global method
int Acoustid::pluginVersion()
{
    return 1;
}


// overrided virtual function
QString Acoustid::waverVersionAPICompatibility()
{
    return "0.0.6";
}


// global method
bool Acoustid::hasUI()
{
    return false;
}


// global method
QUuid Acoustid::persistentUniqueId()
{
    return id;
}


// global method
void Acoustid::setUrl(QUrl url)
{
    Q_UNUSED(url);
}


// global method
void Acoustid::setUserAgent(QString userAgent)
{
    this->userAgent = userAgent;
}


// constructor
Acoustid::Acoustid()
{
    id = QUuid("{EDEA3392-28E8-4C57-A8AE-EAB5864D5E4B}");

    networkAccessManager = nullptr;
    sendDiagnostics      = false;
    duration             = 0;

    state = NotStartedYet;
}


// desctructor
Acoustid::~Acoustid()
{
    if (networkAccessManager != nullptr) {
        networkAccessManager->deleteLater();
    }
}


// thread entry point
void Acoustid::run()
{
    networkAccessManager = new QNetworkAccessManager();
    connect(networkAccessManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(networkFinished(QNetworkReply *)));

    emit loadGlobalConfiguration(id);
}


// configuration
void Acoustid::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// configuration
void Acoustid::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    jsonToConfigGlobal(configuration);
}


// configuration
void Acoustid::sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void Acoustid::globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void Acoustid::sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(error);
}


// message from another plugin
void Acoustid::messageFromPlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value)
{
    if (uniqueId != id) {
        return;
    }

    if (sourceUniqueId != QUuid("{632F2309-5D2F-4007-BD66-E0620D885584}")) {
        return;
    }

    if (messageId == 1) {
        bool OK = false;
        qint64 duration = value.toLongLong(&OK);
        if (OK) {
            this->duration = duration / (1000 * 1000);
        }
    }

    if (messageId == 2) {
        chromaprint = value.toString();
    }

    if (isReadyToQuery()) {
        query();
    }
}


// UI
void Acoustid::getUiQml(QUuid uniqueId)
{
    Q_UNUSED(uniqueId);
}


// UI
void Acoustid::uiResults(QUuid uniqueId, QJsonDocument results)
{
    if (uniqueId != id) {
        return;
    }

    TrackInfo trackInfo;
    trackInfo.url       = QUrl(results.object().value("url").toString());
    trackInfo.performer = results.object().value("performer").toString();
    trackInfo.album     = results.object().value("album").toString();
    trackInfo.title     = results.object().value("title").toString();

    QString tempStr = results.object().value("year").toString();
    bool    OK      = false;
    int     tempInt = tempStr.toInt(&OK);
    if (OK) {
        trackInfo.year = tempInt;
    }

    tempStr = results.object().value("track").toString();
    OK      = false;
    tempInt = tempStr.toInt(&OK);
    if (OK) {
        trackInfo.track = tempInt;
    }

    emit updateTrackInfo(id, trackInfo);
}


// client wants to receive updates of this plugin's diagnostic information
void Acoustid::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = true;
    sendDiagnosticsData();
}


// client doesnt want to receive updates of this plugin's diagnostic information anymore
void Acoustid::stopDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = false;
}


// get the album art of a track
void Acoustid::getInfo(QUuid uniqueId, TrackInfo trackInfo, QVariantHash additionalInfo)
{
    if (uniqueId != id) {
        return;
    }

    this->trackInfo = trackInfo;
    this->additionalInfo = additionalInfo;

    if (isReadyToQuery()) {
        query();
    }
}


void Acoustid::action(QUuid uniqueId, int actionKey, TrackInfo trackInfo)
{
    if (uniqueId != id) {
        return;
    }

    if (trackInfo.url != this->trackInfo.url) {
        return;
    }

    if (actionKey == 1) {
        query();
    }
}


// get the key (this simple little "decryption" is here just so that the key is not included in the source code in plain text format)
//
// WARNING! The key is exempt from the terms of the GNU General Public License. Usage of the key is resticted to the developers of Waver.
//          Please get your own key if you need one, it's free. Check 'https://acoustid.org/' for details.
//
QString Acoustid::getKey()
{
    QFile keyFile("://acoustid_key");
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


bool Acoustid::isReadyToQuery()
{
    if (!QThread::currentThread()->isRunning()) {
        state = NotStartedYet;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        return false;
    }
    if (!trackInfo.url.isValid()) {
        state = NotStartedYet;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        return false;
    }
    if (trackInfo.cast) {
        state = Cast;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        return false;
    }
    if ((duration <= 0) || chromaprint.isEmpty()) {
        state = WaitingForChromaprint;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        return false;
    }

    if (!additionalInfo.contains("incomplete_tags") || !additionalInfo.value("incomplete_tags").toInt()) {
        bool found = false;
        foreach (TrackAction action, trackInfo.actions) {
            if ((action.pluginId == id) && (action.id == 1)) {
                found = true;
                break;
            }
        }
        if (!found) {
            TrackAction newAction;
            newAction.id       = 1;
            newAction.label    = "AcoustID lookup";
            newAction.pluginId = id;

            TrackInfo trackInfoNewAction;
            trackInfoNewAction.url = trackInfo.url;
            trackInfoNewAction.track = 0;
            trackInfoNewAction.year  = 0;
            trackInfoNewAction.actions.append(newAction);

            emit updateTrackInfo(id, trackInfoNewAction);

            trackInfo.actions.append(newAction);
        }

        state = NotCheckingAutomatically;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        return false;
    }

    foreach (Failed failed, alreadyFailed) {
        if (failed.url == trackInfo.url) {
            state     = InAlreadyFailed;
            nextCheck = QDateTime::fromMSecsSinceEpoch(failed.timestamp).addDays(ALREADY_FAILED_EXPIRY_DAYS);
            if (sendDiagnostics) {
                sendDiagnosticsData();
            }
            return false;
        }
    }

    return true;
}


void Acoustid::query()
{
    // TODO save already checked with timestamp

    QNetworkRequest request(QUrl("http://api.acoustid.org/v2/lookup"));
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QStringList postData;
    postData.append(QString("client=%1").arg(getKey()));
    postData.append("meta=recordingids");
    postData.append(QString("duration=%1").arg(duration));
    postData.append("fingerprint=" + chromaprint);

    networkAccessManager->post(request, postData.join("&").toUtf8());

    state = CheckStarted;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// network signal
void Acoustid::networkFinished(QNetworkReply *reply)
{
    // got reply from acoustID
    if (reply->url().host().compare("api.acoustid.org") == 0) {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 200) {
            addToAlreadyFailed();
            emit saveGlobalConfiguration(id, configToJsonGlobal());

            state = NotFound;
            if (sendDiagnostics) {
                sendDiagnosticsData();
            }
            emit infoMessage(id, "<INFO>AcoustID invalid reply");

            reply->deleteLater();
            return;
        }

        QJsonDocument jsonDocument = QJsonDocument::fromJson(reply->readAll());
        if (!jsonDocument.object().contains("status") || (jsonDocument.object().value("status").toString().compare("ok") != 0) || !jsonDocument.object().contains("results")) {
            addToAlreadyFailed();
            emit saveGlobalConfiguration(id, configToJsonGlobal());

            state = NotFound;
            if (sendDiagnostics) {
                sendDiagnosticsData();
            }
            emit infoMessage(id, "<INFO>AcoustID invalid reply");

            reply->deleteLater();
            return;
        }

        QJsonArray results = jsonDocument.object().value("results").toArray();
        if (results.count() < 1) {
            addToAlreadyFailed();
            emit saveGlobalConfiguration(id, configToJsonGlobal());

            state = NotFound;
            if (sendDiagnostics) {
                sendDiagnosticsData();
            }
            emit infoMessage(id, "<INFO>AcoustID not found");

            reply->deleteLater();
            return;
        }

        // get all recordings from acoustID results
        foreach (QJsonValue jsonValue, results) {
            if (!jsonValue.toObject().contains("recordings")) {
                continue;
            }

            double score = jsonValue.toObject().value("score").toDouble();

            foreach (QJsonValue recordingJsonValue, jsonValue.toObject().value("recordings").toArray()) {
                Recording recording = { score, recordingJsonValue.toObject().value("id").toString() };
                recordings.append(recording);
            }
        }

        reply->readAll();
        reply->deleteLater();

        if (recordings.count() < 1) {
            emit infoMessage(id, "<INFO>AcoustID recording not found");
            return;
        }

        // keep only recordings with highest score (more than one can have the same score)
        qSort(recordings.begin(), recordings.end(), [](Recording a, Recording b) {
            return (std::isless(a.score, b.score));
        });
        while (recordings.first().score != recordings.last().score) {
            recordings.removeFirst();
        }

        // take first recording
        Recording recording = recordings.first();
        recordings.removeFirst();

        // query on musicbrainz
        QNetworkRequest request(QUrl("http://musicbrainz.org/ws/2/recording/?query=rid:" + recording.id));
        request.setRawHeader("User-Agent", userAgent.toUtf8());
        networkAccessManager->get(request);

        return;
    }

    // reply from musicbrainz
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 200) {
        addToAlreadyFailed();
        emit saveGlobalConfiguration(id, configToJsonGlobal());

        state = NotFound;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        emit infoMessage(id, "<INFO>musicbrainz invalid reply");

        reply->deleteLater();
        return;
    }

    // get releases by parsing XML

    Result result;
    result.trackInfo.track = 0;
    result.trackInfo.year  = 0;
    result.duration        = 0;

    bool inRecording           = false;
    bool inRecordingTitle      = false;
    bool inRecordingArtist     = false;
    bool inRecordingArtistName = false;
    bool inRelease             = false;
    bool inReleaseTitle        = false;
    bool inReleaseLength       = false;
    bool inReleaseNumber       = false;
    bool inReleaseDate         = false;

    QXmlStreamReader xmlStreamReader(reply);
    while (!xmlStreamReader.atEnd()) {
        QXmlStreamReader::TokenType tokenType = xmlStreamReader.readNext();

        if (tokenType == QXmlStreamReader::StartElement) {
            if (xmlStreamReader.name().toString().compare("recording") == 0) {
                inRecording = true;
            }
            if (xmlStreamReader.name().toString().compare("release") == 0) {
                inRelease = true;
            }
            if (xmlStreamReader.name().toString().compare("title") == 0) {
                if (inRelease) {
                    inReleaseTitle = true;
                }
                else if (inRecording) {
                    inRecordingTitle = true;
                }
            }
            if (inRecording && !inRelease && (xmlStreamReader.name().toString().compare("artist") == 0)) {
                inRecordingArtist = true;
            }
            if (inRecordingArtist && (xmlStreamReader.name().toString().compare("name") == 0)) {
                inRecordingArtistName = true;
            }
            if (inRelease && (xmlStreamReader.name().toString().compare("length") == 0)) {
                inReleaseLength = true;
            }
            if (inRelease && (xmlStreamReader.name().toString().compare("number") == 0)) {
                inReleaseNumber = true;
            }
            if (inRelease && (xmlStreamReader.name().toString().compare("date") == 0)) {
                inReleaseDate = true;
            }
        }

        if (tokenType == QXmlStreamReader::Characters) {
            if (inRecordingTitle) {
                result.trackInfo.title = xmlStreamReader.text().toString();
            }
            if (inRecordingArtistName) {
                result.trackInfo.performer = xmlStreamReader.text().toString();
            }
            if (inReleaseTitle && result.trackInfo.album.isEmpty()) {
                result.trackInfo.album = xmlStreamReader.text().toString();
            }
            if (inReleaseLength) {
                QString str = xmlStreamReader.text().toString();
                bool OK = false;
                qint64 duration = str.toLongLong(&OK);
                if (OK) {
                    result.duration = duration / 1000;
                }
            }
            if (inReleaseNumber) {
                QString str = xmlStreamReader.text().toString();
                bool OK = false;
                int track = str.toInt(&OK);
                if (OK) {
                    result.trackInfo.track = track;
                }
            }
            if (inReleaseDate) {
                QString str = xmlStreamReader.text().toString();
                str = str.left(4);
                bool OK = false;
                int year = str.toInt(&OK);
                if (OK) {
                    result.trackInfo.year = year;
                }
            }
        }

        if (tokenType == QXmlStreamReader::EndElement) {
            if (xmlStreamReader.name().toString().compare("recording") == 0) {
                inRecording = false;

                result.trackInfo.title     = "";
                result.trackInfo.performer = "";
            }
            if (xmlStreamReader.name().toString().compare("release") == 0) {
                inRelease = false;

                results.append(result);

                result.trackInfo.album = "";
                result.trackInfo.track = 0;
                result.trackInfo.year  = 0;
                result.duration        = 0;
            }
            if (xmlStreamReader.name().toString().compare("title") == 0) {
                inRecordingTitle = false;
                inReleaseTitle = false;
            }
            if (xmlStreamReader.name().toString().compare("artist") == 0) {
                inRecordingArtist  = false;
            }
            if (xmlStreamReader.name().toString().compare("name") == 0) {
                inRecordingArtistName = false;
            }
            if (xmlStreamReader.name().toString().compare("length") == 0) {
                inReleaseLength = false;
            }
            if (xmlStreamReader.name().toString().compare("number") == 0) {
                inReleaseNumber = false;
            }
            if (xmlStreamReader.name().toString().compare("date") == 0) {
                inReleaseDate = false;
            }
        }
    }
    reply->readAll();
    reply->deleteLater();

    // query next recoding on musicbrainz
    if (recordings.count() > 0) {
        QThread::currentThread()->msleep(250);

        Recording recording = recordings.first();
        recordings.removeFirst();

        QNetworkRequest request(QUrl("http://musicbrainz.org/ws/2/recording/?query=rid:" + recording.id));
        request.setRawHeader("User-Agent", userAgent.toUtf8());
        networkAccessManager->get(request);

        return;
    }

    // no more recordings, let's select one

    // remove duplicates
    qSort(results.begin(), results.end(), [](Result a, Result b) {
        if (a.trackInfo.performer.compare(b.trackInfo.performer, Qt::CaseInsensitive) < 0) {
            return true;
        }
        if (a.trackInfo.performer.compare(b.trackInfo.performer, Qt::CaseInsensitive) > 0) {
            return false;
        }
        if (a.trackInfo.album.compare(b.trackInfo.album, Qt::CaseInsensitive) < 0) {
            return true;
        }
        if (a.trackInfo.album.compare(b.trackInfo.album, Qt::CaseInsensitive) > 0) {
            return false;
        }
        if (a.trackInfo.title.compare(b.trackInfo.title, Qt::CaseInsensitive) < 0) {
            return true;
        }
        if (a.trackInfo.title.compare(b.trackInfo.title, Qt::CaseInsensitive) > 0) {
            return false;
        }
        return (a.duration < b.duration);
    });
    int i = 0;
    while (i < results.count()) {
        int j = i + 1;
        while (j < results.count()) {
            if ((results.at(i).trackInfo.performer.compare(results.at(j).trackInfo.performer, Qt::CaseInsensitive) == 0) && (results.at(i).trackInfo.album.compare(results.at(j).trackInfo.album, Qt::CaseInsensitive) == 0) && (results.at(i).trackInfo.title.compare(results.at(j).trackInfo.title, Qt::CaseInsensitive) == 0) && (results.at(i).duration == results.at(j).duration)) {
                results.remove(j);
                continue;
            }
            j++;
        }
        i++;
    }

    // eliminate based on duration
    qint64 minDurationDifference = duration;
    i = 0;
    while (i < results.count()) {
        results[i].durationDifference = std::abs(duration - results.at(i).duration);
        if (results.at(i).durationDifference < minDurationDifference) {
            minDurationDifference = results.at(i).durationDifference;
        }
        i++;
    }
    minDurationDifference = qMin(static_cast<qint64>(4), minDurationDifference);
    i = 0;
    while (i < results.count()) {
        if ((results.at(i).duration != 0) && (results.at(i).durationDifference > (minDurationDifference + 2))) {
            results.remove(i);
            continue;
        }
        i++;
    }

    if (results.count() < 1) {
        addToAlreadyFailed();
        emit saveGlobalConfiguration(id, configToJsonGlobal());

        state = NotFound;
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
        emit infoMessage(id, "<INFO>musicbrainz not found");
        return;
    }

    // file-based source plugins try to guess based on dir structure and file name, let's score that

    QStringList sourceWords;

    QString tmp = trackInfo.performer;
    sourceWords.append(tmp.replace("_", " ").split(" "));
    tmp = trackInfo.title;
    sourceWords.append(tmp.replace("_", " ").split(" "));
    tmp = trackInfo.album;
    sourceWords.append(tmp.replace("_", " ").split(" "));

    sourceWords.removeAll("");
    sourceWords.removeDuplicates();

    i = 0;
    while (i < results.count()) {
        double score = 0;

        if (sourceWords.count() > 0) {
            QStringList resultWords;

            tmp = results.at(i).trackInfo.performer;
            resultWords.append(tmp.replace("_", " ").split(" "));

            tmp = results.at(i).trackInfo.title;
            resultWords.append(tmp.replace("_", " ").split(" "));

            tmp = results.at(i).trackInfo.album;
            resultWords.append(tmp.replace("_", " ").split(" "));

            resultWords.removeAll("");
            resultWords.removeDuplicates();

            foreach (QString sourceWord, sourceWords) {
                if (resultWords.contains(sourceWord, Qt::CaseInsensitive)) {
                    score = score + 1;
                }
            }
            foreach (QString resultWord, resultWords) {
                if (sourceWords.contains(resultWord, Qt::CaseInsensitive)) {
                    score = score + 1;
                }
            }
            score = score / (sourceWords.count() + resultWords.count());
        }

        results[i].score = score;

        i++;
    }

    // select the highest score and oldest release (reasoning that the oldest must be the original) (not doing anything special with those that have no release date, sometimes they are the best choice sometimes not, there's no way to predict it)
    qSort(results.begin(), results.end(), [](Result a, Result b) {
        if (std::isgreater(a.score, b.score)) {
            return  true;
        }
        if (std::isless(a.score, b.score)) {
            return  false;
        }
        return (a.trackInfo.year < b.trackInfo.year);
    });

    result = results.at(0);

    QFile settingsFile("://TrackTags.qml");
    settingsFile.open(QFile::ReadOnly);
    QString settings = settingsFile.readAll();
    settingsFile.close();

    settings.replace("<url>", trackInfo.url.toString());
    settings.replace("<currentPerformer>", trackInfo.performer);
    settings.replace("<currentAlbum>", trackInfo.album);
    settings.replace("<currentTitle>", trackInfo.title);
    settings.replace("<currentYear>", QString("%1").arg(trackInfo.year));
    settings.replace("<currentTrack>", QString("%1").arg(trackInfo.track));
    settings.replace("<newPerformer>", result.trackInfo.performer);
    settings.replace("<newAlbum>", result.trackInfo.album);
    settings.replace("<newTitle>", result.trackInfo.title);
    settings.replace("<newYear>", QString("%1").arg(result.trackInfo.year));
    settings.replace("<newTrack>", QString("%1").arg(result.trackInfo.track));

    emit uiQml(id, settings);

    state = Success;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// configuration conversion
QJsonDocument Acoustid::configToJsonGlobal()
{
    QJsonArray jsonArray;
    foreach (Failed failed, alreadyFailed) {
        jsonArray.append(QJsonArray({ failed.url.toString(), failed.timestamp }));
    }

    QJsonObject jsonObject;
    jsonObject.insert("already_failed", jsonArray);

    QJsonDocument returnValue;
    returnValue.setObject(jsonObject);

    return returnValue;
}


// configuration conversion
void Acoustid::jsonToConfigGlobal(QJsonDocument jsonDocument)
{
    if (jsonDocument.object().contains("already_failed")) {
        alreadyFailed.clear();
        foreach (QJsonValue jsonValue, jsonDocument.object().value("already_failed").toArray()) {
            QJsonArray value = jsonValue.toArray();

            if (QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(value.at(1).toDouble())).daysTo(QDateTime::currentDateTime()) > ALREADY_FAILED_EXPIRY_DAYS) {
                continue;
            }
            alreadyFailed.append({ QUrl(value.at(0).toString()), static_cast<qint64>(value.at(1).toDouble()) });
        }
    }
}


// helper
void Acoustid::addToAlreadyFailed()
{
    bool found = false;
    foreach (Failed failed, alreadyFailed) {
        if (failed.url == trackInfo.url) {
            found = true;
            break;
        }
    }
    if (!found) {
        alreadyFailed.append({ trackInfo.url.toString(), QDateTime::currentMSecsSinceEpoch() });
    }
}


// helper
void Acoustid::sendDiagnosticsData()
{
    DiagnosticData diagnosticData;

    switch (state) {
        case NotStartedYet:
            diagnosticData.append((DiagnosticItem){ "Status", "No started yet" });
            break;
        case Cast:
            diagnosticData.append((DiagnosticItem){ "Status", "Radio station or other live stream" });
            break;
        case WaitingForChromaprint:
            diagnosticData.append((DiagnosticItem){ "Status", "Waiting for Chromaprint Analyzer to finish" });
            break;
        case NotCheckingAutomatically:
            diagnosticData.append((DiagnosticItem){ "Status", "Not needed to be checked automatically" });
            break;
        case InAlreadyFailed:
            diagnosticData.append((DiagnosticItem){ "Status", "Already checked and not found" });
            diagnosticData.append((DiagnosticItem){ "Next check", QString("On or after %1").arg(nextCheck.toString("yyyy/MM/dd")) });
            break;
        case CheckStarted:
            diagnosticData.append((DiagnosticItem){ "Status", "Checking..." });
            break;
        case Success:
            diagnosticData.append((DiagnosticItem){ "Status", "Success" });
            break;
        case NotFound:
            diagnosticData.append((DiagnosticItem){ "Status", "Not found" });
            break;
    }

    emit diagnostics(id, diagnosticData);
}

