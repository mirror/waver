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


#include "genericdecoder.h"


// plugin factory
void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal)
{
    if (pluginTypesMask & PLUGIN_TYPE_DECODER) {
        #ifndef Q_OS_ANDROID
        retVal->append((QObject *) new GenericDecoder());
        #endif
    }
}


// global method
int GenericDecoder::pluginType()
{
    return PLUGIN_TYPE_DECODER;
}


// global method
QString GenericDecoder::pluginName()
{
    return "Generic Decoder";
}


// global method
int GenericDecoder::pluginVersion()
{
    return 3;
}


// global function
QString GenericDecoder::waverVersionAPICompatibility()
{
    return "0.0.5";
}


// global method
QUuid GenericDecoder::persistentUniqueId()
{
    return id;
}


// global method
bool GenericDecoder::hasUI()
{
    return false;
}


// overriden virtual function
int GenericDecoder::priority()
{
    return 2;
}


// global method
void GenericDecoder::setUrl(QUrl url)
{
    // url can be set only once
    if (this->url.isEmpty()) {
        this->url = url;
    }
}


// global method
void GenericDecoder::setUserAgent(QString userAgent)
{
    this->userAgent = userAgent;
}


// constructor
GenericDecoder::GenericDecoder()
{
    id = QUuid("{2958F1AD-9042-48DC-8652-CFC96378E063}");

    audioDecoder        = NULL;
    file                = NULL;
    networkDownloader   = NULL;
    networkDeviceSet    = false;
    memoryUsage         = 0;
    decodedMicroSeconds = 0;
    sendDiagnostics     = false;
}


// destructor
GenericDecoder::~GenericDecoder()
{
    // housekeeping

    if (audioDecoder != NULL) {
        audioDecoder->stop();
        audioDecoder->deleteLater();
    }

    networkThread.quit();
    networkThread.wait();

    if (file != NULL) {
        file->close();
        file->deleteLater();
    }

    while (audioBuffers.count() > 0) {
        delete audioBuffers.at(0);
        audioBuffers.remove(0);
    }
}


// thread entry point
void GenericDecoder::run()
{
    // checky-checky
    if (url.isEmpty()) {
        emit error(id, "Url is empty");
        return;
    }
}


// start decoding
void GenericDecoder::start(QUuid uniqueId)
{
    // parameter check
    if (uniqueId != id) {
        return;
    }

    // checky-checky
    if (url.isEmpty()) {
        emit error(id, "Url is empty");
        return;
    }

    // desired audio format
    QAudioFormat audioFormat;
    audioFormat.setByteOrder(QSysInfo::ByteOrder == QSysInfo::BigEndian ? QAudioFormat::BigEndian : QAudioFormat::LittleEndian);
    audioFormat.setChannelCount(2);
    audioFormat.setCodec("audio/pcm");
    audioFormat.setSampleRate(44100);
    audioFormat.setSampleSize(16);
    audioFormat.setSampleType(QAudioFormat::SignedInt);

    // audio decoder
    audioDecoder = new QAudioDecoder();
    audioDecoder->setAudioFormat(audioFormat);

    // connect signals
    connect(audioDecoder, SIGNAL(bufferReady()),               this, SLOT(decoderBufferReady()));
    connect(audioDecoder, SIGNAL(finished()),                  this, SLOT(decoderFinished()));
    connect(audioDecoder, SIGNAL(error(QAudioDecoder::Error)), this, SLOT(decoderError(QAudioDecoder::Error)));

    // input device
    if (url.isLocalFile()) {
        file = new QFile(url.toLocalFile());
        file->open(QFile::ReadOnly);
        audioDecoder->setSourceDevice(file);
        audioDecoder->start();
    }
    else {
        networkDownloader = new NetworkDownloader(url, &waitCondition, userAgent);
        networkDownloader->moveToThread(&networkThread);

        connect(&networkThread, SIGNAL(started()),  networkDownloader, SLOT(run()));
        connect(&networkThread, SIGNAL(finished()), networkDownloader, SLOT(deleteLater()));

        connect(networkDownloader, SIGNAL(ready()),        this, SLOT(networkReady()));
        connect(networkDownloader, SIGNAL(error(QString)), this, SLOT(networkError(QString)));

        networkThread.start();
    }
}


// this plugin has no configuration
void GenericDecoder::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// this plugin has no configuration
void GenericDecoder::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// configuration
void GenericDecoder::sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void GenericDecoder::globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void GenericDecoder::sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(error);
}


// this plugin has no configuration
void GenericDecoder::getUiQml(QUuid uniqueId)
{
    Q_UNUSED(uniqueId);
}


// this plugin has no configuration
void GenericDecoder::uiResults(QUuid uniqueId, QJsonDocument results)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(results);
}


// client wants to receive updates of this plugin's diagnostic information
void GenericDecoder::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = true;
    sendDiagnosticsData();
}


// client doesnt want to receive updates of this plugin's diagnostic information anymore
void GenericDecoder::stopDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = false;
}


// network download signal handler
void GenericDecoder::networkReady()
{
    // start decoding
    if (audioDecoder->state() == QAudioDecoder::StoppedState) {
        networkDownloader->open(QIODevice::ReadOnly);
        audioDecoder->setSourceDevice(networkDownloader);
        audioDecoder->start();
    }
}


