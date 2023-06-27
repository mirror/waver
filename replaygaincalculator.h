/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef REPLAYGAINCALCULATOR_H
#define REPLAYGAINCALCULATOR_H

#include <QVector>

#include <waveriir/iirfilter.h>
#include <waveriir/iirfiltercallback.h>


class ReplayGainCalculator : IIRFilterCallback {

    public:

        ReplayGainCalculator(IIRFilter::SampleTypes sampleType, int sampleRate, bool calculateScaledPeak = false);

        void   filterCallback(double *sample, int channelIndex) override;
        double calculateResult();
        void   reset();


    private:

        static constexpr double RMS_BLOCK_SECONDS    = 0.05;
        static const     int    STATS_MAX_DB         = 120;
        static const     int    STATS_STEPS_PER_DB   = 100;
        static const     int    STATS_TABLE_MAX      = (STATS_MAX_DB *STATS_STEPS_PER_DB) - 1;
        static constexpr double STATS_RMS_PERCEPTION = 0.95;
        static constexpr double PINK_NOISE_REFERENCE = 64.82;

        IIRFilter::SampleTypes sampleType;

        bool   calculateScaledPeak;

        int  samplesPerRmsBlock;

        double int16Min;
        double int16Max;
        double int16Range;
        double sampleMin;
        double sampleRange;

        double stereoRmsSum;
        int    countRmsSum;

        unsigned int statsTable[STATS_MAX_DB * STATS_STEPS_PER_DB];

};


#endif // REPLAYGAINCALCULATOR_H
