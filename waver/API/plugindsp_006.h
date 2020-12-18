/*
    This file is part of Waver

    Copyright (C) 2017-2020 Peter Papp <peter.papp.p@gmail.com>

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


#ifndef PLUGINDSP_H
#define PLUGINDSP_H

#include "../pluginglobals.h"
#include "pluginbase_006.h"

#include <QVariant>


class PluginDsp_004 : public PluginBase_006 {
        Q_OBJECT


    public:

        // greater number means less priority
        Q_INVOKABLE virtual int priority() = 0;

        Q_INVOKABLE virtual void setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex) = 0;
        Q_INVOKABLE virtual void setCast(bool cast)                                                 = 0;


    signals:

        void bufferDone(QUuid uniqueId, QAudioBuffer *buffer);


    public slots:

        virtual void bufferAvailable(QUuid uniqueId)                                                              = 0;
        virtual void playBegin(QUuid uniqueId)                                                                    = 0;
        virtual void messageFromDspPrePlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value) = 0;

};

#endif // PLUGINDSP_H
