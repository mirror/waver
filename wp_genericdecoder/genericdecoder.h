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


#ifndef GENERICDECODER_H
#define GENERICDECODER_H

#include "wp_genericdecoder_global.h"

#include <QtMath>

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QMutex>
#include <QSysInfo>
#include <QThread>
#include <QVector>
#include <QWaitCondition>

#include "networkdownloader.h"
#include "../waver/pluginfactory.h"
#include "../waver/API/plugindecoder_004.h"


extern "C" WP_GENERICDECODER_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);


class WP_GENERICDECODER_EXPORT GenericDecoder : public PluginDecoder_004 {
        Q_OBJECT

    public:

        int     pluginType()                   override;
        QString pluginName()                   override;
        int     pluginVersion()                override;
        QString waverVersionAPICompatibility() override;
        QUuid   persistentUniqueId()           override;
        bool    hasUI()                        override;
        void    setUrl(QUrl url)               override;

        explicit GenericDecoder();
        ~GenericDecoder();


    private:

        static const unsigned long MAX_MEMORY   = 50 * 1024 * 1024;
        static const unsigned long USEC_PER_SEC = 1000000;

        QUuid id;
        QUrl  url;

        QAudioDecoder           *audioDecoder;
        QVector<QAudioBuffer *>  audioBuffers;

        QFile             *file;
        QThread            networkThread;
        NetworkDownloader *networkDownloader;

        QMutex         waitMutex;
        QWaitCondition waitCondition;

        bool          networkDeviceSet;
        unsigned long memoryUsage;
        qint64        decodedMicroSeconds;
        bool          sendDiagnostics;

        void    sendDiagnosticsData();
        QString formatBytes(double bytes);


    private slots:

        void networkReady();
        void networkError(QString errorString);

        void decoderBufferReady();
        void decoderFinished();
        void decoderError(QAudioDecoder::Error error);


    public slots:

        void run()                 override;
        void start(QUuid uniqueId) override;

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)       override;
        void loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration) override;

        void getUiQml(QUuid uniqueId)                         override;
        void uiResults(QUuid uniqueId, QJsonDocument results) override;

        void startDiagnostics(QUuid uniqueId) override;
        void stopDiagnostics(QUuid uniqueId)  override;

        void bufferDone(QUuid uniqueId, QAudioBuffer *buffer) override;

};

#endif // GENERICDECODER_H
