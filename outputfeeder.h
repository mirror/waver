/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef OUTPUTFEEDER_H
#define OUTPUTFEEDER_H

#include <QAudioFormat>
#include <QAudioOutput>
#include <QByteArray>
#include <QIODevice>
#include <QMutex>
#include <QObject>
#include <QtMath>
#include <QThread>
#include <QTimer>

#include "peakcallback.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


class OutputFeeder : public QObject
{
    Q_OBJECT

    public:

        explicit OutputFeeder(QByteArray *outputBuffer, QMutex *outputBufferMutex, QAudioFormat audioFormat, QAudioOutput *audioOutput, PeakCallback::PeakCallbackInfo peakCallbackInfo, QObject *parent = nullptr);

        void setOutputDevice(QIODevice *outputDevice);
        void setWideStereoDelayMillisec(int wideStereoDelayMillisec);


    private:

        static const qint64 MICROSECONDS_PER_SECOND        = 1000 * 1000;
        static const int    WIDE_STEREO_DELAY_MILLISEC_MAX = 50;

        QAudioOutput *audioOutput;
        QByteArray   *outputBuffer;
        QMutex       *outputBufferMutex;
        QIODevice    *outputDevice;

        QAudioFormat                   audioFormat;
        PeakCallback::PeakCallbackInfo peakCallbackInfo;

        QMutex outputDeviceMutex;
        QMutex peakDelaySumMutex;

        int channelCount;
        int channelIndex;
        int frameCount;
        int dataType;
        int dataBytes;

        double int16Min;
        double int16Max;
        double int16Range;
        double sampleMin;
        double sampleRange;

        double  lPeak;
        double  rPeak;
        qint64  peakDelaySum;

        int    wideStereoDelayMillisec;
        int    newWideStereoDelayMillisec;
        QMutex wideStereoDelayMutex;


    public slots:

        void run();


};

#endif // OUTPUTFEEDER_H
