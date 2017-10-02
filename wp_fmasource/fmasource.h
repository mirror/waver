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
#include "../waver/API/pluginsource_004.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


extern "C" WP_FMASOURCE_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);


class WP_FMASOURCE_EXPORT FMASource : public PluginSource_004 {
        Q_OBJECT

    public:

        int     pluginType()                    override;
        QString pluginName()                    override;
        int     pluginVersion()                 override;
        QString waverVersionAPICompatibility()  override;
        QUuid   persistentUniqueId()            override;
        bool    hasUI()                         override;
        void    setUserAgent(QString userAgent) override;

        explicit FMASource();
        ~FMASource();


    private:

        static const int GENRES_EXPIRY_DAYS = 30;
        static const int NETWORK_WAIT_MS    = 500;

        static const bool SQL_TEMPORARY_DB = false;

        static const int SQL_CREATETABLE_ALBUMS           = 1;
        static const int SQL_CREATETABLE_TRACKS           = 2;
        static const int SQL_LOADMORE_ALBUMGENRES         = 10;
        static const int SQL_LOADMORE_ALBUMSLOADED        = 11;
        static const int SQL_LOADMORE_ALBUMSWITHOUTTRACKS = 12;
        static const int SQL_LOADMORE_TRACKSLOADED        = 13;
        static const int SQL_GET_PLAYLIST                 = 20;
        static const int SQL_NO_RESULTS                   = 99;


        enum ParseElement {
            Unknown,
            Page, TotalPages,
            GenreId, GenreHandle, GenreParentId, GenreTitle,
            AlbumId, AlbumTitle, AlbumDateReleased, ArtistName, AlbumImages,
            TrackId, TrackTitle, TrackUrl, TrackImageFile, TrackNumber, TrackGenres
        };

        enum State {
            Idle,
            GenreList,
            AlbumList,
            TrackList,
            Opening,
            Searching
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
            int   genreId;
            State state;
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
            QString url;
            QString pictureUrl;
            int     track;
        };

        QUuid   id;
        QString userAgent;
        QString key;
        bool    readySent;
        bool    sendDiagnostics;
        State   state;

        void setState(State state);

        QVector<Genre> genres;
        QDateTime      genresLoaded;
        QVector<int>   selectedGenres;

        QVector<GenreSearchItem> genreSearchItems;
        QVector<AlbumSearchItem> albumSearchItems;

        QNetworkAccessManager *networkAccessManager;

        QJsonDocument configToJson();
        QJsonDocument configToJsonGlobal();
        void          jsonToConfig(QJsonDocument jsonDocument);
        void          jsonToConfigGlobal(QJsonDocument jsonDocument);

        QString getKey();

        bool stringToInt(QString str, int *num);
        void sortGenres(int parentId, QVector<GenreDisplay> *sorted, int level);
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

        void unableToStart(QUuid uniqueId, QUrl url)                       override;
        void getPlaylist(QUuid uniqueId, int maxCount)                     override;
        void getOpenTracks(QUuid uniqueId, QString parentId)               override;
        void resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks) override;

        void search(QUuid uniqueId, QString criteria)        override;
        void action(QUuid uniqueId, int actionKey, QUrl url) override;


    private slots:

        void networkFinished(QNetworkReply *reply);
        void genreSearch();
        void albumSearch();
};

#endif // FMASOURCE_H
