/*
    This file is part of Waver

    Copyright (C) 2017 Peter Papp <peter.papp.p@gmail.com>

    Please visit https://launchpad.net/waver for details

    Waver is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Waver is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    (GPL.TXT) along with Waver. If not, see <http://www.gnu.org/licenses/>.

*/


#ifndef REPLAYGAINCALCULATOR_H
#define REPLAYGAINCALCULATOR_H

#include <QVector>

#include "iirfilter.h"
#include "iirfiltercallback.h"


class ReplayGainCalculator : IIRFilterCallback {

    public:

        ReplayGainCalculator(IIRFilter::SampleTypes sampleType, int sampleRate);

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
