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
    initialCachingDone    = false;
    feedTimerWaits        = false;
    beginningMilliseconds = -1;
    audioSink             = nullptr;
    feeder                = nullptr;
    feedTimer             = nullptr;
    positionTimer         = nullptr;
    lastMilliseconds      = -1;
    volume                = 1.0;

    bytesToPlay      = nullptr;
    bytesToPlayMutex = nullptr;

    this->peakCallbackInfo = peakCallbackInfo;
}


SoundOutput::~SoundOutput()
{
    if (feedTimer != nullptr) {
        feedTimer->stop();
        delete feedTimer;
    }

    if (positionTimer != nullptr) {
        positionTimer->stop();
        delete positionTimer;
    }

    if (feeder != nullptr) {
        feeder->setOutputDevice(nullptr);
        feederThread.requestInterruption();
        feederThread.quit();
        feederThread.wait();
        delete feeder;
    }

    if (audioSink != nullptr) {
        audioSink->deleteLater();
    }

    clearBuffers();

    if (bytesToPlay != nullptr) {
        delete bytesToPlay;
    }
    if (bytesToPlayMutex != nullptr) {
        delete bytesToPlayMutex;
    }

}


void SoundOutput::audioOutputStateChanged(QAudio::State state)
{
    qWarning() << state;

    if ((state == QAudio::StoppedState) && (audioSink->error() != QAudio::NoError)) {
        QString errorString;
        switch (audioSink->error()) {
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

    if (!initialCachingDone) {
        if (chunkQueue->count() < INITIAL_CACHE_BUFFER_COUNT) {
            emit needChunk();
            return;
        }

        audioIODevice = audioSink->start();

        feederThread.start();
        feeder->setOutputDevice(audioIODevice);

        initialCachingDone = true;
    }

    if ((chunkQueue->count() > 0) && (beginningMilliseconds < 0)) {
        beginningMilliseconds = chunkQueue->at(0).startMicroseconds / 1000;
    }

    if (!feedTimerWaits && (audioSink->state() != QAudio::StoppedState)) {
        fillBytesToPlay();
    }
}


void SoundOutput::clearBuffers()
{
    chunkQueueMutex->lock();
    chunkQueue->clear();
    chunkQueueMutex->unlock();
}


void SoundOutput::fillBytesToPlay()
{
    if (chunkQueue->count() < 1) {
        feedTimerWaits = false;
        emit bufferUnderrun();
        return;
    }

    if (audioSink->state() == QAudio::StoppedState) {
        feedTimerWaits = false;
        return;
    }

    if ((bytesToPlay == nullptr) || (bytesToPlayMutex == nullptr)) {
        feedTimerWaits = false;
        return;
    }

    int timerDelay = 0;

    while ((chunkQueue->count() > 0) && (bytesToPlay->count() < (audioSink->bufferSize() * 3))) {
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

    feedTimerWaits = true;
    feedTimer->singleShot(timerDelay > 0 ? timerDelay / 4 * 3 : 50, this, SLOT(feedTimerTimeout()));
}


void SoundOutput::pause()
{
    if (feeder != nullptr) {
        feeder->setOutputDevice(nullptr);
    }

    if (audioSink != nullptr) {
        audioSink->stop();
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
    beginningMilliseconds = -1;
    initialCachingDone    = false;

    if (audioSink != nullptr) {
        audioIODevice = audioSink->start();
        if (feeder != nullptr) {
            feeder->setOutputDevice(audioIODevice);
        }
        if (!feedTimerWaits) {
            fillBytesToPlay();
        }
    }
}


void SoundOutput::run()
{
    audioSink = new QAudioSink(format);
    audioSink->setBufferSize(audioSink->format().bytesForDuration(FEED_LENGTH_MICROSECONDS));

    bytesToPlay      = new QByteArray();
    bytesToPlayMutex = new QMutex();

    feedTimer     = new QTimer();
    positionTimer = new QTimer();

    connect(audioSink,     SIGNAL(stateChanged(QAudio::State)), this, SLOT(audioOutputStateChanged(QAudio::State)));
    connect(positionTimer, SIGNAL(timeout()),                   this, SLOT(positionTimerTimeout()));

    feeder = new OutputFeeder(bytesToPlay, bytesToPlayMutex, format, audioSink, peakCallbackInfo);
    feeder->moveToThread(&feederThread);

    connect(&feederThread, SIGNAL(started()), feeder, SLOT(run()));

    emit needChunk();

    positionTimer->start(250);
}


void SoundOutput::setBufferQueue(TimedChunkQueue *chunkQueue, QMutex *chunkQueueMutex)
{
    this->chunkQueue      = chunkQueue;
    this->chunkQueueMutex = chunkQueueMutex;
}


void SoundOutput::feedTimerTimeout()
{
    fillBytesToPlay();
}


void SoundOutput::positionTimerTimeout()
{
    if (audioSink == nullptr) {
        return;
    }

    qint64 milliseconds = audioSink->processedUSecs() / 1000 + beginningMilliseconds;
    if (milliseconds != lastMilliseconds) {
        lastMilliseconds = milliseconds;
        emit positionChanged(milliseconds);
    }
}
