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


#ifndef EQUALIZER_H
#define EQUALIZER_H

#include "wp_equalizer_global.h"

#include <QUuid>
#include <QVector>

#include "analyzer.h"
#include "iirfilter.h"
#include "iirfilterchain.h"
#include "iirfiltercallback.h"
#include "../waver/API/plugindsp_001.h"


class WP_EQUALIZER_EXPORT Equalizer : public PluginDsp_001, IIRFilterCallback {
        Q_OBJECT

    public:

        int     pluginType()                                                       override;
        QString pluginName()                                                       override;
        int     pluginVersion()                                                    override;
        QString waverVersionAPICompatibility()                                     override;
        QUuid   persistentUniqueId()                                               override;
        bool    hasUI()                                                            override;
        int     priority()                                                         override;
        void    setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex) override;

        Equalizer();
        ~Equalizer();

        void filterCallback(double *sample, int channelIndex) override;


    private:

        struct Band {
            double centerFrequency;
            double bandwidth;
        };

        QUuid id;

        QVector<Band>   bands;
        QVector<double> gains;
        double          preAmp;

        BufferQueue *bufferQueue;
        QMutex      *bufferQueueMutex;

        IIRFilter::SampleTypes sampleType;
        int                    sampleRate;
        double                 replayGain;
        double                 currentReplayGain;

        IIRFilterChain *equalizerFilters;

        void createFilters();


    public slots:

        void run() override;

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration) override;

        void getUiQml(QUuid uniqueId)                         override;
        void uiResults(QUuid uniqueId, QJsonDocument results) override;

        void bufferAvailable(QUuid uniqueId)                                                              override;
        void playBegin(QUuid uniqueId)                                                                    override;
        void messageFromDspPrePlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value) override;
};

#endif // EQUALIZER_H