// network download signal handler
void GenericDecoder::networkError(QString errorString)
{
    // let the world know
    emit error(id, errorString);

    // just in case
    if (audioDecoder != NULL) {
        audioDecoder->stop();
    }
}


// decoder signal handler
void GenericDecoder::decoderBufferReady()
{
    // just to be on the safe side (seems like it happens sometimes when there's a decoder error)
    if (!audioDecoder->bufferAvailable()) {
        return;
    }

    // get the data
    QAudioBuffer bufferReady = audioDecoder->read();

    // make a copy
    QAudioBuffer *audioBuffer = new QAudioBuffer(QByteArray((char *)bufferReady.constData(), bufferReady.byteCount()), bufferReady.format(), decodedMicroSeconds);

    // update counters
    memoryUsage         += bufferReady.byteCount();
    decodedMicroSeconds += bufferReady.format().durationForBytes(bufferReady.byteCount());

    // remember buffer so it can be deleted later
    audioBuffers.append(audioBuffer);

    // emit signal
    emit bufferAvailable(id, audioBuffer);

    // diagnostics
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    // delay to limit memory usage and also to relieve the CPU (the exponential curve allows fast decoding in the beginning, then slows it down sharply as memory gets closer to limit)
    double        factor = qPow(memoryUsage, 12) / qPow(MAX_MEMORY, 12);
    unsigned long delay  = qMin((unsigned long)2500 * qRound((factor * USEC_PER_SEC) / 2500), USEC_PER_SEC - 1);
    delay = 1000;
    if (delay > 0) {
        QThread::currentThread()->usleep(delay);
    }

    // must prevent input underrun, appearently reading 0 bytes is show-stopper for the decoder
    if (networkDownloader != NULL) {
        networkDownloader->setErrorOnUnderrun(false);

        waitMutex.lock();
        while ((networkDownloader->realBytesAvailable() < 4048) && !networkDownloader->isFinshed() && !QThread::currentThread()->isInterruptionRequested()) {
            waitCondition.wait(&waitMutex, 1000);
        }
        waitMutex.unlock();

        if (sendDiagnostics) {
            DiagnosticData diagnosticData;
            diagnosticData.append({ "Download buffer size", formatBytes((double) networkDownloader->realBytesAvailable()) });
            emit diagnostics(id, diagnosticData);
        }
    }
}


// decoder signal handler
void GenericDecoder::decoderFinished()
{
    // nothing much to do, just emit signal
    emit finished(id);
}


// decoder signal handler
void GenericDecoder::decoderError(QAudioDecoder::Error error)
{
    QString errorStr = audioDecoder->errorString();
    if (errorStr.length() == 0) {
        switch (error) {
            case QAudioDecoder::NoError:
                errorStr = "No error has occurred.";
                break;
            case QAudioDecoder::ResourceError:
                errorStr = "A media resource couldn't be resolved.";
                break;
            case QAudioDecoder::FormatError:
                errorStr = "The format of a media resource isn't supported.";
                break;
            case QAudioDecoder::AccessDeniedError:
                errorStr = "There are not the appropriate permissions to play a media resource.";
                break;
            case QAudioDecoder::ServiceMissingError:
                errorStr = "A valid service was not found, decoding cannot proceed.";
        }
    }

    // let the world know
    emit this->error(id, errorStr);

    // just in case
    if (audioDecoder != NULL) {
        audioDecoder->stop();
    }
}


// signal handler from owner
void GenericDecoder::bufferDone(QUuid uniqueId, QAudioBuffer *buffer)
{
    // parameter check
    if (uniqueId != id) {
        return;
    }

    // release buffer
    if (audioBuffers.contains(buffer)) {
        audioBuffers.remove(audioBuffers.indexOf(buffer));
        memoryUsage -= buffer->byteCount();
        delete buffer;
    }

    // diagnostics
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// private method
void GenericDecoder::sendDiagnosticsData()
{
    DiagnosticData diagnosticData;
    diagnosticData.append({ "PCM buffer size", formatBytes((double) memoryUsage) });
    if (audioBuffers.count() > 0) {
        QString type;
        switch (audioBuffers.last()->format().sampleType()) {
            case QAudioFormat::SignedInt:
                type = "signed";
                break;
            case QAudioFormat::UnSignedInt:
                type = "unsigned";
                break;
            case QAudioFormat::Float:
                type = "floating point";
                break;
            default:
                type = "unknown";
        }
        diagnosticData.append({ "PCM format", QString("%1 Hz, %2 bit, %3").arg(audioBuffers.last()->format().sampleRate()).arg(audioBuffers.last()->format().sampleSize()).arg(type) });
    }
    emit diagnostics(id, diagnosticData);
}


// helper
QString GenericDecoder::formatBytes(double bytes)
{
    if (bytes > (1024 * 1024)) {
        return QString("%1 MB").arg(bytes / (1024 * 1024), 0, 'f', 2);
    }
    else if (bytes > 1024) {
        return QString("%1 KB").arg(bytes / 1024, 0, 'f', 2);
    }

    return QString("%1 B").arg(bytes, 0, 'f', 0);
}
