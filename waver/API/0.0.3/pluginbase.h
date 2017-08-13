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


#ifndef PLUGINBASE_H
#define PLUGINBASE_H

#include <QAudioBuffer>
#include <QJsonDocument>
#include <QMutex>
#include <QObject>
#include <QUuid>
#include <QVector>


class PluginBase : public QObject {
        Q_OBJECT


    public:

        typedef QVector<QAudioBuffer *> BufferQueue;

        static const int PLUGIN_TYPE_SOURCE  = 1;
        static const int PLUGIN_TYPE_DECODER = 2;
        static const int PLUGIN_TYPE_DSP_PRE = 4;
        static const int PLUGIN_TYPE_DSP     = 8;
        static const int PLUGIN_TYPE_OUTPUT  = 16;
        static const int PLUGIN_TYPE_INFO    = 32;
        static const int PLUGIN_TYPE_ALL     =
            PLUGIN_TYPE_SOURCE  |
            PLUGIN_TYPE_DECODER |
            PLUGIN_TYPE_DSP_PRE |
            PLUGIN_TYPE_DSP     |
            PLUGIN_TYPE_OUTPUT  |
            PLUGIN_TYPE_INFO;

        Q_INVOKABLE virtual int     pluginType()                   = 0;
        Q_INVOKABLE virtual QString pluginName()                   = 0;
        Q_INVOKABLE virtual int     pluginVersion()                = 0;
        Q_INVOKABLE virtual QString waverVersionAPICompatibility() = 0;
        Q_INVOKABLE virtual QUuid   persistentUniqueId()           = 0;
        Q_INVOKABLE virtual bool    hasUI()                        = 0;


    signals:

        void loadConfiguration(QUuid uniqueId);
        void loadGlobalConfiguration(QUuid uniqueId);
        void saveConfiguration(QUuid uniqueId, QJsonDocument configuration);
        void saveGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration);

        void uiQml(QUuid uniqueId, QString qmlString);

        void infoMessage(QUuid uniqueId, QString message);


    public slots:

        virtual void run() = 0;

        virtual void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration) = 0;
        virtual void loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration) = 0;

        virtual void getUiQml(QUuid uniqueId)                         = 0;
        virtual void uiResults(QUuid uniqueId, QJsonDocument results) = 0;

};


#endif // PLUGINBASE_H

