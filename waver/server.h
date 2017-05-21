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

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLibrary>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QThread>
#include <QUrl>
#include <QUuid>
#include <QVariantHash>
#include <QVector>

#include "ipcmessageutils.h"
#include "globals.h"
#include "pluginlibsloader.h"
#include "pluginbase.h"
#include "pluginsource.h"
#include "servertcphandler.h"
#include "settingshandler.h"
#include "track.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif

class WaverServer : public QObject
{
    Q_OBJECT

public:

    explicit WaverServer(QObject *parent, QStringList arguments);


private:

    static const int  MAX_TRACKS_AT_ONCE            = 4;
    static const long CAST_PLAYTIME_MILLISECONDS    = 450 * 1000;
    static const long START_DECODE_PRE_MILLISECONDS = 45 * 1000;

    struct SourcePlugin {
        QString name;
        int     version;
        int     baseVersion;
        int     pluginTypeVersion;
        bool    hasUI;
        bool    ready;
    };
    typedef QHash<QUuid, SourcePlugin> SourcePlugins;

    QStringList arguments;
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
    QVector<Track*>  playlistTracks;

    Track::PluginsWithUI pluginsWithUI;

    int  unableToStartCount;
    bool waitingForLocalSource;
    bool waitingForLocalSourceTimerStarted;
    long positionSeconds;

    void finish();

    void requestPlaylist();
    void startNextTrack();

    void handleCollectionsDialogResults(QJsonDocument jsonDocument);
    void handlePluginUIRequest(QJsonDocument jsonDocument);
    void handlePluginUIResults(QJsonDocument jsonDocument);
    void handleOpenTracksRequest(QJsonDocument jsonDocument);
    void handleOpenTracksSelection(QJsonDocument jsonDocument);
    void handleSearchRequest(QJsonDocument jsonDocument);
    void handleSearchSelection(QJsonDocument jsonDocument);

    void sendPlaylistToClients();
    void sendPluginsWithUiToClients();
    void sendOpenTracksToClients(IpcMessageUtils::IpcMessages message, QUuid uniqueId, PluginSource::OpenTracks openTracks);


signals:

    void finished();

    void ipcSend(QString data);

    void saveCollectionList(QStringList collections, QString currentCollection);
    void saveCollectionList(QString currentCollection);
    void getCollectionList();
    void savePluginSettings(QUuid uniqueId, QString collectionName, QJsonDocument settings);
    void loadPluginSettings(QUuid uniqueId, QString collectionName);

    void unableToStart(QUuid uniqueId, QUrl url);
    void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration);
    void getPlaylist(QUuid uniqueId, int maxCount);
    void getOpenTracks(QUuid uniqueId, QString parentId);
    void search(QUuid uniqueId, QString criteria);
    void resolveOpenTracks(QUuid uniqueId, QStringList selectedTrackIds);

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

    void collectionList(QStringList collections, QString currentCollection);
    void loadedPluginSettings(QUuid uniqueId, QJsonDocument settings);

    void pluginUi(QUuid uniqueId, QString qml);

    void pluginReady(QUuid uniqueId);
    void pluginUnready(QUuid uniqueId);
    void pluginInfoMessage(QUuid uniqueId, QString message);
    void loadConfiguration(QUuid uniqueId);
    void saveConfiguration(QUuid uniqueId, QJsonDocument configuration);
    void playlist(QUuid uniqueId, PluginSource::TracksInfo tracksInfo);
    void openTracksResults(QUuid uniqueId, PluginSource::OpenTracks openTracks);
    void searchResults(QUuid uniqueId, PluginSource::OpenTracks openTracks);
    void requestedRemoveTracks(QUuid uniqueId);

    void trackError(QUrl url, bool fatal, QString errorString);
    void trackLoadedPluginsWithUI(Track::PluginsWithUI pluginsWithUI);
    void trackRequestFadeInForNextTrack(QUrl url, qint64 lengthMilliseconds);
    void trackPosition(QUrl url, bool cast, bool decoderFinished, long knownDurationMilliseconds, long positionMilliseconds);
    void trackAboutToFinish(QUrl url);
    void trackFinished(QUrl url);
    void trackInfoUpdated(QUrl url);

};


#endif // SERVER_H
