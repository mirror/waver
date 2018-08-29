/*
    This file is part of Waver

    Copyright (C) 2018 Peter Papp <peter.papp.p@gmail.com>

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
#include <QFileInfo>
#include <QFileInfoList>
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

#include "../waver/pluginglobals.h"

class SSHClient : public QObject {
        Q_OBJECT

    public:

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

        explicit SSHClient(SSHClientConfig config, QObject *parent = nullptr);
        ~SSHClient();

        SSHClientConfig getConfig();
        QString         formatUserHost();
        bool            isConnected();

        QString localToRemote(QString local);
        QString remoteToLocal(QString remote);

    private:

        static const int  HOSTKEY_HASH_SHA1_LENGTH  = 20;
        static const long UPLOAD_CREATE_PERMISSIONS = LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH;

        static const QString AUTH_METHOD_PUBLICKEY;
        static const QString AUTH_METHOD_PASSWORD;
        static const QString AUTH_METHOD_KB_INTERACTIVE;

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

        QStringList downloadList;

        void connectAuthPublicKey();
        void connectAuthPassword(QString fingerprint);
        void connectCreateKeys();
        void connectCheckDir();
        void setHomeDir();

        void findCachedAudio(QString localDir, QStringList *results);

        bool    executeSSH(QString command);
        QString getErrorMessageSSH();

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


    public slots:

        void run();

        void connectSSH(int id);
        void disconnectSSH(int id);
        void disconnectSSH(int id, QString errorMessage);

        void passwordEntryResult(int id, QString fingerprint, QString psw);
        void keySetupQuestionResult(int id, bool answer);
        void dirSelectorResult(int id, bool openOnly, QString path);

        void findAudio(int id);
        void getAudio(int id, QStringList remoteFiles);
        void getOpenItems(int id, QString remotePath);

        void trackInfoUpdated(TrackInfo trackInfo);


    private slots:

        void socketConnected();
        void socketError(QAbstractSocket::SocketError errorCode);
        void socketStateChanged(QAbstractSocket::SocketState socketState);

        void autoConnect();
        void dowloadNext();
};

#endif // SSHCLIENT_H
