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


#ifndef LOCALSOURCE_H
#define LOCALSOURCE_H

#include "wp_localsource_global.h"

#include <QtGlobal>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMimeDatabase>
#include <QMutex>
#include <QObject>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUuid>
#include <QVariant>
#include <QVector>

#include "filescanner.h"
#include "../waver/pluginfactory.h"
#include "../waver/pluginglobals.h"
#include "../waver/API/pluginsource_004.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


extern "C" WP_LOCALSOURCE_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);


class WP_LOCALSOURCE_EXPORT LocalSource : public PluginSource_004 {
        Q_OBJECT

    public:

        int     pluginType()                    override;
        QString pluginName()                    override;
        int     pluginVersion()                 override;
        QString waverVersionAPICompatibility()  override;
        QUuid   persistentUniqueId()            override;
        bool    hasUI()                         override;
        void    setUserAgent(QString userAgent) override;

        explicit LocalSource();
        ~LocalSource();


    private:

        QUuid id;

        QMutex                 mutex;
        QVector<FileScanner *> scanners;
        bool                   readyEmitted;

        QStringList directories;
        QStringList trackFileNames;
        QStringList alreadyPlayedTrackFileNames;
        QStringList bannedFileNames;

        QString variationSetting;
        int     variationCurrent;
        int     variationRemaining;
        QString variationDir;
        int     variationSetCountSinceHigh;
        int     variationSetCountSinceLow;

        bool sendDiagnostics;

        QMimeDatabase mimeDatabase;

        void scanDir(QString dir);

        QJsonDocument configToJson();
        QJsonDocument configToJsonGlobal();
        void          jsonToConfig(QJsonDocument jsonDocument);
        void          jsonToConfigGlobal(QJsonDocument jsonDocument);

        bool      isTrackFile(QFileInfo fileInfo);
        TrackInfo trackInfoFromFilePath(QString filePath);

        int  variationSettingId();
        void variationSetCurrentRemainingDir();

        void sendDiagnosticsData();


    public slots:

        void run() override;

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)       override;
        void loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration) override;

        void sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)       override;
        void globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results) override;
        void sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)              override;

        void getUiQml(QUuid uniqueId)                         override;
        void uiResults(QUuid uniqueId, QJsonDocument results) override;

        void startDiagnostics(QUuid uniqueId) override;
        void stopDiagnostics(QUuid uniqueId)  override;

        void unableToStart(QUuid uniqueId, QUrl url)                       override;
        void getPlaylist(QUuid uniqueId, int maxCount)                     override;
        void getOpenTracks(QUuid uniqueId, QString parentId)               override;
        void resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks) override;

        void search(QUuid uniqueId, QString criteria)        override;
        void action(QUuid uniqueId, int actionKey, QUrl url) override;


    private slots:

        void scannerFoundFirst();
        void scannerFinished();

        void readyTimer();

};

#endif // LOCALSOURCE_H
