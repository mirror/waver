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
    if (pluginTypesMask & PluginBase::PLUGIN_TYPE_OUTPUT) {
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
    fadeDirection       = FADE_DIRECTION_NONE;
    sendFadeComplete    = true;
    volume              = 1.0;
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
    return 1;
}


// overrided virtual function
QString SoundOutput::waverVersionAPICompatibility()
{
    return "0.0.1";
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
void SoundOutput::getUiQml(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    QFile settingsFile("://SO_Settings.qml");
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
        sendFadeComplete = false;
        fadeIn(id, 2);
        audioIODevice = audioOutput->start();
        if (feeder != NULL) {
            feeder->setOutputDevice(audioIODevice);
        }
        if (!timerWaits) {
            fillBytesToPlay();
        }
    }
}


// signal handler
void SoundOutput::fadeIn(QUuid uniqueId, int seconds)
{
    if (uniqueId != id) {
        return;
    }

    fadeDirection  = FADE_DIRECTION_IN;
    fadePercent    = 0;
    fadeSeconds    = seconds;
    fadeFrameCount = 0;
}


// signal handler
void SoundOutput::fadeOut(QUuid uniqueId, int seconds)
{
    if (uniqueId != id) {
        return;
    }

    fadeDirection  = FADE_DIRECTION_OUT;
    fadePercent    = 100;
    fadeSeconds    = seconds;
    fadeFrameCount = 0;
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
        // fade in / out
        if (fadeDirection != FADE_DIRECTION_NONE) {
            applyFade();
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
void SoundOutput::applyFade()
{
    // usual paranoia
    if (bufferQueue->count() < 1) {
        return;
    }

    // so that it doesn't have to be done many times
    QAudioBuffer *buffer = bufferQueue->at(0);

    // some variables that are needed
    double framesPerPercent = buffer->format().framesForDuration(fadeSeconds * 1000000) / 100;
    double framesPerSample  = 1.0 / buffer->format().channelCount();

    // this is only to speed up things inside the loop
    int dataType = 0;
    if (buffer->format().sampleType() == QAudioFormat::SignedInt) {
        if (buffer->format().sampleSize() == 8) {
            dataType = 1;
        }
        else if (buffer->format().sampleSize() == 16) {
            dataType = 2;
        }
        else if (buffer->format().sampleSize() == 32) {
            dataType = 3;
        }
    }
    else if (buffer->format().sampleType() == QAudioFormat::UnSignedInt) {
        if (buffer->format().sampleSize() == 8) {
            dataType = 4;
        }
        else if (buffer->format().sampleSize() == 16) {
            dataType = 5;
        }
        else if (buffer->format().sampleSize() == 32) {
            dataType = 6;
        }
    }

    // only one of these will be used, depending on the data type
    qint8   *int8;
    qint16  *int16;
    qint32  *int32;
    quint8  *uint8;
    quint16 *uint16;
    quint32 *uint32;

    // do the math sample by sample (simple linear fade)
    char  *data      = (char *)buffer->data();
    int    byteCount = 0;
    while (byteCount < buffer->byteCount()) {
        // not all formats supported, but most common ones are
        if (dataType != 0) {
            // calculation
            switch (dataType) {
                case 1:
                    int8  = (qint8 *)data;
                    *int8 = (fadePercent * *int8) / 100;
                    data      += 1;
                    byteCount += 1;
                    break;
                case 2:
                    int16  = (qint16 *)data;
                    *int16 = (fadePercent * *int16) / 100;
                    data      += 2;
                    byteCount += 2;
                    break;
                case 3:
                    int32  = (qint32 *)data;
                    *int32 = (fadePercent * *int32) / 100;
                    data      += 4;
                    byteCount += 4;
                    break;
                case 4:
                    uint8  = (quint8 *)data;
                    *uint8 = (fadePercent * *uint8) / 100;
                    data      += 1;
                    byteCount += 1;
                    break;
                case 5:
                    uint16  = (quint16 *)data;
                    *uint16 = (fadePercent * *uint16) / 100;
                    data      += 2;
                    byteCount += 2;
                    break;
                case 6:
                    uint32  = (quint32 *)data;
                    *uint32 = (fadePercent * *uint32) / 100;
                    data      += 4;
                    byteCount += 4;
            }
        }
        else {
            byteCount += buffer->format().sampleSize() / 8;
        }

        fadeFrameCount += framesPerSample;

        // change percentage if it's time to do that
        if (fadeFrameCount >= framesPerPercent) {
            // reset counter for next percent
            fadeFrameCount = 0;

            // fade in
            if ((fadeDirection == FADE_DIRECTION_IN) && (fadePercent < 100)) {
                fadePercent++;

                // after fade in, it is excepted that the track will play along, so stop fading when 100% is reached
                if (fadePercent == 100) {
                    fadeDirection = FADE_DIRECTION_NONE;

                    if (sendFadeComplete) {
                        emit fadeInComplete(id);
                    }
                    sendFadeComplete = true;
                }
            }

            // fade out
            if ((fadeDirection == FADE_DIRECTION_OUT) && (fadePercent > 0)) {
                fadePercent--;

                // after fade out, it is expected that track will stop, so keep it faded
                if (fadePercent == 0) {
                    if (sendFadeComplete) {
                        emit fadeOutComplete(id);
                    }
                    sendFadeComplete = true;
                }
            }
        }
    }
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
