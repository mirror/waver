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


#include "replaygaincalculator.h"

// constructor
ReplayGainCalculator::ReplayGainCalculator(IIRFilter::SampleTypes sampleType, int sampleRate)
{
    this->sampleType = sampleType;

    samplesPerRmsBlock = ((int)ceil(sampleRate * RMS_BLOCK_SECONDS)) * 2;

    int16Min   = std::numeric_limits<qint16>::min();
    int16Max   = std::numeric_limits<qint16>::max();
    int16Range = int16Max - int16Min;

    double sampleMax;
    switch (sampleType) {
    case IIRFilter::int8Sample:
        sampleMin = std::numeric_limits<qint8>::min();
        sampleMax = std::numeric_limits<qint8>::max();
        break;
    case IIRFilter::uint8Sample:
        sampleMin = std::numeric_limits<quint8>::min();
        sampleMax = std::numeric_limits<quint8>::max();
        break;
    case IIRFilter::uint16Sample:
        sampleMin = std::numeric_limits<quint16>::min();
        sampleMax = std::numeric_limits<quint16>::max();
        break;
    case IIRFilter::int32Sample:
        sampleMin = std::numeric_limits<qint32>::min();
        sampleMax = std::numeric_limits<qint32>::max();
        break;
    case IIRFilter::uint32Sample:
        sampleMin = std::numeric_limits<quint32>::min();
        sampleMax = std::numeric_limits<quint32>::max();
        break;
    case IIRFilter::floatSample:
        sampleMin = std::numeric_limits<float>::min();
        sampleMax = std::numeric_limits<float>::max();
        break;
    default:
        break;
    }
    sampleRange = sampleMax - sampleMin;

    stereoRmsSum = 0.0;
    countRmsSum  = 0;

    reset();
}


// filter callback analyze PCM data
void ReplayGainCalculator::filterCallback(double *sample, int channelIndex)
{
    // process right and left channels only (PCM might be quadro, surround, or even more channels)
    if (channelIndex >= 2) {
        return;
    }

    // so we can modify it if needed
    double sampleValue = *sample;

    // have to scale value if not the expected type
    if (sampleType != IIRFilter::int16Sample) {
        sampleValue = (((sampleValue - sampleMin) / sampleRange) * int16Range) + int16Min;
    }

    // replay gain: sum of squares for RMS
    // TODO support mono too (it's simple, just have to do this stereoRmsSum addition twice, becuase the same sound will be in both speakers)
    stereoRmsSum += (sampleValue * sampleValue);
    countRmsSum++;

    // replay gain: statistical processing
    if (countRmsSum == samplesPerRmsBlock) {
        // calculate the RMS and convert it to dB
        double rmsAverageTableSlot = (double)STATS_STEPS_PER_DB * 10. * log10(stereoRmsSum / (samplesPerRmsBlock / 2) * 0.5 + 1.e-37);

        // cap RMS average so it fits into the statistics table
        if (rmsAverageTableSlot < 0) {
            rmsAverageTableSlot = 0;
        }
        if (rmsAverageTableSlot > STATS_TABLE_MAX) {
            rmsAverageTableSlot = STATS_TABLE_MAX;
        }

        // increase the appropriate slot in the staticstics table
        statsTable[(int)rmsAverageTableSlot]++;

        // reset variables
        stereoRmsSum = 0.0;
        countRmsSum  = 0;
    }
}


// calculations to get the result
double ReplayGainCalculator::calculateResult()
{
    long sum = 0;
    for (int i = 0; i < (STATS_MAX_DB * STATS_STEPS_PER_DB); i++) {
        sum += statsTable[i];
    }

    if (sum == 0) {
        return 0.0;
    }

    int percepted = (int)ceil(sum * (1. - STATS_RMS_PERCEPTION));

    int statElement;
    for (statElement = (STATS_MAX_DB * STATS_STEPS_PER_DB); statElement-- > 0;) {
        if ((percepted -= statsTable[statElement]) <= 0) {
            break;
        }
    }

    return (double)(PINK_NOISE_REFERENCE - (double)statElement / (double)STATS_STEPS_PER_DB);
}


// reset
void ReplayGainCalculator::reset()
{
    stereoRmsSum = 0.0;
    countRmsSum  = 0;

    memset(&statsTable, 0, STATS_MAX_DB * STATS_STEPS_PER_DB * sizeof(int));
}
