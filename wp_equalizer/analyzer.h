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


#ifndef ANALYZER_H
#define ANALYZER_H

#include "wp_equalizer_global.h"

#include <QtGlobal>

#include <QByteArray>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>

#include "coefficientlist.h"
#include "iirfilter.h"
#include "iirfilterchain.h"
#include "replaygaincalculator.h"
#include "fadeoutdetector.h"

#include "../waver/API/0.0.1/plugindsppre.h"
#include "../waver/track.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


// filter coefficients for standard ReplayGain calculation
#define REPLAYGAIN_44100_YULEWALK_A    { 0.05418656406430, -0.02911007808948, -0.00848709379851, -0.00851165645469, -0.00834990904936, 0.02245293253339, -0.02596338512915, 0.01624864962975, -0.00240879051584, 0.00674613682247, -0.00187763777362 }
#define REPLAYGAIN_44100_YULEWALK_B    { -3.47845948550071, 6.36317777566148, -8.54751527471874, 9.47693607801280, -8.81498681370155, 6.85401540936998, -4.39470996079559, 2.19611684890774, -0.75104302451432, 0.13149317958808 }
#define REPLAYGAIN_44100_BUTTERWORTH_A { 0.98500175787242, -1.97000351574484, 0.98500175787242 }
#define REPLAYGAIN_44100_BUTTERWORTH_B { -1.96977855582618, 0.97022847566350 }


class WP_EQUALIZER_EXPORT Analyzer : PluginDspPre {
        Q_OBJECT

    public:

        static const int DSP_MESSAGE_REPLAYGAIN = 0;

        int     pluginType()                                                       override;
        QString pluginName()                                                       override;
        int     pluginVersion()                                                    override;
        QString waverVersionAPICompatibility()                                     override;
        QUuid   persistentUniqueId()                                               override;
        bool    hasUI()                                                            override;
        int     priority()                                                         override;
        void    setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex) override;

        Analyzer();
        ~Analyzer();


    private:

        QUuid id;

        bool enableTransitions;

        BufferQueue *bufferQueue;
        QMutex      *bufferQueueMutex;

        bool                    filtersSetup;
        bool                    firstNonSilentPositionChecked;
        qint64                  resultLastCalculated;
        IIRFilter::SampleTypes  sampleType;
        IIRFilterChain         *replayGainFilter;
        ReplayGainCalculator   *replayGainCalculator;
        FadeOutDetector        *fadeOutDetector;

        bool decoderFinished;

        void transition();


    public slots:

        void run() override;

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration) override;

        void getUiQml(QUuid uniqueId)                         override;
        void uiResults(QUuid uniqueId, QJsonDocument results) override;

        void bufferAvailable(QUuid uniqueId)             override;
        void decoderDone(QUuid uniqueId)                 override;
};

#endif // ANALYZER_H
