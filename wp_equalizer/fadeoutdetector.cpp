/*
    This file is part of Waver

    Copyright (C) 2017-2019 Peter Papp <peter.papp.p@gmail.com>

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


#include "fadeoutdetector.h"

FadeOutDetector::FadeOutDetector(IIRFilter::SampleTypes sampleType, int sampleRate)
{
    this->sampleType     = sampleType;
    this->sampleRate     = sampleRate;

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

    frameCounter         = 0;
    positionUSec         = 0;
    firstNonSilentUSec   = -1000;
    lastNonSilentUSec    = 0;
    sum                  = 0.0;
    sumCounter           = 0;
    envelopeLastPosition = 0;
    firstAverage         = 0.0;
}


void FadeOutDetector::filterCallback(double *sample, int channelIndex)
{
    // process right and left channels only (PCM might be quadro, surround, or even more channels)
    if (channelIndex >= 2) {
        return;
    }

    // frame counter on the first channel, also conversion to mono
    if (channelIndex == 0) {
        frameCounter++;
        positionUSec = qRound64(((double)frameCounter / (double)sampleRate) * 1000000.00);

        stereoToMono = *sample;
        return;
    }

    // conversion to mono
    stereoToMono = stereoToMono + *sample;
    stereoToMono = stereoToMono / 2;

    // have to scale value if not the expected type
    if (sampleType != IIRFilter::int16Sample) {
        stereoToMono = (((stereoToMono - sampleMin) / sampleRange) * int16Range) + int16Min;
    }

    // we need the absolute value from now on
    stereoToMono = qAbs(stereoToMono);

    // non-silent positions
    if (stereoToMono >= (int16Max / 100)) {
        lastNonSilentUSec = positionUSec;
        if (firstNonSilentUSec < 0) {
            firstNonSilentUSec = positionUSec;
        }
    }

    // summarize
    sum = sum + stereoToMono;
    sumCounter++;

    // envelope: moving average of 1 second averages
    if (positionUSec >= (envelopeLastPosition + 1000000)) {
        double average = sum / sumCounter;

        if ((envelope.count() == 0) && (firstAverage == 0.0)) {
            firstAverage = average;
        }
        else if (envelope.count() == 0) {
            envelope.append({ positionUSec, average, (average + firstAverage) / 2 });
        }
        else {
            envelope.append({ positionUSec, average, (average + envelope.last().oneSecAverage) / 2 });
        }
        sum                  = 0.0;
        sumCounter           = 0;
        envelopeLastPosition = positionUSec;
    }
}


qint64 FadeOutDetector::getFadeOutStartPoisitionMSec()
{
    if (envelope.count() < 1) {
        return std::numeric_limits <qint64>::max();
    }

    qint64 retval   = envelope.last().positionUSec / 1000;
    double previous = 0.0;

    int i = envelope.count() - 1;
    while ((i >= 0) && (envelope.at(i).movingAverage < 10.00)) {
        retval   = envelope.at(i).positionUSec / 1000;
        previous = envelope.at(i).movingAverage;
        i--;
    }
    while ((i >= 0) && (envelope.at(i).movingAverage > previous)) {
        retval   = envelope.at(i).positionUSec / 1000;
        previous = envelope.at(i).movingAverage;
        i--;
    }

    return retval;
}


qint64 FadeOutDetector::getFadeOutEndPoisitionMSec()
{
    if (envelope.count() < 1) {
        return std::numeric_limits <qint64>::max();
    }

    return envelope.last().positionUSec / 1000;
}


qint64 FadeOutDetector::getFirstNonSilentMSec()
{
    return firstNonSilentUSec / 1000;
}


qint64 FadeOutDetector::getLastNonSilentMSec()
{
    return lastNonSilentUSec / 1000;
}


qint64 FadeOutDetector::getFadeInEndPoisitionMSec()
{
    if (envelope.count() < 1) {
        return 0;
    }

    qint64 retval   = envelope.first().positionUSec / 1000;
    double previous = 0.0;

    int i = 0;
    while ((i < envelope.count()) && (envelope.at(i).movingAverage > previous)) {
        retval   = envelope.at(i).positionUSec / 1000;
        previous = envelope.at(i).movingAverage;
        i++;
    }

    return retval;
}


qint64 FadeOutDetector::checkedPositionMSec()
{
    return positionUSec / 1000;
}
