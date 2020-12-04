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


#ifndef WP_ACOUSTID_H
#define WP_ACOUSTID_H

#include "wp_acoustid_global.h"

#include "cmath"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QThread>
#include <QVector>
#include <QXmlStreamAttribute>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>

#include "../waver/API/plugininfo_006.h"

class WP_ACOUSTID_EXPORT Acoustid : PluginInfo_006 {
        Q_OBJECT


    public:

        static const int ALREADY_FAILED_EXPIRY_DAYS = 30;

        explicit Acoustid();
        ~Acoustid();

        int     pluginType()                    override;
        QString pluginName()                    override;
        int     pluginVersion()                 override;
        QString waverVersionAPICompatibility()  override;
        QUuid   persistentUniqueId()            override;
        bool    hasUI()                         override;
        void    setUrl(QUrl url)                override;
        void    setUserAgent(QString userAgent) override;


    private:

        struct Recording {
            double  score;
            QString id;
        };

        struct Result {
            TrackInfo trackInfo;
            qint64    duration;
            qint64    durationDifference;
            double    score;
        };

        struct Failed {
            QUrl   url;
            qint64 timestamp;
        };

        enum State {
            NotStartedYet,
            Cast,
            WaitingForChromaprint,
            NotCheckingAutomatically,
            InAlreadyFailed,
            CheckStarted,
            Success,
            NotFound
        };

        QUuid   id;
        QString userAgent;
        bool    sendDiagnostics;
        State   state;

        TrackInfo    trackInfo;
        QVariantHash additionalInfo;
        qint64       duration;
        QString      chromaprint;

        QVector<Failed> alreadyFailed;
        QDateTime       nextCheck;

        QVector<Recording> recordings;
        QVector<Result>    results;

        QNetworkAccessManager *networkAccessManager;

        QString getKey();

        bool isReadyToQuery();
        void query();

        QJsonDocument configToJsonGlobal();
        void          jsonToConfigGlobal(QJsonDocument jsonDocument);
        void          addToAlreadyFailed();

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

        void getInfo(QUuid uniqueId, TrackInfo trackInfo, QVariantHash additionalInfo) override;
        void action(QUuid uniqueId, int actionKey, TrackInfo trackInfo)                override;


    private slots:

        void networkFinished(QNetworkReply *reply);
};

#endif // WP_ACOUSTID_H
