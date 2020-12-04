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


#ifndef SERVERTCPHANDLER_H
#define SERVERTCPHANDLER_H

#include <QJsonDocument>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QVector>

#include "globals.h"
#include "ipcmessageutils.h"


class ServerTcpHandler : public QObject {
        Q_OBJECT

    public:

        explicit ServerTcpHandler(QObject *parent = 0);
        ~ServerTcpHandler();


    private:

        struct TcpClient {
            QTcpSocket      *tcpSocket;
            IpcMessageUtils *ipcMessageUtils;
        };

        QTcpServer         *tcpServer;
        IpcMessageUtils     serverIpcMessageUtils;
        QVector<TcpClient>  tcpClients;
        QMutex              mutex;
        int                 sendCount;
        bool                noClientSent;


    signals:

        void message(IpcMessageUtils::IpcMessages message, QJsonDocument jsonDocument);
        void url(QUrl url);

        void error(bool fatal, QString error);
        void noClient();


    public slots:

        void run();

        void send(TcpClient tcpClient, QString ipcString);
        void send(QString ipcString);
        void send(QStringList ipcStrings);


    private slots:

        void acceptError(QAbstractSocket::SocketError socketError);
        void newConnection();

        void socketReadyRead();

};

#endif // SERVERTCPHANDLER_H
