/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/

#include "soundoutput.h"


SoundOutput::SoundOutput(QAudioFormat format, PeakCallback::PeakCallbackInfo peakCallbackInfo, QObject *parent) : QObject(parent)
{
    this->format = format;

    wasError              = false;
    wasUnderrun           = false;
    initialCachingDone    = false;
    timerWaits            = false;
    notificationCounter   = 0;
    beginningMicroseconds = -1;
    audioOutput           = nullptr;
    feeder                = nullptr;
    feedTimer             = nullptr;
    volume                = 1.0;

    bytesToPlay      = nullptr;
    bytesToPlayMutex = nullptr;

    chunkQueue      = nullptr;
    chunkQueueMutex = nullptr;

    this->peakCallbackInfo = peakCallbackInfo;
}


SoundOutput::~SoundOutput()
{
    if (feedTimer != nullptr) {
        feedTimer->stop();
        delete feedTimer;
    }

    if (feeder != nullptr) {
        feeder->setOutputDevice(nullptr);
        feederThread.requestInterruption();
        feederThread.quit();
        feederThread.wait();
        delete feeder;
    }

    if (audioOutput != nullptr) {
        audioOutput->deleteLater();
    }

    clearBuffers();

    if (bytesToPlay != nullptr) {
        delete bytesToPlay;
    }
    if (bytesToPlayMutex != nullptr) {
        delete bytesToPlayMutex;
    }
}


void SoundOutput::audioOutputNotification()
{
    notificationCounter++;
    if (!wasUnderrun) {
        emit positionChanged((notificationCounter * audioOutput->notifyInterval()) + (beginningMicroseconds / 1000));
    }
}


void SoundOutput::audioOutputStateChanged(QAudio::State state)
{
    if ((state == QAudio::StoppedState) && (audioOutput->error() != QAudio::NoError)) {
        QString errorString;
        switch (audioOutput->error()) {
            case QAudio::OpenError:
                errorString = tr("An error occurred opening the audio device");
                break;
            case QAudio::IOError:
                errorString = tr("An error occurred during read/write of audio device");
                break;
            case QAudio::UnderrunError:
                errorString = tr("Audio data is not being fed to the audio device at a fast enough rate");
                break;
            case QAudio::FatalError:
                errorString = tr("A non-recoverable error has occurred, the audio device is not usable at this time.");
                break;
            default:
                errorString = tr("Unknown audio output error");
        }

        wasError = true;
        emit error(errorString);
    }
}


void SoundOutput::chunkAvailable()
{
    if (wasError) {
        clearBuffers();
        return;
    }

    if ((chunkQueue == nullptr) || (chunkQueueMutex == nullptr)) {
        return;
    }

    if (!initialCachingDone) {
        if (chunkQueue->count() < INITIAL_CACHE_BUFFER_COUNT) {
            emit needChunk();
            return;
        }

        audioIODevice = audioOutput->start();

        feederThread.start();
        feeder->setOutputDevice(audioIODevice);

        initialCachingDone = true;
    }

    if ((chunkQueue->count() > 0) && (beginningMicroseconds < 0)) {
        notificationCounter   = 0;
        beginningMicroseconds = chunkQueue->at(0).startMicroseconds;
    }

    if (!timerWaits && (audioOutput->state() != QAudio::StoppedState)) {
        fillBytesToPlay();
    }
}


void SoundOutput::clearBuffers()
{
    if ((chunkQueue == nullptr) || (chunkQueueMutex == nullptr)) {
        return;
    }

    chunkQueueMutex->lock();
    chunkQueue->clear();
    chunkQueueMutex->unlock();
}


