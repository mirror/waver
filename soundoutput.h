/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/

#ifndef SOUNDOUTPUT_H
#define SOUNDOUTPUT_H

#include <QAudioFormat>
#include <QAudioOutput>
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QIODevice>
#include <QMutex>
#include <QObject>
#include <QThread>
#include <QTimer>

#include "decodergeneric.h"
#include "equalizer.h"
#include "globals.h"
#include "outputfeeder.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif

class SoundOutput : public QObject
{
    Q_OBJECT

    public:
        explicit SoundOutput(QAudioFormat format, PeakCallback::PeakCallbackInfo peakCallbackInfo, QObject *parent = nullptr);
        ~SoundOutput();

        void setBufferQueue(TimedChunkQueue *chunkQueue, QMutex *chunkQueueMutex);
        void wideStereoDelayChanged(int wideStereoDelayMillisec);

        qint64 remainingMilliseconds();


    private:

        static const int    INITIAL_CACHE_BUFFER_COUNT         = 3;
        static const qint64 NOTIFICATION_INTERVAL_MILLISECONDS = 500;

        QAudioFormat                    format;
        PeakCallback::PeakCallbackInfo  peakCallbackInfo;

        TimedChunkQueue *chunkQueue;
        QMutex          *chunkQueueMutex;

        QAudioOutput *audioOutput;
        QIODevice    *audioIODevice;

        QByteArray *bytesToPlay;
        QMutex     *bytesToPlayMutex;

        QThread       feederThread;
        OutputFeeder *feeder;
        QTimer       *feedTimer;

        bool   wasError;
        bool   wasUnderrun;
        bool   initialCachingDone;
        bool   timerWaits;
        qint64 notificationCounter;
        qint64 beginningMicroseconds;

        double volume;

        void fillBytesToPlay();
        void clearBuffers();


    public slots:

        void run();

        void chunkAvailable();

        void pause();
        void resume();


    private slots:

        void timerTimeout();

        void audioOutputNotification();
        void audioOutputStateChanged(QAudio::State state);


    signals:

        void positionChanged(qint64 posMilliseconds);
        void needChunk();
        void bufferUnderrun();
        void error(QString errorMessage);
};

#endif // SOUNDOUTPUT_H
