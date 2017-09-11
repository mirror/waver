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

        static const int CACHE_REQUEST_COUNT    = 6;
        static const int PLAYLIST_REQUEST_DELAY = 100;

        struct Genre {
            QString name;
            bool    isPrimary;
        };

        struct SelectedGenre {
            QString name;
            int     limit;
        };

        struct Station {
            QString id;
            QString base;
            QString name;
            QString genre;
            QUrl    url;
        };

        enum State {
            Idle,
            GenreList,
            Caching,
            Opening,
            Searching
        };

        QUuid   id;
        QString userAgent;
        QString key;
        bool    readySent;
        bool    sendDiagnostics;
        State   state;

        QVector<Genre>         genres;
        QDateTime              genresLoaded;
        QVector<SelectedGenre> selectedGenres;
        QStringList            bannedUrls;
        QStringList            unableToStartUrls;

        QVector<Station>                 stationsCache;
        QHash<QString, QVector<Station>> openData;
        QVector<Station>                 stationsToOpen;
        int                              cacheRetries;

        QNetworkAccessManager *networkAccessManager;
        QNetworkAccessManager *playlistAccessManager;
        QNetworkReply         *latestReply;

        QJsonDocument configToJson();
        QJsonDocument configToJsonGlobal();
        void          jsonToConfig(QJsonDocument jsonDocument);
        void          jsonToConfigGlobal(QJsonDocument jsonDocument);

        QString getKey();

        void cache();
        int  findSelectedGenreIndex(QString genreName);
        int  findStationWithoutUrl(QVector<Station> stations);
        int  findStationById(QVector<Station> stations, QString id, bool emptyUrlOnly);
        void sendDiagnosticsData();

        QString getOpenTracksWaitParentId;


    public slots:

        void run() override;

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration) override;
        void loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration) override;

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
        void playlistFinished(QNetworkReply *reply);
        void cacheWait();
        void tuneIn();
        void getOpenTracksWait();
        void resolveOpenTracksWait();

};

#endif // RADIOSOURCE_H
