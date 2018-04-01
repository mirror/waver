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


#ifndef FMASOURCE_H
#define FMASOURCE_H

#include "wp_fmasource_global.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegExp>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVariant>
#include <QVariantList>
#include <QVector>
#include <QXmlStreamAttribute>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>

#include "../waver/pluginfactory.h"
#include "../waver/pluginglobals.h"
#include "../waver/API/pluginsource_005.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


extern "C" WP_FMASOURCE_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);


class WP_FMASOURCE_EXPORT FMASource : public PluginSource_005 {
        Q_OBJECT

    public:

        int     pluginType()                    override;
        QString pluginName()                    override;
        int     pluginVersion()                 override;
        QString waverVersionAPICompatibility()  override;
        QUuid   persistentUniqueId()            override;
        bool    hasUI()                         override;
        void    setUserAgent(QString userAgent) override;
        QUrl    menuImageURL()                  override;

        explicit FMASource();
        ~FMASource();


    private:

        static const int NETWORK_WAIT_MS    = 500;

        static const int SQL_CREATETABLE_GENRES           = 1;
        static const int SQL_CREATETABLE_ALBUMS           = 2;
        static const int SQL_CREATETABLE_TRACKS           = 3;
        static const int SQL_CREATETABLE_BANNED           = 4;
        static const int SQL_CREATETABLE_LOVED            = 5;
        static const int SQL_STARTUPCHECK_GENRECOUNT      = 10;
        static const int SQL_STARTUPCHECK_GENRESLOADED    = 11;
        static const int SQL_COLLECTIONCHECK_GENRE        = 12;
        static const int SQL_LOADMORE_GENRESWITHOUTALBUM  = 20;
        static const int SQL_LOADMORE_ALBUMSLOADED        = 21;
        static const int SQL_LOADMORE_ALBUMSWITHOUTTRACKS = 22;
        static const int SQL_LOADMORE_TRACKSLOADED        = 23;
        static const int SQL_GET_PLAYLIST                 = 30;
        static const int SQL_GET_REPLACEMENT              = 31;
        static const int SQL_GET_LOVED                    = 32;
        static const int SQL_OPEN_TOPLEVEL                = 33;
        static const int SQL_OPEN_PERFORMERS              = 34;
        static const int SQL_OPEN_PERFORMERS_GENRESEARCH  = 35;
        static const int SQL_OPEN_PERFORMERS_LOADED       = 36;
        static const int SQL_OPEN_ALBUMS                  = 37;
        static const int SQL_OPEN_TRACKS                  = 38;
        static const int SQL_OPEN_TRACKS_LOADED           = 39;
        static const int SQL_SEARCH                       = 40;
        static const int SQL_UIQML_GENRELIST              = 41;
        static const int SQL_TO_BE_REMOVED                = 50;
        static const int SQL_DIAGNOSTICS                  = 90;
        static const int SQL_NO_RESULTS                   = 99;


        enum ParseElement {
            Unknown,
            Page, TotalPages,
            GenreId, GenreHandle, GenreParentId, GenreTitle,
            AlbumId, AlbumTitle, AlbumDateReleased, ArtistName, AlbumImages,
            TrackId, TrackTitle, TrackArtistName, TrackUrl, TrackImageFile, TrackNumber, TrackGenres
        };

        enum State {
            Idle,
            GenreList,
            AlbumList,
            TrackList,
            OpeningPerformersAlbums,
            OpeningTracks
        };

        struct Genre {
            int     id;
            int     parentId;
            QString handle;
            QString name;
        };

        struct GenreDisplay {
            int     id;
            QString name;
            bool    isTopLevel;
            int     indent;
        };

        struct GenreSearchItem {
            int     genreId;
            QString genreHandle;
            State   state;
        };

        struct Album {
            int     id;
            int     genreId;
            QString album;
            QString performer;
            int     year;
        };

        struct AlbumSearchItem {
            int   albumId;
            State state;
        };

        struct Track {
            int     id;
            int     albumId;
            QString title;
            QString performer;
            QString url;
            QString pictureUrl;
            int     track;
        };

        QUuid              id;
        QString            userAgent;
        QString            key;
        bool               readySent;
        bool               sendDiagnostics;
        State              state;
        int                openingId;
        QString            searchCriteria;

        TrackInfo lovedTemp;
        int       lovedMode;
        int       lovedLeft;

        void setState(State state);

        QVector<int> selectedGenres;

        QVector<GenreSearchItem> genreSearchItems;
        QVector<AlbumSearchItem> albumSearchItems;

        QNetworkAccessManager *networkAccessManager;

        QJsonDocument configToJson();
        void          jsonToConfig(QJsonDocument jsonDocument);

        QString getKey();

        void loadMore();

        bool stringToInt(QString str, int *num);
        void sortGenres(QVector<Genre> genres, int parentId, QVector<GenreDisplay> *sorted, int level);
        void selectedGenresBinds(QString *binds, QVariantList *values);

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

        void unableToStart(QUuid uniqueId, QUrl url)                        override;
        void castFinishedEarly(QUuid uniqueId, QUrl url, int playedSeconds) override;
        void getPlaylist(QUuid uniqueId, int trackCount, int mode)          override;
        void getReplacement(QUuid uniqueId)                                 override;
        void getOpenTracks(QUuid uniqueId, QString parentId)                override;
        void resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks)  override;

        void search(QUuid uniqueId, QString criteria)                   override;
        void action(QUuid uniqueId, int actionKey, TrackInfo trackInfo) override;


    private slots:

        void networkFinished(QNetworkReply *reply);
        void genreSearch();
        void albumSearch();
};

#endif // FMASOURCE_H
