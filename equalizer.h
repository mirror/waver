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
#include <iirfilter.h>
#include <iirfilterchain.h>
#include <iirfiltercallback.h>

#include "globals.h"


#define BANDS_3  { 62, 750, 5000 }
#define BANDS_4  { 62, 500, 2500, 7500 }
#define BANDS_5  { 62, 250, 750, 2500, 7500 }
#define BANDS_6  { 31, 62, 125, 250, 2500, 7500 }
#define BANDS_7  { 31, 62, 125, 250, 2500, 5000, 12500 }
#define BANDS_8  { 31, 62, 125, 250, 750, 2500, 5000, 12500 }
#define BANDS_9  { 31, 62, 125, 250, 500, 1000, 2500, 5000, 12500 }
#define BANDS_10 { 31, 62, 125, 250, 500, 1000, 2500, 5000, 10000, 16000 }


class Equalizer : public QObject, IIRFilterCallback {
        Q_OBJECT

    public:

        struct Band {
            double centerFrequency;
            double bandwidth;
        };
        typedef QVector<Band> Bands;

        static Bands calculateBands(QVector<double> centerFrequencies);
        static Bands calculateBands(std::initializer_list<double> centerFrequencies);

        Equalizer(QAudioFormat format);
        ~Equalizer();

        void setChunkQueue(TimedChunkQueue *chunkQueue, QMutex *chunkQueueMutex);
        void setGains(bool on, QVector<double> gains, double preAmp);

        void filterCallback(double *sample, int channelIndex) override;

        QVector<double> getBandCenterFrequencies();


    private:

        QAudioFormat format;

        bool                  on;
        Bands                 bands;
        QVector<double>       gains;
        double                preAmp;

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
