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


#ifndef SERVER_H
#define SERVER_H

#include <QtGlobal>

#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QMetaObject>
#include <QMetaMethod>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QThread>
#include <QUrl>
#include <QUuid>
#include <QVariantHash>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include "ipcmessageutils.h"
#include "globals.h"
#include "pluginfactory.h"
#include "pluginglobals.h"
#include "pluginlibsloader.h"
#include "servertcphandler.h"
#include "settingshandler.h"
#include "track.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


class WaverServer : public QObject {
        Q_OBJECT

    public:

        explicit WaverServer(QObject *parent, QStringList arguments);

        TrackInfo     notificationsHelper_Metadata();
        void          notificationsHelper_Next();
        void          notificationsHelper_OpenUri(QString uri);
        void          notificationsHelper_Pause();
        void          notificationsHelper_Play();
        Track::Status notificationsHelper_PlaybackStatus();
        void          notificationsHelper_PlayPause();
        long          notificationsHelper_Position();
        void          notificationsHelper_Quit();
        void          notificationsHelper_Raise();
        void          notificationsHelper_Stop();
        double        notificationsHelper_Volume();


    private:

        static const int  GIVE_UP_TRACKS_COUNT          = 12;
        static const long START_DECODE_PRE_MILLISECONDS = 45 * 1000;

        struct SourcePlugin {
            QString name;
            int     version;
            QString waverVersionAPICompatibility;
            bool    hasUI;
            bool    ready;
            int     priority;
        };
        typedef QHash<QUuid, SourcePlugin> SourcePlugins;

        struct ErrorLogItem {
            QDateTime timestamp;
            QString   title;
            bool      fatal;
            QString   message;
        };

        QStringList arguments;
        QStringList collections;
        QString     currentCollection;

        PluginLibsLoader::LoadedLibs loadedLibs;
        SourcePlugins                sourcePlugins;
        QUuid                        lastPlaylistPlugin;

        QThread serverTcpThread;
        QThread settingsThread;
        QThread pluginLibsLoaderThread;
        QThread sourcesThread;

        Track           *previousTrack;
        Track           *currentTrack;
        QVector<Track *> playlistTracks;

        Track::PluginList plugins;
        Track::PluginList pluginsWithUI;

        QVector<ErrorLogItem> errorLog;

        int  trackCountForLoved;
        int  trackCountForSimilar;
        int  unableToStartCount;
        bool waitingForLocalSource;
        bool waitingForLocalSourceTimerStarted;
        long positionSeconds;
        bool showPreviousTime;
        long previousPositionSeconds;

        bool                        sendErrorLogDiagnostics;
        bool                        diagnosticsChanged;
        QUuid                       lastDiagnosticsPluginId;
        QHash<QUrl, DiagnosticData> diagnosticsAggregator;
        QTimer                      diagnosticsTimer;

        void finish();
        void finish(QString errorMessage);

        QJsonDocument configToJson();
        QJsonDocument configToJsonGlobal();
        void          jsonToConfigGlobal(QJsonDocument jsonDocument);
        void          jsonToConfig(QJsonDocument jsonDocument);

        void outputError(QString errorMessage, QString title, bool fatal);
        void requestPlaylist();
        void startNextTrack();

        void          reassignFadeIns();
        QVariantHash  positionToElapsedRemaining(bool decoderFinished, long knownDurationMilliseconds, long positionMilliseconds);
        QString       findTitleFromUrl(QUrl url);
        Track        *createTrack(TrackInfo trackInfo, QUuid pluginId);
        void          stopAllDiagnostics();

        void handleCollectionMenuChange(QJsonDocument jsonDocument);
        void handleCollectionsDialogResults(QJsonDocument jsonDocument);
        void handlePluginUIRequest(QJsonDocument jsonDocument);
        void handlePluginUIResults(QJsonDocument jsonDocument);
        void handleOpenTracksRequest(QJsonDocument jsonDocument);
        void handleOpenTracksSelection(QJsonDocument jsonDocument);
        void handleSearchRequest(QJsonDocument jsonDocument);
        void handleSearchSelection(QJsonDocument jsonDocument);
        void handleTrackActionsRequest(QJsonDocument jsonDocument);
        void handleDiagnostics(QJsonDocument jsonDocument);
        void handleSourcePrioritiesRequest(QJsonDocument jsonDocument);
        void handleSourcePrioritiesResult(QJsonDocument jsonDocument);
        void sendCollectionListToClients();
        void sendPlaylistToClients(int contextShowTrackIndex);
        void sendPlaylistToClients();
        void sendPluginsToClients();
        void sendPluginsWithUiToClients();
        void sendOpenTracksToClients(IpcMessageUtils::IpcMessages message, QUuid uniqueId, OpenTracks openTracks);
        void sendErrorLogDiagnosticsToClients();


