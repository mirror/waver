/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "replaygaincalculator.h"

// constructor
ReplayGainCalculator::ReplayGainCalculator(IIRFilter::SampleTypes sampleType, int sampleRate)
{
    qRegisterMetaType<ReplayGainCalculator::Silences>("ReplayGainCalculator::Silences");

    this->sampleType = sampleType;
    this->sampleRate = sampleRate;

    samplesPerRmsBlock = ((int)ceil((double)sampleRate * RMS_BLOCK_SECONDS)) * 2;

    int16Min   = std::numeric_limits<qint16>::min();
    int16Max   = std::numeric_limits<qint16>::max();
    int16Range = int16Max - int16Min;

    sampleMin = int16Min;

    double sampleMax = int16Max;
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
    }
    sampleRange      = sampleMax - sampleMin;
    silenceThreshold = pow(10, SILENCE_THRESHOLD_DB / 10) * sampleMax;

    stereoRmsSum = 0.0;
    countRmsSum  = 0;
    framesCount  = 0;
    silenceStart = 0;

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

    // just in case
    if (std::isnan(sampleValue)) {
        sampleValue = 0;
    }

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

    // silence detector (only record silences longer then specified except for silence at the beginning of track)
    if (!silenceStart && (abs(sampleValue) <= silenceThreshold)) {
        silenceStart = static_cast<qint64>(floor((double)framesCount / sampleRate * 1000000));
    }
    if ((silenceStart || (silences.count() == 0)) && (abs(sampleValue) > silenceThreshold)) {
        qint64 silenceEnd = static_cast<qint64>(floor(static_cast<double>(framesCount - 1) / sampleRate * 1000000));
        if (silences.count() == 0) {
            silences.append({ SilenceAtBeginning, silenceStart, silenceEnd });
        }
        else if (silenceEnd - silenceStart >= SILENCE_MIN_MICROSEC) {
            silences.append({ SilenceIntermediate, silenceStart, silenceEnd });
        }
        silenceStart = 0;
    }
    if (channelIndex == 1) {
        framesCount++;
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


QVector<ReplayGainCalculator::SilenceRange> ReplayGainCalculator::getSilences(bool addFinalSilence)
{
    QVector<SilenceRange> aCopy(silences);

    // silence at the end of track (set flag only after decoding has finished)
    if (addFinalSilence && silenceStart) {
        aCopy.append({ SilenceAtEnd, silenceStart, static_cast<qint64>(floor(static_cast<double>(framesCount - 1) / sampleRate * 1000000)) });
    }

    return aCopy;
}


// reset
void ReplayGainCalculator::reset()
{
    stereoRmsSum = 0.0;
    countRmsSum  = 0;

    memset(&statsTable, 0, STATS_MAX_DB * STATS_STEPS_PER_DB * sizeof(int));
    silences.clear();
}
