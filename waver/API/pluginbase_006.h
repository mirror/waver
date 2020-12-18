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


#ifndef PLUGINBASE_H
#define PLUGINBASE_H

#include <QAudioBuffer>
#include <QJsonDocument>
#include <QMutex>
#include <QObject>
#include <QUuid>
#include <QVector>

#include "../pluginglobals.h"

class PluginBase_006 : public QObject {
        Q_OBJECT


    public:

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
        void executeSql(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString sql, QVariantList values);
        void executeGlobalSql(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString sql, QVariantList values);
        void messageToPlugin(QUuid uniqueId, QUuid destinationUniqueId, int messageId, QVariant value);

        void uiQml(QUuid uniqueId, QString qmlString);

        void infoMessage(QUuid uniqueId, QString message);
        void diagnostics(QUuid uniqueId, DiagnosticData data);


    public slots:

        virtual void run() = 0;

        virtual void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)                                                                  = 0;
        virtual void loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)                                                            = 0;
        virtual void sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)       = 0;
        virtual void globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results) = 0;
        virtual void sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)              = 0;
        virtual void messageFromPlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value)                                            = 0;

        virtual void getUiQml(QUuid uniqueId)                         = 0;
        virtual void uiResults(QUuid uniqueId, QJsonDocument results) = 0;
        
        virtual void startDiagnostics(QUuid uniqueId) = 0;
        virtual void stopDiagnostics(QUuid uniqueId)  = 0;
};


#endif // PLUGINBASE_H

