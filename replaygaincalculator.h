/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef REPLAYGAINCALCULATOR_H
#define REPLAYGAINCALCULATOR_H

#include <QVector>
#include "globals.h"

#include <iirfilter.h>
#include <iirfiltercallback.h>


class ReplayGainCalculator : IIRFilterCallback {

    public:

        enum SilenceType {
            SilenceAtBeginning,
            SilenceIntermediate,
            SilenceAtEnd
        };
        struct SilenceRange {
            SilenceType type;
            qint64      startMicroseconds;
            qint64      endMicroseconds;
        };
        typedef QVector<ReplayGainCalculator::SilenceRange> Silences;

        ReplayGainCalculator(IIRFilter::SampleTypes sampleType, int sampleRate);

        void     filterCallback(double *sample, int channelIndex) override;
        double   calculateResult();
        Silences getSilences(bool addFinalSilence);
        void     reset();


    private:

        static constexpr double RMS_BLOCK_SECONDS    = 0.05;
        static const     int    STATS_MAX_DB         = 120;
        static const     int    STATS_STEPS_PER_DB   = 100;
        static const     int    STATS_TABLE_MAX      = (STATS_MAX_DB *STATS_STEPS_PER_DB) - 1;
        static constexpr double STATS_RMS_PERCEPTION = 0.95;
        static constexpr double PINK_NOISE_REFERENCE = 64.82;
        static const     int    SILENCE_MIN_MICROSEC = 2750000;

        IIRFilter::SampleTypes sampleType;
        int                    sampleRate;

        int  samplesPerRmsBlock;

        double int16Min;
        double int16Max;
        double int16Range;
        double sampleMin;
        double sampleRange;
        double silenceThreshold;

        double stereoRmsSum;
        int    countRmsSum;
        qint64 framesCount;
        qint64 silenceStart;

        QVector<SilenceRange> silences;

        unsigned int statsTable[STATS_MAX_DB * STATS_STEPS_PER_DB];
};


#endif // REPLAYGAINCALCULATOR_H
