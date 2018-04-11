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


#include "soundoutput.h"


// plugin factory
void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal)
{
    if (pluginTypesMask & PLUGIN_TYPE_OUTPUT) {
        retVal->append((QObject *) new SoundOutput());
    }
}


// constructor
SoundOutput::SoundOutput()
{
    id                  = QUuid("{B1784AD6-D44E-43A8-A568-B01793C44623}");
    wasError            = false;
    timerWaits          = false;
    notificationCounter = 0;
    audioOutput         = NULL;
    feeder              = NULL;
    volume              = 1.0;
    sendDiagnostics     = false;
}


// destruction
SoundOutput::~SoundOutput()
{
    if (feeder != NULL) {
        feeder->setOutputDevice(NULL);
        feederThread.requestInterruption();
        feederThread.quit();
        feederThread.wait();
    }

    if (audioOutput != NULL) {
        audioOutput->stop();
        delete audioOutput;
    }

    clearBuffers();
}


// overrided virtual function
int SoundOutput::pluginType()
{
    return PLUGIN_TYPE_OUTPUT;
}


// overrided virtual function
QString SoundOutput::pluginName()
{
    return "Sound Output";
}


// overrided virtual function
int SoundOutput::SoundOutput::pluginVersion()
{
    return 3;
}


// overrided virtual function
QString SoundOutput::waverVersionAPICompatibility()
{
    return "0.0.5";
}


// overrided virtual function
void SoundOutput::setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex)
{
    this->bufferQueue      = bufferQueue;
    this->bufferQueueMutex = bufferQueueMutex;
}


// overrided virtual function
bool SoundOutput::isMainOutput()
{
    return true;
}


// overrided virtual function
QUuid SoundOutput::persistentUniqueId()
{
    return id;
}


// overrided virtual function
bool SoundOutput::hasUI()
{
    // volume control disabled
    //
    //return true;

    return false;
}


// overrided virtual function
QUrl SoundOutput::menuImageURL()
{
    return QUrl();
}


// thread entry point
void SoundOutput::run()
{
    // volume control disabled
    //
    // emit loadConfiguration(id);
}


// signal handler
void SoundOutput::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    if (configuration.isEmpty()) {
        // default value
        volume = 1.0;
    }
    else {
        // get value from configuration
        volume = configuration.object().value("volume").toDouble();
    }

    if (audioOutput != NULL) {
        // TODO volume control disabled
        //audioOutput->setVolume((qreal)volume);
    }
}


// signal handler
void SoundOutput::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// configuration
void SoundOutput::sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void SoundOutput::globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void SoundOutput::sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(error);
}


// signal handler
void SoundOutput::getUiQml(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    QFile settingsFile("://SOSettings.qml");
    settingsFile.open(QFile::ReadOnly);
    QString settings = settingsFile.readAll();
    settingsFile.close();

    settings.replace("replace_volume_value", QString("%1").arg(volume));

    emit uiQml(id, settings);
}


// signal handler
void SoundOutput::uiResults(QUuid uniqueId, QJsonDocument results)
{
    if (uniqueId != id) {
        return;
    }

    loadedConfiguration(id, results);
    emit saveConfiguration(id, results);
}


// client wants to receive updates of this plugin's diagnostic information
void SoundOutput::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = true;
    sendDiagnosticsData();
}


// client doesnt want to receive updates of this plugin's diagnostic information anymore
void SoundOutput::stopDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = false;
}


// signal handler
void SoundOutput::bufferAvailable(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    // don't try to do anything if an error occured
    if (wasError) {
        clearBuffers();
        return;
    }

    // had to wait with creating the output until now, because it needs to know the audio format
    if (audioOutput == NULL) {
        // let's make sure there's some data buffered
        if (bufferQueue->count() < CACHE_BUFFER_COUNT) {
            return;
        }

        // for diagnostics
        diagnosticsAudioFormat = bufferQueue->at(0)->format();

        // create output and iodevice
        audioOutput = new QAudioOutput(bufferQueue->at(0)->format());
        audioOutput->setNotifyInterval(NOTIFICATION_INTERVAL_MILLISECONDS);
        // TODO volume control disabled
        //audioOutput->setVolume((qreal)volume);
        connect(audioOutput, SIGNAL(notify()),                    this, SLOT(audioOutputNotification()));
        connect(audioOutput, SIGNAL(stateChanged(QAudio::State)), this, SLOT(audioOutputStateChanged(QAudio::State)));

        // start output
        audioIODevice = audioOutput->start();

        // create and start feeder
        feeder = new Feeder(&bytesToPlay, &bytesToPlayMutex, bufferQueue->at(0)->format(), audioOutput->periodSize());
        feeder->moveToThread(&feederThread);
        connect(&feederThread, SIGNAL(started()),  feeder, SLOT(run()));
        connect(&feederThread, SIGNAL(finished()), feeder, SLOT(deleteLater()));
        feederThread.start();
        feeder->setOutputDevice(audioIODevice);
    }

    // kick it off
    if (!timerWaits && (audioOutput->state() != QAudio::StoppedState)) {
        fillBytesToPlay();
    }
}