    signals:

        void finished();

        void ipcSend(QString data);

        void saveWaverSettings(QString collectionName, QJsonDocument settings);
        void loadWaverSettings(QString collectionName);
        void savePluginSettings(QUuid uniqueId, QString collectionName, QJsonDocument settings);
        void loadPluginSettings(QUuid uniqueId, QString collectionName);
        void executeSettingsSql(QUuid uniqueId, QString collectionName, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString sql, QVariantList values);
        void startDiagnostics(QUuid uniqueId);
        void stopDiagnostics(QUuid uniqueId);

        void unableToStart(QUuid uniqueId, QUrl url);
        void castFinishedEarly(QUuid uniqueId, QUrl url, int playedSeconds);
        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration);
        void loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration);
        void executedSqlResults(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results);
        void executedGlobalSqlResults(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results);
        void executedSqlError(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error);
        void getPlaylist(QUuid uniqueId, int trackCountForLoved);
        void getPlaylist(QUuid uniqueId, int trackCountForLoved, int mode);
        void getReplacement(QUuid uniqueId);
        void getOpenTracks(QUuid uniqueId, QString parentId);
        void search(QUuid uniqueId, QString criteria);
        void resolveOpenTracks(QUuid uniqueId, QStringList selectedTrackIds);
        void trackAction(QUuid uniqueId, int actionKey, QUrl url);
        void trackAction(QUuid uniqueId, int actionKey, TrackInfo trackInfo);
        void trackTrackAction(QUuid uniqueId, int actionKey, QUrl url);

        void requestPluginUi(QUuid uniqueId);
        void pluginUiResults(QUuid uniqueId, QJsonDocument results);


    public slots:

        void run();


    private slots:

        void pluginLibsLoaded();
        void pluginLibsFailInfo(QString info);

        void notWaitingForLocalSourceAnymore();

        void ipcReceivedMessage(IpcMessageUtils::IpcMessages message, QJsonDocument jsonDocument);
        void ipcReceivedUrl(QUrl url);
        void ipcReceivedError(bool fatal, QString error);
        void ipcNoClient();

        void loadedWaverSettings(QJsonDocument settings);
        void loadedWaverGlobalSettings(QJsonDocument settings);
        void loadedPluginSettings(QUuid uniqueId, QJsonDocument settings);
        void loadedPluginGlobalSettings(QUuid uniqueId, QJsonDocument settings);
        void executedPluginSqlResults(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results);
        void executedPluginGlobalSqlResults(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results);
        void executedPluginSqlError(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error);

        void pluginUi(QUuid uniqueId, QString qml, QString header);
        void pluginUi(QUuid uniqueId, QString qml);

        void pluginReady(QUuid uniqueId);
        void pluginUnready(QUuid uniqueId);
        void pluginInfoMessage(QUuid uniqueId, QString message);
        void pluginUpdateTrackInfo(QUuid uniqueId, TrackInfo trackInfo);
        void pluginDiagnostics(QUuid uniqueId, DiagnosticData data);
        void pluginDiagnostics(QUuid uniqueId, QUrl url, DiagnosticData data);

        void loadConfiguration(QUuid uniqueId);
        void loadGlobalConfiguration(QUuid uniqueId);
        void saveConfiguration(QUuid uniqueId, QJsonDocument configuration);
        void saveGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration);
        void executeSql(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString sql, QVariantList values);
        void executeGlobalSql(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString sql, QVariantList values);

        void playlist(QUuid uniqueId, TracksInfo tracksInfo);
        void replacement(QUuid uniqueId, TrackInfo trackInfo);
        void openTracksResults(QUuid uniqueId, OpenTracks openTracks);
        void searchResults(QUuid uniqueId, OpenTracks openTracks);
        void requestedRemoveTracks(QUuid uniqueId);
        void requestedRemoveTrack(QUuid uniqueId, QUrl url);
        void sourceOpenUrl(QUrl urlToOpen);

        void trackError(QUrl url, bool fatal, QString errorString);
        void trackLoadedPlugins(Track::PluginList pluginsWithUI);
        void trackLoadedPluginsWithUI(Track::PluginList pluginsWithUI);
        void trackRequestFadeInForNextTrack(QUrl url, qint64 lengthMilliseconds);
        void trackRequestAboutToFinishSendForPreviousTrack(QUrl url, qint64 posBeforeEndMilliseconds);
        void trackPosition(QUrl url, bool decoderFinished, long knownDurationMilliseconds, long positionMilliseconds);
        void trackAboutToFinish(QUrl url);
        void trackFinished(QUrl url);
        void trackInfoUpdated(QUrl url);

        void startNextTrackUISignal();
        void diagnosticsRefreshUI();
};


#endif // SERVER_H
