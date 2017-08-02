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


#ifndef FADEOUTDETECTOR_H
#define FADEOUTDETECTOR_H

#include <QtGlobal>

#include <QVector>

#include "iirfilter.h"
#include "iirfiltercallback.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


class FadeOutDetector : public IIRFilterCallback {

    public:

        FadeOutDetector(IIRFilter::SampleTypes sampleType, int sampleRate);

        void   filterCallback(double *sample, int channelIndex) override;
        qint64 getFadeOutStartPoisitionMSec();
        qint64 getFadeOutEndPoisitionMSec();
        qint64 getFirstNonSilentMSec();
        qint64 getLastNonSilentMSec();


    private:

        struct EnvelopePoint {
            qint64 positionUSec;
            double oneSecAverage;
            double movingAverage;
        };

        IIRFilter::SampleTypes sampleType;
        int                    sampleRate;

        double int16Min;
        double int16Max;
        double int16Range;
        double sampleMin;
        double sampleRange;

        qint64 frameCounter;
        qint64 positionUSec;
        qint64 firstNonSilentUSec;
        qint64 lastNonSilentUSec;

        double stereoToMono;
        double sum;
        double sumCounter;

        QVector<EnvelopePoint> envelope;
        qint64                 envelopeLastPosition;
        double                 firstAverage;
};

#endif // FADEOUTDETECTOR_H
