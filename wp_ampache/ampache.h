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


#ifndef AMPACHE_H
#define AMPACHE_H

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QVariantHash>
#include <QVector>
#include <QXmlStreamAttribute>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>

#include "wp_ampache_global.h"

#include "../waver/pluginfactory.h"
#include "../waver/API/pluginsource_006.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


extern "C" WP_AMPACHE_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);


class WP_AMPACHE_EXPORT Ampache : public PluginSource_006 {
        Q_OBJECT

    public:

        int     pluginType()                    override;
        QString pluginName()                    override;
        int     pluginVersion()                 override;
        QString waverVersionAPICompatibility()  override;
        QUuid   persistentUniqueId()            override;
        bool    hasUI()                         override;
        void    setUserAgent(QString userAgent) override;

        explicit Ampache();
        ~Ampache();


    private:

        static const long SERVER_API_VERSION_MIN              = 400001;
        static const int  PLAYLIST_MODE_VARIATION_FIRST       = 101;
        static const int  PLAYLIST_MODE_LOVED_SIMILAR_RESOLVE = 102;
        static const int  NET_DELAY                           = 50;

        enum State {
            Idle,
            Handshake,
            Playlist,
            Replacement,
            OpeningArtistList,
            OpeningAlbumList,
            OpeningSongList,
            Searching,
            Resolving,
            Flagging
        };

        struct PlaylistRequest {
            int trackCount;
            int mode;
        };

        QUuid   id;
        QString userAgent;
        State   state;

        bool  readySent;
        bool  sendDiagnostics;

        QUrl    serverUrl;
        QString serverUser;
        QString serverPassword;
        long    serverApiVersion;
        QString authKey;

        QVector<PlaylistRequest> playlistRequests;
        QVector<PlaylistRequest> playlistRequestsSent;
        bool                     playlistScheduled;
        TracksInfo               playlistTracksInfo;
        ExtraInfo                playlistExtraInfo;

        QStringList resolveIds;
        bool        resolveScheduled;
        TracksInfo  resolveTracksInfo;

        qint32              lastReturnedAlbumId;
        QHash<QUrl, qint32> urlsToServerId;

        QString variationSetting;
        int     variationCurrent;
        int     variationRemaining;
        qint32  variationAlbum;
        int     variationSetCountSinceHigh;
        int     variationSetCountSinceLow;

        int  variationSettingId();
        void variationSetCurrentRemainingAlbum();

        QNetworkAccessManager *networkAccessManager;

        void handshake();
        void playlisting();
        void resolve();
        void updateFlag(int songId, bool flag);

        QJsonDocument configToJson();
        QJsonDocument configToJsonGlobal();
        void          jsonToConfig(QJsonDocument jsonDocument);
        void          jsonToConfigGlobal(QJsonDocument jsonDocument);

        QNetworkRequest buildRequest(QUrlQuery query);

        void setState(State state);
        void sendDiagnosticsData();


    public slots:

        void run() override;

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration) override;
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

        void networkFinished(QNetworkReply *reply);
        void playlistNext();
        void resolveNext();
};

#endif // AMPACHE_H
