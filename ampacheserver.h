/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef AMPACHESERVER_H
#define AMPACHESERVER_H

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QGuiApplication>
#include <QHash>
#include <QLatin1String>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QXmlStreamAttribute>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>

#ifdef QT_DEBUG
    #include <QDebug>
#endif

#include "globals.h"
#include "qt5keychain/keychain.h"


using namespace QKeychain;


class AmpacheServer : public QObject
{
    Q_OBJECT


    public:

        enum OpCode {
            Search,
            BrowseRoot,
            BrowseArtist,
            BrowseAlbum,
            CountFlagged,
            PlaylistRoot,
            PlaylistSongs,
            RadioStations,
            SetFlag,
            Shuffle,
            Song,
            Tags,
            Unknown
        };

        typedef QHash<QString, QString> OpData;
        typedef QList<OpData>           OpResults;

        explicit AmpacheServer(QUrl host, QString user, QObject *parent = nullptr);

        QUrl    getHost();
        QString getUser();

        QString formattedName();

        void    setId(QString id);
        void    setSettingsId(QUuid settingsId);
        QString getId();
        QUuid   getSettingsId();

        void setShuffleTag(int tagId, bool selected);
        bool isShuffleTagSelected(int tagId);

        void setPassword(QString psw);
        void retreivePassword();

        void startOperation(OpCode opCode, OpData opData, QObject *extra = nullptr);


    private:

        struct Operation {
            OpCode   opCode;
            OpData   opData;
            QObject *extra;
        };

        static const long SERVER_API_VERSION_MIN = 400001;


        QUrl    host;
        QString psw;
        QString user;
        QString id;
        QUuid   settingsId;

        bool    handshakeInProgress;
        QString authKey;
        long    serverVersion;
        int     songCount;
        int     flaggedCount;

        int       shuffled;
        OpResults shuffleRegular;
        OpResults shuffleFavorites;
        bool      shuffleRegularCompleted;
        bool      shuffleFavoritesCompleted;

        QList<Operation> opQueue;
        QList<int>       shuffleTags;

        WritePasswordJob *writeKeychainJob;
        ReadPasswordJob  *readKeychainJob;

        QNetworkAccessManager networkAccessManager;


        QNetworkRequest buildRequest(QUrlQuery query, QObject *extra);
        long            apiVersionFromString(QString apiVersionString);
        QStringList     shuffleTagsToStringList();


    private slots:

        void writeKeychainJobFinished(QKeychain::Job* job);
        void readKeychainJobFinished(QKeychain::Job* job);

        void startHandshake();
        void startOperations();

        void networkFinished(QNetworkReply *reply);


    signals:

        void operationFinished(OpCode opCode, OpData opData, OpResults opResults);
        void errorMessage(QString id, QString info, QString error);
};


#endif // AMPACHESERVER_H