void SoundOutput::fillBytesToPlay()
{
    if ((chunkQueue == nullptr) || (chunkQueueMutex == nullptr)) {
        return;
    }

    if (chunkQueue->count() < 1) {
        timerWaits = false;
        wasUnderrun = true;
        emit bufferUnderrun();
        return;
    }
    wasUnderrun = false;

    if (audioOutput->state() == QAudio::StoppedState) {
        timerWaits = false;
        return;
    }

    if ((bytesToPlay == nullptr) || (bytesToPlayMutex == nullptr)) {
        timerWaits = false;
        return;
    }

    int timerDelay = 0;

    while ((chunkQueue->count() > 0) && (bytesToPlay->count() < (audioOutput->periodSize() * 3))) {
        QByteArray *chunk = chunkQueue->at(0).chunkPointer;

        bytesToPlayMutex->lock();
        bytesToPlay->append(chunk->data(), chunk->size());
        bytesToPlayMutex->unlock();

        timerDelay += format.durationForBytes(chunk->size()) / 1000;

        chunkQueueMutex->lock();
        chunkQueue->remove(0);
        chunkQueueMutex->unlock();
        delete chunk;

        emit needChunk();
    }

    timerWaits = true;
    feedTimer->singleShot(timerDelay > 0 ? timerDelay / 4 * 3 : 50, this, SLOT(timerTimeout()));
}


void SoundOutput::pause()
{
    if (feeder != nullptr) {
        feeder->setOutputDevice(nullptr);
    }

    if (audioOutput != nullptr) {
        audioOutput->stop();
    }

    if ((bytesToPlay != nullptr) && (bytesToPlayMutex != nullptr)) {
        bytesToPlayMutex->lock();
        bytesToPlay->clear();
        bytesToPlayMutex->unlock();
    }
}


qint64 SoundOutput::remainingMilliseconds()
{
    qint64 microseconds = 0;

    if ((bytesToPlay == nullptr) || (bytesToPlayMutex == nullptr)) {
        return microseconds;
    }
    if ((chunkQueue == nullptr) || (chunkQueueMutex == nullptr)) {
        return microseconds;
    }

    bytesToPlayMutex->lock();
    microseconds += format.durationForBytes(bytesToPlay->size());
    bytesToPlayMutex->unlock();

    chunkQueueMutex->lock();
    for (int i = 0; i < chunkQueue->size(); i++) {
        microseconds += format.durationForBytes(chunkQueue->at(i).chunkPointer->size());
    }
    chunkQueueMutex->unlock();

    return microseconds / 1000;
}


void SoundOutput::resume()
{
    clearBuffers();
    notificationCounter   = 0;
    beginningMicroseconds = -1;
    initialCachingDone    = false;

    if (audioOutput != nullptr) {
        emit needChunk();
    }
}


void SoundOutput::run()
{
    audioOutput = new QAudioOutput(format);
    audioOutput->setNotifyInterval(NOTIFICATION_INTERVAL_MILLISECONDS);

    QSettings settings;
    int wideStereoDelayMillisec = settings.value("options/wide_stereo_delay_millisec", DEFAULT_WIDE_STEREO_DELAY_MILLISEC).toInt();

    bytesToPlay      = new QByteArray();
    bytesToPlayMutex = new QMutex();
    feedTimer        = new QTimer();

    connect(audioOutput, SIGNAL(notify()),                    this, SLOT(audioOutputNotification()));
    connect(audioOutput, SIGNAL(stateChanged(QAudio::State)), this, SLOT(audioOutputStateChanged(QAudio::State)));

    feeder = new OutputFeeder(bytesToPlay, bytesToPlayMutex, format, audioOutput, peakCallbackInfo);
    feeder->moveToThread(&feederThread);
    feeder->setWideStereoDelayMillisec(wideStereoDelayMillisec);

    connect(&feederThread, SIGNAL(started()), feeder, SLOT(run()));

    emit needChunk();
}


void SoundOutput::setBufferQueue(TimedChunkQueue *chunkQueue, QMutex *chunkQueueMutex)
{
    this->chunkQueue      = chunkQueue;
    this->chunkQueueMutex = chunkQueueMutex;
}


void SoundOutput::timerTimeout()
{
    fillBytesToPlay();
}


void SoundOutput::wideStereoDelayChanged(int wideStereoDelayMillisec)
{
    if (feeder == nullptr) {
        return;
    }
    feeder->setWideStereoDelayMillisec(wideStereoDelayMillisec);
}
