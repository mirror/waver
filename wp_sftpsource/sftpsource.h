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


#ifndef SFTPSOURCE_H
#define SFTPSOURCE_H

#include "wp_sftpsource_global.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariantHash>
#include <QVector>
#include <libssh2.h>
#include <taglib/fileref.h>
#include <taglib/tstring.h>

#include "sshclient.h"
#include "../waver/pluginfactory.h"
#include "../waver/pluginglobals.h"
#include "../waver/API/pluginsource_006.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif

#include <QThread>


extern "C" WP_SFTPSOURCE_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);


class WP_SFTPSOURCE_EXPORT SFTPSource : public PluginSource_006 {
        Q_OBJECT

    public:

        int     pluginType()                    override;
        QString pluginName()                    override;
        int     pluginVersion()                 override;
        QString waverVersionAPICompatibility()  override;
        QUuid   persistentUniqueId()            override;
        bool    hasUI()                         override;
        void    setUserAgent(QString userAgent) override;

        explicit SFTPSource();
        ~SFTPSource();


    private:

        const int PLAYLIST_DESIRED_SIZE = 15;
        const int PLAYLIST_READY_SIZE   = 10;

        struct PlaylistItem {
            int     clientId;
            QString remotePath;
            QUrl    cachePath;
        };

        enum State {
            Idle,
            SSHLib2Fail,
            DataDirFail,
            Indexing,
            Caching
        };

        QUuid   id;
        State   state;
        QDir    keysDir;
        QDir    cacheDir;
        bool    readySent;
        bool    sendDiagnostics;

        QString variationSetting;
        int     variationSetCountSinceHigh;
        int     variationSetCountSinceLow;

        QVector<SSHClient *> clients;
        QVector<QString>     uiQueue;

        QHash<int, QStringList> audioFiles;
        QVector<PlaylistItem>   futurePlaylist;
        QVector<PlaylistItem>   lovedPlaylist;
        QVector<PlaylistItem>   similarPlaylist;
        QStringList             alreadyPlayed;
        QStringList             banned;
        QStringList             loved;
        QStringList             alreadyPlayedLoved;
        QVector<QUrl>           doNotDelete;
        QVector<QUrl>           playImmediately;

        void       addClient(SSHClient::SSHClientConfig config);
        void       removeAllClients();
        SSHClient *clientFromId(int id);

        TrackInfo trackInfoFromFilePath(QString filePath, int clientId);

        void addToUIQueue(QString UI);
        void removeFromUIQueue();
        void displayNextUIQueue();

        void    appendToPlaylist();
        QString formatTrackForLists(int clientId, QString filePath);

        QJsonDocument configToJson();
        void          jsonToConfig(QJsonDocument jsonDocument);
        QJsonDocument configToJsonGlobal();
        void          jsonToConfigGlobal(QJsonDocument jsonDocument);
        int           variationSettingId();

        void setState(State state);
        void sendDiagnosticsData();


    signals:

        void clientConnect(int id);
        void clientDisconnect(int id);

        void clientPasswordEntryResult(int id, QString fingerprint, QString psw);
        void clientKeySetupQuestionResult(int id, bool answer);
        void clientDirSelectorResult(int id, bool openOnly, QString path);

        void clientFindAudio(int id);
        void clientGetAudio(int id, QStringList remoteFiles);
        void clientGetOpenItems(int id, QString remotePath);


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

        void unableToStart(QUuid uniqueId, QUrl url)                        override;
        void castFinishedEarly(QUuid uniqueId, QUrl url, int playedSeconds) override;
        void done(QUuid uniqueId, QUrl url)                                 override;
        void getPlaylist(QUuid uniqueId, int trackCount, int mode)          override;
        void getReplacement(QUuid uniqueId)                                 override;
        void getOpenTracks(QUuid uniqueId, QString parentId)                override;
        void resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks)  override;

        void search(QUuid uniqueId, QString criteria)                   override;
        void action(QUuid uniqueId, int actionKey, TrackInfo trackInfo) override;


    private slots:

        void delayStartup();

        void clientConnected(int id);
        void clientDisconnected(int id);

        void clientShowPasswordEntry(int id, QString userAtHost, QString fingerprint, QString explanation);
        void clientShowKeySetupQuestion(int id, QString userAtHost);
        void clientShowDirSelector(int id, QString userAtHost, QString currentDir, SSHClient::DirList dirList);
        void clientUpdateConfig(int id);

        void clientAudioList(int id, QStringList files);
        void clientGotAudio(int id, QString remote, QString local);
        void clientGotOpenItems(int id, OpenTracks openTracks);

        void clientError(int id, QString errorMessage);
        void clientInfo(int id, QString infoMessage);
};

#endif // SFTPSOURCE_H
