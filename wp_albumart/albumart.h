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


#ifndef ALBUMART_H
#define ALBUMART_H

#include "wp_albumart_global.h"

#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegExp>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVector>
#include <QXmlStreamAttribute>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>

#include "../waver/pluginfactory.h"
#include "../waver/API/plugininfo_004.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


extern "C" WP_ALBUMART_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);


class WP_ALBUMART_EXPORT AlbumArt : public PluginInfo_004 {
        Q_OBJECT

    public:

        explicit AlbumArt();
        ~AlbumArt();

        int     pluginType()                    override;
        QString pluginName()                    override;
        int     pluginVersion()                 override;
        QString waverVersionAPICompatibility()  override;
        QUuid   persistentUniqueId()            override;
        bool    hasUI()                         override;
        void    setUrl(QUrl url)                override;
        void    setUserAgent(QString userAgent) override;


    private:

        struct PerformerAlbum {
            QString performer;
            QString album;
        };

        enum State {
            NotStartedYet,
            NotToBeChecked,
            CanNotCheck,
            InAlreadyFailed,
            CheckStarted,
            Success,
            NotFound
        };

        QUuid                    id;
        QString                  userAgent;
        TrackInfo                trackInfo;
        TrackInfo                requestedTrackInfo;
        QVector<PerformerAlbum>  alreadyFailed;
        QNetworkAccessManager   *networkAccessManager;

        // TODO these two should be user configurable + allow user to empty alreadyFailed
        bool checkAlways;
        bool allowLooseMatch;

        bool  sendDiagnostics;
        State state;

        QJsonDocument configToJsonGlobal();
        void          jsonToConfigGlobal(QJsonDocument jsonDocument);
        void          sendDiagnosticsData();
        QString       pictureFileName(TrackInfo pictureTrackInfo);


    public slots:

        void run() override;

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)       override;
        void loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration) override;

        void getUiQml(QUuid uniqueId)                         override;
        void uiResults(QUuid uniqueId, QJsonDocument results) override;

        void startDiagnostics(QUuid uniqueId) override;
        void stopDiagnostics(QUuid uniqueId)  override;

        void getInfo(QUuid uniqueId, TrackInfo trackInfo) override;


    private slots:

        void networkFinished(QNetworkReply *reply);
        void signalTimer();
};

#endif // ALBUMART_H
