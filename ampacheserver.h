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
#include <QMultiHash>
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
            SearchAlbums,
            BrowseRoot,
            BrowseArtist,
            BrowseAlbum,
            CountFlagged,
            PlaylistRoot,
            PlaylistSongs,
            RadioStations,
            SetFlag,
            Shuffle,
            Artist,
            Song,
            Tags,
            Unknown
        };

        typedef QHash<QString, QString> OpData;
        typedef QList<OpData>           OpResults;

        explicit AmpacheServer(QUrl host, QString user, QObject *parent = nullptr);

        long    getServerVersion();
        QUrl    getHost();
        QString getUser();

        QString formattedName();

        void    setId(QString id);
        void    setSettingsId(QUuid settingsId);
        QString getId();
        QUuid   getSettingsId();

        void setShuffleTags(QStringList selectedTags);
        bool isShuffleTagsSelected();
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

        static const long SERVER_API_VERSION_MIN = 5000000;


        QUrl    host;
        QString psw;
        QString user;
        QString id;
        QUuid   settingsId;

        bool    handshakeInProgress;
        bool    sessionExpiredHandshakeStarted;
        QString authKey;
        long    serverVersion;
        int     songCount;
        int     flaggedCount;

        int       shuffled;
        OpResults shuffleRegular;
        OpResults shuffleFavorites;
        OpResults shuffleRecentlyAdded;
        bool      shuffleRegularCompleted;
        bool      shuffleFavoritesCompleted;
        bool      shuffleRecentlyAddedCompleted;

        QList<Operation> opQueue;
        QList<int>       shuffleTags;

        WritePasswordJob *writeKeychainJob;
        ReadPasswordJob  *readKeychainJob;

        QNetworkAccessManager networkAccessManager;


        QNetworkRequest buildRequest(QUrlQuery query, QObject *extra);
        long            apiVersionFromString(QString apiVersionString);
        QStringList     shuffleTagsToStringList();
        QString         tagIdToString(int tagId);
        QObject        *copyExtra(QObject *extra);


    private slots:

        void writeKeychainJobFinished(QKeychain::Job* job);
        void readKeychainJobFinished(QKeychain::Job* job);

        void startHandshake();
        void startOperations();

        void networkFinished(QNetworkReply *reply);


    signals:

        void operationFinished(OpCode opCode, OpData opData, OpResults opResults);
        void errorMessage(QString id, QString info, QString error);
        void passwordNeeded(QString id);
};


#endif // AMPACHESERVER_H