// signal handler
void SoundOutput::pause(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    if (feeder != NULL) {
        feeder->setOutputDevice(NULL);
    }

    if (audioOutput != NULL) {
        audioOutput->stop();
    }

    bytesToPlayMutex.lock();
    bytesToPlay.clear();
    bytesToPlayMutex.unlock();
}


// signal handler
void SoundOutput::resume(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    if (audioOutput != NULL) {
        clearBuffers();
        audioIODevice = audioOutput->start();
        if (feeder != NULL) {
            feeder->setOutputDevice(audioIODevice);
        }
        if (!timerWaits) {
            fillBytesToPlay();
        }
    }
}


// audio output signal handler
void SoundOutput::audioOutputNotification()
{
    notificationCounter++;
    emit positionChanged(id, notificationCounter * NOTIFICATION_INTERVAL_MILLISECONDS);
}


// audio output signal handler
void SoundOutput::audioOutputStateChanged(QAudio::State state)
{
    if ((state == QAudio::StoppedState) && (audioOutput->error() != QAudio::NoError)) {
        QString errorString = "Unknown audio output error";
        switch (audioOutput->error()) {
            case QAudio::OpenError:
                errorString = "An error occurred opening the audio device";
                break;
            case QAudio::IOError:
                errorString = "An error occurred during read/write of audio device";
                break;
            case QAudio::UnderrunError:
                errorString = "Audio data is not being fed to the audio device at a fast enough rate";
                break;
            case QAudio::FatalError:
                errorString = "A non-recoverable error has occurred, the audio device is not usable at this time.";
                break;
            default:
                break;
        }

        wasError = true;
        emit error(id, errorString);
    }
}


// timer signal handler
void SoundOutput::timerTimeout()
{
    fillBytesToPlay();
}


// private method
void SoundOutput::fillBytesToPlay()
{
    // there might be no buffer available at this time
    if (bufferQueue->count() < 1) {
        timerWaits = false;
        emit bufferUnderrun(id);
        return;
    }

    // paused?
    if (audioOutput->state() == QAudio::StoppedState) {
        timerWaits = false;
        return;
    }

    int timerDelay = 0;

    // fill in to the temporary buffer
    while ((bufferQueue->count() > 0) && (bytesToPlay.count() < (audioOutput->periodSize() * 3))) {
        // diagonostics
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }

        // so that it doesn't have to be called many times
        QAudioBuffer *buffer = bufferQueue->at(0);

        // append to temporary buffer
        bytesToPlayMutex.lock();
        bytesToPlay.append((char *)buffer->constData(), buffer->byteCount());
        bytesToPlayMutex.unlock();

        timerDelay += buffer->format().durationForBytes(buffer->byteCount()) / 1000;

        // remove from queue
        bufferQueueMutex->lock();
        bufferQueue->remove(0);
        bufferQueueMutex->unlock();

        // this makes the decoder delete it
        emit bufferDone(id, buffer);
    }

    // timer to write next chunk
    timerWaits = true;
    QTimer::singleShot(timerDelay > 0 ? timerDelay / 4 * 3 : 50, this, SLOT(timerTimeout()));
}


// private method
void SoundOutput::clearBuffers()
{
    foreach (QAudioBuffer *buffer, *bufferQueue) {
        emit bufferDone(id, buffer);
    }

    bufferQueueMutex->lock();
    bufferQueue->clear();
    bufferQueueMutex->unlock();
}


// private method
void SoundOutput::sendDiagnosticsData()
{
    DiagnosticData diagnosticData;

    bytesToPlayMutex.lock();
    double bufferSize = (double) bytesToPlay.count();
    bytesToPlayMutex.unlock();

    if (bufferSize > 1024) {
        diagnosticData.append({ "Play buffer size", QString("%1 KB (%2 ms)").arg(bufferSize / 1024, 5, 'f', 2, '0').arg((double)diagnosticsAudioFormat.durationForBytes(bufferSize) / 1000, 7, 'f', 3, '0') });
    }
    else {
        diagnosticData.append({ "Play buffer size", QString("%1 B (%2 ms)").arg(bufferSize, 4, 'f', 0, '0').arg((double)diagnosticsAudioFormat.durationForBytes(bufferSize) / 1000, 7, 'f', 3, '0') });
    }

    emit diagnostics(id, diagnosticData);
}
