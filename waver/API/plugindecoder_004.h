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


#ifndef PLUGINDECODER_H
#define PLUGINDECODER_H

#include <QAudioBuffer>
#include <QUrl>

#include "pluginbase_004.h"


class PluginDecoder_004 : public PluginBase_004 {
        Q_OBJECT


    public:

        Q_INVOKABLE virtual void setUrl(QUrl url)                = 0;
        Q_INVOKABLE virtual void setUserAgent(QString userAgent) = 0;


    signals:

        void bufferAvailable(QUuid uniqueId, QAudioBuffer *buffer);
        void finished(QUuid uniqueId);
        void error(QUuid uniqueId, QString errorMessage);


    public slots:

        virtual void start(QUuid uniqueId) = 0;

        virtual void bufferDone(QUuid uniqueId, QAudioBuffer *buffer) = 0;

};

#endif // PLUGINDECODER_H
