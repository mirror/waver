#include "analyzer.h"

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


#include "analyzer.h"

// overriden virtual function
int Analyzer::pluginType()
{
    return PLUGIN_TYPE_DSP_PRE;
}


// overriden virtual function
QString Analyzer::pluginName()
{
    return "Chromaprint Analyzer";
}


// overrided virtual function
int Analyzer::pluginVersion()
{
    return 1;
}


// overrided virtual function
QString Analyzer::waverVersionAPICompatibility()
{
    return "0.0.6";
}

// overriden virtual function
QUuid Analyzer::persistentUniqueId()
{
    return id;
}


// overriden virtual function
bool Analyzer::hasUI()
{
    return true;
}


// overriden virtual function
int Analyzer::priority()
{
    return 2;
}


// overriden virtual function
void Analyzer::setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex)
{
    this->bufferQueue      = bufferQueue;
    this->bufferQueueMutex = bufferQueueMutex;
}


// overriden virtual function
void Analyzer::setCast(bool cast)
{
    this->cast = cast;
}


// constructor
Analyzer::Analyzer()
{
    id = QUuid("{632F2309-5D2F-4007-BD66-E0620D885584}");

    chromaprintContext   = nullptr;
    decoderStarted       = false;
    decoderFinished      = false;
    enabled              = true;
    globalEnabled        = true;
    firstBuffer          = true;
    analyzeOK            = false;
    sendDiagnostics      = false;
}


// destructor
Analyzer::~Analyzer()
{
    if (chromaprintContext != nullptr) {
        chromaprint_free(chromaprintContext);
    }
}


// server event handler
void Analyzer::run()
{
    chromaprintContext = chromaprint_new(CHROMAPRINT_ALGORITHM_DEFAULT);

    emit loadGlobalConfiguration(id);
}


// server event handler
void Analyzer::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// server event handler
void Analyzer::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    // can not change after decoder started
    if (uniqueId != id) {
        return;
    }

    if (configuration.object().contains("enabled")) {
        globalEnabled = configuration.object().value("enabled").toBool();

    }

    if (!decoderStarted) {
        enabled = globalEnabled;

        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
    }
}


// configuration
void Analyzer::sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void Analyzer::globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void Analyzer::sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(error);
}


// server event handler
void Analyzer::getUiQml(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    QFile settingsFile("://ChromaprintAnalyzerSettings.qml");
    settingsFile.open(QFile::ReadOnly);
    QString settings = settingsFile.readAll();
    settingsFile.close();

    if (!globalEnabled) {
        settings.replace("checked: true", "checked: false");
    }

    emit uiQml(id, settings);
}


// server event handler
void Analyzer::uiResults(QUuid uniqueId, QJsonDocument results)
{
    if (uniqueId != id) {
        return;
    }

    globalEnabled = results.object().value("enabled").toBool();
    emit saveGlobalConfiguration(id, results);

    // can not change after decoding started
    if (!decoderStarted) {
        enabled = globalEnabled;

        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
    }
}


// server event handler
void Analyzer::messageFromPlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(sourceUniqueId);
    Q_UNUSED(messageId);
    Q_UNUSED(value);
}


// server event handler
void Analyzer::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = true;
    sendDiagnosticsData();
}


// server event handler
void Analyzer::stopDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = false;
}


// server event handler
void Analyzer::bufferAvailable(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    decoderStarted = true;

    while (bufferQueue->count() > 0) {
        // had to wait with chromaprint start for the first buffer because format needed
        if (enabled && !cast && firstBuffer) {
            // chromaprint supports only one format + in some rare cases the first buffer is empty
            if ((bufferQueue->at(0)->format().channelCount() > 0) && (bufferQueue->at(0)->format().sampleSize() == 16) && (bufferQueue->at(0)->format().sampleType() == QAudioFormat::SignedInt)) {
                chromaprint_start(chromaprintContext, bufferQueue->at(0)->format().sampleRate(), bufferQueue->at(0)->format().channelCount());
                firstBuffer = false;
                analyzeOK   = true;
            }
        }

        // process buffer - chromaprint only cares for first two miutes
        if (analyzeOK && (bufferQueue->at(0)->startTime() < (120 * 1000 * 1000))) {
            if (!chromaprint_feed(chromaprintContext, static_cast<const int16_t *>(bufferQueue->at(0)->constData()), bufferQueue->at(0)->sampleCount())) {
                emit infoMessage(id, "Chromaprint reported error while processing audio data");
                analyzeOK = false;
            }
        }

        // duration
        duration = bufferQueue->at(0)->startTime() + bufferQueue->at(0)->duration();

        // need this pointer now
        QAudioBuffer *bufferPointer = bufferQueue->at(0);

        // remove from queue
        bufferQueueMutex->lock();
        bufferQueue->remove(0);
        bufferQueueMutex->unlock();

        // this makes the track pass it on to the next plugin
        emit bufferDone(id, bufferPointer);
    }

    // this might not happen here, see decoderDone
    if (enabled && !cast && decoderFinished && !firstBuffer && (bufferQueue->count() == 0)) {
        sendChromaprint();
    }
}


// server event handler
void Analyzer::decoderDone(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    decoderFinished = true;

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    // this signal can come before or after the last buffer is processed
    if (enabled && !cast && !firstBuffer && (bufferQueue->count() == 0)) {
        sendChromaprint();
    }
}


// private method
void Analyzer::sendChromaprint()
{
    if (!analyzeOK) {
        return;
    }

    chromaprint_finish(chromaprintContext);

    char *fingerprint;
    chromaprint_get_fingerprint(chromaprintContext, &fingerprint);

    chromaprint = QString::fromUtf8(fingerprint);

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    emit messageToPlugin(id, QUuid("{EDEA3392-28E8-4C57-A8AE-EAB5864D5E4B}"), 1, duration);
    emit messageToPlugin(id, QUuid("{EDEA3392-28E8-4C57-A8AE-EAB5864D5E4B}"), 2, chromaprint);

    chromaprint_dealloc(fingerprint);

    analyzeOK = false;
}


// private method
void Analyzer::sendDiagnosticsData()
{
    DiagnosticData diagnosticData;

    diagnosticData.append((DiagnosticItem){ "Enabled", enabled ? "Yes" : "No" });
    if (cast) {
        diagnosticData.append((DiagnosticItem){ "Status", "Not needed to be analyzed" });
    }
    if (!chromaprint.isEmpty()) {
        diagnosticData.append((DiagnosticItem){ "Length of fingerprint", QString("%1").arg(chromaprint.length()) });
    }
    if (decoderFinished) {
        diagnosticData.append((DiagnosticItem){ "Duration", QString("%1").arg(duration / (1000 * 1000)) });
    }

    emit diagnostics(id, diagnosticData);
}
