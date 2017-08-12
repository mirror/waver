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


#ifndef SOUNDOUTPUT_H
#define SOUNDOUTPUT_H

#include "wp_soundoutput_global.h"

#include <QAudioBuffer>
#include <QAudioFormat>
#include <QAudioOutput>
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QIODevice>
#include <QMutex>
#include <QThread>
#include <QTimer>

#include "feeder.h"
#include "../waver/pluginfactory.h"
#include "../waver/pluginoutput.h"


extern "C" WP_SOUNDOUTPUT_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);


// TODO: QAudioOutput->setVolume somehow kills the output, it must be investigated. For now, volume control is disabled.


class WP_SOUNDOUTPUT_EXPORT SoundOutput : public PluginOutput {
        Q_OBJECT

    public:

        int     pluginType()                                                       override;
        QString pluginName()                                                       override;
        int     pluginVersion()                                                    override;
        void    setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex) override;
        bool    isMainOutput()                                                     override;
        QUuid   persistentUniqueId()                                               override;
        bool    hasUI()                                                            override;

        explicit SoundOutput();
        ~SoundOutput();


    private:

        static const qint64 NOTIFICATION_INTERVAL_MILLISECONDS = 100;
        static const int    FADE_DIRECTION_NONE                = 0;
        static const int    FADE_DIRECTION_IN                  = 1;
        static const int    FADE_DIRECTION_OUT                 = 2;

        QUuid id;

        BufferQueue *bufferQueue;
        QMutex      *bufferQueueMutex;

        QAudioOutput *audioOutput;
        QIODevice    *audioIODevice;

        QByteArray bytesToPlay;
        QMutex     bytesToPlayMutex;

        QThread feederThread;
        Feeder *feeder;

        bool   wasError;
        bool   timerWaits;
        qint64 notificationCounter;

        int    fadeDirection;
        qint64 fadePercent;
        int    fadeSeconds;
        double fadeFrameCount;
        bool   sendFadeComplete;

        double volume;

        void fillBytesToPlay();
        void applyFade();
        void clearBuffers();


    public slots:

        void run() override;

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration) override;

        void getUiQml(QUuid uniqueId)                         override;
        void uiResults(QUuid uniqueId, QJsonDocument results) override;

        void bufferAvailable(QUuid uniqueId) override;

        void pause(QUuid uniqueId)                override;
        void resume(QUuid uniqueId)               override;
        void fadeIn(QUuid uniqueId, int seconds)  override;
        void fadeOut(QUuid uniqueId, int seconds) override;


    private slots:

        void timerTimeout();

        void audioOutputNotification();
        void audioOutputStateChanged(QAudio::State state);

};

#endif // SOUNDOUTPUT_H
