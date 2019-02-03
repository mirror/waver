/*
    This file is part of Waver

    Copyright (C) 2018-2019 Peter Papp <peter.papp.p@gmail.com>

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


#ifndef SSHCLIENT_H
#define SSHCLIENT_H

#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QMutex>
#include <QObject>
#include <QRegExp>
#include <QString>
#include <QStringList>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QVector>
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <taglib/fileref.h>
#include <taglib/tstring.h>

#include "../waver/pluginglobals.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


class SSHClient : public QObject {
        Q_OBJECT

    public:

        static const int CHUNK_SIZE    = 65536;
        static const int TIMEOUT_MS    = 5000;
        static const int KEEPALIVE_SEC = 30;

        struct SSHClientConfig {
            int      id;
            QString  host;
            QString  user;
            QString  fingerprint;
            QString  privateKeyFile;
            QString  publicKeyFile;
            QString  dir;
            QString  cacheDir;
            QThread *thread;
        };

        struct DirListItem {
            QString name;
            QString fullPath;
            bool    isDir;
        };
        typedef QVector<DirListItem> DirList;

        enum SSHClientState {
            Idle,
            Connecting,
            Disconnecting,
            CheckingCache,
            ExecutingSSH,
            Downloading,
            Uploading,
            GettingDirList
        };

        explicit SSHClient(SSHClientConfig config, QObject *parent = nullptr);
        ~SSHClient();

        SSHClientConfig getConfig();
        void            setCacheDir(QString cacheDir);
        QString         formatUserHost();

        bool wasConnectionAttempt();

        QString localToRemote(QString local);
        QString remoteToLocal(QString remote);

        SSHClientState getState();
        qint64         getLoadingBytes();

    private:

        static const int  HOSTKEY_HASH_SHA1_LENGTH  = 20;
        static const long UPLOAD_CREATE_PERMISSIONS = LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH;

        static const QString AUTH_METHOD_PUBLICKEY;
        static const QString AUTH_METHOD_PASSWORD;
        static const QString AUTH_METHOD_KB_INTERACTIVE;

        enum QueuedTaskType {
            QueueFindAudio,
            QueueDownload,
            QueueGetOpenItems,
            QueueTrackInfoUpdated
        };
        struct QueuedTask {
            QueuedTaskType type;
            QString        argString;
            TrackInfo      argTrackInfo;
        };

        bool isConnected;
        bool attemptedToConnect;
        void updateIsConnected(bool isConnected);

        QMutex         *mutex;
        SSHClientState  state;
        qint64          loadingBytes;

        QStringList extensions;

        SSHClientConfig config;

        QTcpSocket *socket;

        LIBSSH2_SESSION *session;
        LIBSSH2_SFTP    *sftpSession;

        QString     password;
        QStringList userAuthList;
        QString     homeDir;

        QStringList stdOutSSH;
        QStringList stdErrSSH;

        QVector<QueuedTask> queuedTasks;

        void updateState(SSHClientState state);

        void connectAuthPublicKey();
        void connectAuthPassword(QString fingerprint);
        void connectCreateKeys();
        void connectCheckDir();
        void setHomeDir();

        void findCachedAudio(QString localDir, QStringList *results);

        bool executeSSH(QString command);

        void queueFindAudio(QString subdir);
        void queueDownload(QString remoteFile);
        void queueGetOpenItems(QString remotePath);
        void queueTrackInfoUpdated(TrackInfo trackInfo);

        bool dirList(QString dir, DirList *contents);
        bool download(QString source, QString destination);
        bool upload(QString source, QString destination);

    signals:

        void connected(int id);
        void disconnected(int id);

        void showPasswordEntry(int id, QString userAtHost, QString fingerprint, QString explanation);
        void showKeySetupQuestion(int id, QString userAtHost);
        void showDirSelector(int id, QString userAtHost, QString currentDir, SSHClient::DirList dirList);
        void updateConfig(int id);

        void audioList(int id, QStringList files, bool alreadyCached);
        void gotAudio(int id, QString remote, QString local);
        void gotOpenItems(int id, OpenTracks openTracks);

        void error(int id, QString errorMessage);
        void info(int id, QString infoMessage);
        void stateChanged(int id);


    public slots:

        void run();

        void connectSSH(int id);
        void disconnectSSH(int id);
        void disconnectSSH(int id, QString errorMessage);

        void passwordEntryResult(int id, QString fingerprint, QString psw);
        void keySetupQuestionResult(int id, bool answer);
        void dirSelectorResult(int id, bool openOnly, QString path);

        void findAudio(int id, QString subdir);
        void getAudio(int id, QStringList remoteFiles);
        void getOpenItems(int id, QString remotePath);

        void trackInfoUpdated(TrackInfo trackInfo);


    private slots:

        void socketConnected();
        void socketError(QAbstractSocket::SocketError errorCode);
        void socketStateChanged(QAbstractSocket::SocketState socketState);

        void autoConnect();
        void executeNextInQueue();
};

#endif // SSHCLIENT_H
