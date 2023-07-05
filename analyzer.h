/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef ANALYZER_H
#define ANALYZER_H


#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QObject>
#include <QtGlobal>
#include <QTimeZone>
#include <QVariant>
#include <QVariantHash>
#include <waveriir/coefficientlist.h>
#include <waveriir/iirfilter.h>
#include <waveriir/iirfilterchain.h>
#include <waveriir/replaygaincoefficients.h>

#include "globals.h"
#include "replaygaincalculator.h"


#ifdef QT_DEBUG
    #include <QDebug>
#endif


class Analyzer : public QObject
{
    Q_OBJECT

    public:

        explicit Analyzer(QAudioFormat format, QObject *parent = nullptr);
        ~Analyzer();

        void setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex);


    private:

        static const int REPLAY_GAIN_UPDATE_INTERVAL_MICROSECONDS = 4 * 1000 * 1000;

        BufferQueue *bufferQueue;
        QMutex      *bufferQueueMutex;

        QAudioFormat format;

        bool                    decoderFinished;
        qint64                  resultLastCalculated;
        IIRFilter::SampleTypes  sampleType;
        IIRFilterChain         *replayGainFilter;
        ReplayGainCalculator   *replayGainCalculator;


    public slots:

        void run();

        void bufferAvailable();
        void decoderDone();
        void resetReplayGain();

        void silencesRequested(bool addFinalSilence);


    signals:

        void replayGain(double replayGain);
        void silences(ReplayGainCalculator::Silences silences);
};

#endif // ANALYZER_H
