/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef EQUALIZER_H
#define EQUALIZER_H

#include <QAudioFormat>
#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QtGlobal>
#include <QVector>
#include <waveriir/iirfilter.h>
#include <waveriir/iirfilterchain.h>
#include <waveriir/iirfiltercallback.h>

#include "globals.h"


class Equalizer : public QObject, IIRFilterCallback {
        Q_OBJECT

    public:

        Equalizer(QAudioFormat format);
        ~Equalizer();

        void setChunkQueue(TimedChunkQueue *chunkQueue, QMutex *chunkQueueMutex);
        void setGains(bool on, QVector<double> gains, double preAmp);

        void filterCallback(double *sample, int channelIndex) override;

        QVector<double> getBandCenterFrequencies();


    private:

        struct Band {
            double centerFrequency;
            double bandwidth;
        };

        QAudioFormat format;

        bool            on;
        QVector<Band>   bands;
        QVector<double> gains;
        double          preAmp;

        QMutex filtersMutex;

        TimedChunkQueue *chunkQueue;
        QMutex          *chunkQueueMutex;

        IIRFilter::SampleTypes sampleType;
        int                    sampleRate;
        double                 replayGain;
        double                 currentReplayGain;

        IIRFilterChain *equalizerFilters;

        double eqOffMinValue;
        double eqOffMaxValue;
        int    eqOffCurrentChannel;

        void createFilters();


    public slots:

        void run();

        void chunkAvailable(int maxToProcess);
        void playBegins();
        void setReplayGain(double replayGain);
        void requestForReplayGainInfo();


    signals:

        void chunkEqualized(TimedChunk chunk);
        void replayGainChanged(double current);
};

#endif // EQUALIZER_H
