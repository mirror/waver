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


#ifndef RADIOSOURCE_H
#define RADIOSOURCE_H

#include "wp_radiosource_global.h"

#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegExp>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVariantHash>
#include <QVector>
#include <QXmlStreamAttribute>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>

#include "../waver/pluginfactory.h"
#include "../waver/API/pluginsource_004.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


extern "C" WP_RADIOSOURCE_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);


class WP_RADIOSOURCE_EXPORT RadioSource : public PluginSource_004 {
        Q_OBJECT

    public:

        int     pluginType()                    override;
        QString pluginName()                    override;
        int     pluginVersion()                 override;
        QString waverVersionAPICompatibility()  override;
        QUuid   persistentUniqueId()            override;
        bool    hasUI()                         override;
        void    setUserAgent(QString userAgent) override;

        explicit RadioSource();
        ~RadioSource();


    private:

        static const int UNABLE_TO_START_EXPIRY_DAYS = 90;
        static const int GENRES_EXPIRY_DAYS          = 180;
        static const int GENRE_EXPIRY_HOURS          = 12;        // SHOUTcast documentation says 24 hours, but there's no starting point for that
        static const int NETWORK_WAIT_MS             = 500;
        static const int PLAYLIST_REQUEST_DELAY_MS   = 100;
        static const int SEARCH_TABLE_LIMIT          = 250000;

        static const bool SQL_TEMPORARY_DB = true;                // using an in-memory temporary database because SHOUTcast IDs do change

        static const int SQL_CREATE_TABLE_STATIONS     = 1;
        static const int SQL_CREATE_TABLE_SEARCH       = 2;
        static const int SQL_GENRE_SEARCH_STATION_LIST = 10;
        static const int SQL_GET_PLAYLIST              = 11;
        static const int SQL_GET_REPLACEMENT           = 12;
        static const int SQL_GENRE_SEARCH_OPENING      = 13;
        static const int SQL_OPEN_GENRE_STATIONS       = 14;
        static const int SQL_OPEN_PLAYLIST             = 15;
        static const int SQL_STATION_SEARCH_OPENING    = 20;
        static const int SQL_STATION_SEARCH_STATIONS   = 21;
        static const int SQL_STATION_SEARCH_PLAYLIST   = 22;
        static const int SQL_STATION_UPDATED_PLAYLIST  = 23;
        static const int SQL_DIAGNOSTICS               = 90;
        static const int SQL_SEARCH_COUNT              = 91;
        static const int SQL_NO_RESULTS                = 99;

        enum State {
            Idle,
            GenreList,
            StationList,
            Opening,
            Searching,
            TuneIn
        };

        enum StationTempDestination {
            Playlist,
            Replacement,
            Open,
            Search
        };

        struct Genre {
            QString name;
            bool    isPrimary;
        };

        struct GenreSearchItem {
            QString genreName;
            State   state;
        };

        struct StationTemp {
            StationTempDestination destination;
            QString                id;
            QString                base;
            QString                name;
            QString                genre;
            QUrl                   url;
            QUrl                   logo;
        };

        struct UnableToStartUrl {
            QUrl   url;
            qint64 timestamp;
        };

        QUuid   id;
        QString userAgent;
        QString key;
        bool    readySent;
        bool    sendDiagnostics;
        State   state;

        void setState(State state);

        QVector<Genre>            genres;
        QDateTime                 genresLoaded;
        QStringList               selectedGenres;
        QVector<QUrl>             bannedUrls;
        QVector<UnableToStartUrl> unableToStartUrls;
        QHash<QString, QDateTime> stationsLoaded;

        QVector<StationTemp> tuneInTemp;

        QVector<GenreSearchItem> genreSearchItems;
        QString                  openGenreSearchGenre;

        QString stationSearchCriteria;

        QNetworkAccessManager *networkAccessManager;
        QNetworkAccessManager *playlistAccessManager;

        QJsonDocument configToJson();
        QJsonDocument configToJsonGlobal();
        void          jsonToConfig(QJsonDocument jsonDocument);
        void          jsonToConfigGlobal(QJsonDocument jsonDocument);

        QString getKey();

        void removeExpiredUnableToStartUrls();
        bool isUnableToStartUrl(QUrl url);

        void sendDiagnosticsData();


    public slots:

        void run() override;

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration) override;
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
        void getPlaylist(QUuid uniqueId, int trackCount)                    override;
        void getReplacement(QUuid uniqueId)                                 override;
        void getOpenTracks(QUuid uniqueId, QString parentId)                override;
        void resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks)  override;

        void search(QUuid uniqueId, QString criteria)        override;
        void action(QUuid uniqueId, int actionKey, QUrl url) override;


    private slots:

        void networkFinished(QNetworkReply *reply);
        void playlistFinished(QNetworkReply *reply);
        void genreSearch();
        void stationSearch();
        void tuneInStarter();
        void tuneIn();

};

#endif // RADIOSOURCE_H
