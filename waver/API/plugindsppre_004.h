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


#ifndef PLUGINDSPPRE_H
#define PLUGINDSPPRE_H

#include "../pluginglobals.h"
#include "pluginbase_004.h"

#include <QVariant>


class PluginDspPre_004 : public PluginBase_004 {
        Q_OBJECT


    public:

        // greater number means less priority
        Q_INVOKABLE virtual int priority() = 0;

        Q_INVOKABLE virtual void setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex) = 0;


    signals:

        void requestFadeIn(QUuid uniqueId, qint64 lengthMilliseconds);
        void requestFadeInForNextTrack(QUuid uniqueId, qint64 lengthMilliseconds);
        void requestInterrupt(QUuid uniqueId, qint64 posMilliseconds, bool withFadeOut);
        void requestAboutToFinishSend(QUuid uniqueId, qint64 posMilliseconds);
        void requestAboutToFinishSendForPreviousTrack(QUuid uniqueId, qint64 posBeforeEndMilliseconds);
        void messageToDspPlugin(QUuid uniqueId, QUuid destinationUniqueId, int messageId, QVariant value);

        void bufferDone(QUuid uniqueId, QAudioBuffer *buffer);


    public slots:

        virtual void bufferAvailable(QUuid uniqueId) = 0;
        virtual void decoderDone(QUuid uniqueId)     = 0;
};

#endif // PLUGINDSPPRE_H
