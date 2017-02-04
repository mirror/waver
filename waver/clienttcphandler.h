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


#ifndef CLIENTTCPHANDLER_H
#define CLIENTTCPHANDLER_H

#include <QByteArray>
#include <QHostAddress>
#include <QJsonDocument>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

#include "globals.h"
#include "ipcmessageutils.h"

class ClientTcpHandler : public QObject
{
    Q_OBJECT

public:

    explicit ClientTcpHandler(QObject *parent = 0);
    ~ClientTcpHandler();

    bool isOpen();

private:

    IpcMessageUtils ipcMessageUtils;

    QTcpSocket *tcpSocket;


signals:

    void opened();
    void closed();
    void message(IpcMessageUtils::IpcMessages message, QJsonDocument jsonDocument);
    void error(bool fatal, QString error);


public slots:

    void run();

    void open();
    void close();

    void send(QString ipcString);
    void send(QStringList ipcStrings);
    void send(IpcMessageUtils::IpcMessages ipcMessage);
    void send(IpcMessageUtils::IpcMessages ipcMessage, QJsonDocument ipcJsonData);


private slots:

    void connectTimerTimeout();

    void socketConnected();
    void socketDisconnected();
    void socketReadyRead();
    void socketError(QAbstractSocket::SocketError socketError);

};

#endif // CLIENTTCPHANDLER_H
