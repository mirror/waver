/*
    This file is part of Waver

    Copyright (C) 2018 Peter Papp <peter.papp.p@gmail.com>

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


#ifndef RMSMETER_H
#define RMSMETER_H

#include "wp_rmsmeter_global.h"

#include <cmath>
#include <QAudioBuffer>
#include <QAudioFormat>
#include <QFile>
#include <QTimer>
#include <QThread>
#include "websocketserver.h"

#include "../waver/pluginfactory.h"
#include "../waver/API/pluginoutput_006.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


extern "C" WP_RMSMETER_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);


class WP_RMSMETER_EXPORT RMSMeter : public PluginOutput_006 {
        Q_OBJECT

    public:

        int     pluginType()                                                       override;
        QString pluginName()                                                       override;
        int     pluginVersion()                                                    override;
        QString waverVersionAPICompatibility()                                     override;
        void    setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex) override;
        bool    isMainOutput()                                                     override;
        QUuid   persistentUniqueId()                                               override;
        bool    hasUI()                                                            override;

        explicit RMSMeter();
        ~RMSMeter();


    private:

        static int              instanceCount;
        static int              instanceTotal;
        static QThread         *webSocketServerThread;
        static WebSocketServer *webSocketServer;

        QUuid id;

        BufferQueue *bufferQueue;
        QMutex      *bufferQueueMutex;

        int instanceId;

        int audioFramePerVideoFrame;
        int frameCount;

        int dataType;

        double int16Min;
        double int16Max;
        double int16Range;
        double sampleMin;
        double sampleRange;

        int channelCount;
        int channelIndex;

        double lRmsSum;
        double rRmsSum;
        double lPeak;
        double rPeak;

        bool sendDiagnostics;

        void sendDiagnosticsData();


    signals:

        void rms(int instanceId, qint64 position, int channelIndex, double rms, double peak);
        void position(int instanceId, qint64 position);
        void clean(int instanceId);


    public slots:

        void run() override;

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)       override;
        void loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration) override;

        void sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)       override;
        void globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results) override;
        void sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)              override;

        void messageFromPlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value) override;

        void getUiQml(QUuid uniqueId)                         override;
        void uiResults(QUuid uniqueId, QJsonDocument results) override;

        void startDiagnostics(QUuid uniqueId) override;
        void stopDiagnostics(QUuid uniqueId)  override;

        void bufferAvailable(QUuid uniqueId) override;
        void mainOutputPosition(qint64 posMilliseconds) override;

        void pause(QUuid uniqueId)  override;
        void resume(QUuid uniqueId) override;


    private slots:

        void requestWindow();

};

#endif // RMSMETER_H
