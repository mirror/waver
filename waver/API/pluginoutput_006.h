/*
    This file is part of Waver

    Copyright (C) 2018-2019 Peter Papp <peter.papp.p@gmail.com>

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


#ifndef PLUGINOUTPUT_H
#define PLUGINOUTPUT_H

#include <QAudioBuffer>
#include <QMutex>

#include "../pluginglobals.h"
#include "pluginbase_006.h"


class PluginOutput_006 : public PluginBase_006 {
        Q_OBJECT


    public:

        Q_INVOKABLE virtual void setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex) = 0;
        Q_INVOKABLE virtual bool isMainOutput()                                                     = 0;


    signals:

        void positionChanged(QUuid uniqueId, qint64 posMilliseconds);
        void bufferDone(QUuid uniqueId, QAudioBuffer *buffer);
        void bufferUnderrun(QUuid uniqueId);
        void error(QUuid uniqueId, QString errorMessage);

        void window(QString qmlString);


    public slots:

        virtual void bufferAvailable(QUuid uniqueId)            = 0;
        virtual void mainOutputPosition(qint64 posMilliseconds) = 0;

        virtual void pause(QUuid uniqueId)  = 0;
        virtual void resume(QUuid uniqueId) = 0;
};

#endif // PLUGINOUTPUT_H
