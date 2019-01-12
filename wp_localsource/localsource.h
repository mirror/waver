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
#include <taglib/fileref.h>
#include <taglib/tstring.h>

#include "filescanner.h"
#include "../waver/pluginfactory.h"
#include "../waver/pluginglobals.h"
#include "../waver/API/pluginsource_006.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


extern "C" WP_LOCALSOURCE_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);


class WP_LOCALSOURCE_EXPORT LocalSource : public PluginSource_006 {
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
        int                    unableToStartCount;

        QStringList directories;
        QStringList trackFileNames;
        QStringList alreadyPlayedTrackFileNames;
        QStringList bannedFileNames;
        QStringList lovedFileNames;
        QStringList alreadyPlayedLovedFileNames;

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
        TrackInfo trackInfoFromFilePath(QString filePath, bool *tagLibOK);

        int  variationSettingId();
        void variationSetCurrentRemainingDir();

        void addToExtraInfo(ExtraInfo *extraInfo, QUrl url, QString key, QVariant value);
        void sendDiagnosticsData();


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

        void unableToStart(QUuid uniqueId, QUrl url)                        override;
        void castFinishedEarly(QUuid uniqueId, QUrl url, int playedSeconds) override;
        void done(QUuid uniqueId, QUrl url, bool wasError)                  override;
        void getPlaylist(QUuid uniqueId, int trackCount, int mode)          override;
        void getReplacement(QUuid uniqueId)                                 override;
        void getOpenTracks(QUuid uniqueId, QString parentId)                override;
        void resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks)  override;

        void search(QUuid uniqueId, QString criteria)                   override;
        void action(QUuid uniqueId, int actionKey, TrackInfo trackInfo) override;


    private slots:

        void scannerFoundFirst();
        void scannerFinished();

        void readyTimer();

};

#endif // LOCALSOURCE_H
