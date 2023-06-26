/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef PREANALYZER_H
#define PREANALYZER_H


#include <QAudioFormat>
#include <QMutex>
#include <QObject>
#include <waveriir/iirfilter.h>
#include <waveriir/iirfilterchain.h>
#include <waveriir/iirfiltercallback.h>

#include "equalizer.h"
#include "globals.h"
#include "pcmcache.h"
#include "replaygaincalculator.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


#define PRE_EQ_REF_3  { 0, 0, 0 }
#define PRE_EQ_REF_4  { 0, 0, 0, 0 }
// #define PRE_EQ_REF_5  { 30, 4, 8, -1.5, -3 }   // N = 4
#define PRE_EQ_REF_5  { 20, 1.5, 3, -4, -6 }      // N = 8
#define PRE_EQ_REF_6  { 0, 0, 0, 0, 0, 0 }
#define PRE_EQ_REF_7  { 0, 0, 0, 0, 0, 0, 0 }
#define PRE_EQ_REF_8  { 0, 0, 0, 0, 0, 0, 0, 0 }
#define PRE_EQ_REF_9  { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
#define PRE_EQ_REF_10 { 23, 16, 15, 11.5, 9, 6, -2.5, 1.5, -5, 3 }   // N = 8


class PreAnalyzer : public QObject
{
    Q_OBJECT


    public:

        explicit PreAnalyzer(QAudioFormat format, PCMCache *cache, int bandCount, int N);
        ~PreAnalyzer();

        void setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex);
        int  getbandCount();


    private:

        static const int PRE_EQ_CALC_US = 4 * 1000 * 1000;

        const double PRE_EQ_MAX_DB = 35.0;
        const double PRE_EQ_MIN_DB = -15.0;

        int              bandCount;
        int              N;
        QAudioFormat     format;
        PCMCache        *cache;
        QVector<double>  referenceLevels;
        double           absolutePeak;

        IIRFilter::SampleTypes sampleType;
        qint64                 resultLastCalculated;
        bool                   decoderFinished;

        BufferQueue *bufferQueue;
        QMutex      *bufferQueueMutex;

        IIRFilterChain                  *measurementFilters;
        QVector<ReplayGainCalculator *>  replayGainCalculators;


    public slots:

        void run();
        void bufferAvailable();
        void decoderDone();


    signals:

        void running();
        void measuredGains(QVector<double> gains, double absolutePeak);
};

#endif // PREANALYZER_H
