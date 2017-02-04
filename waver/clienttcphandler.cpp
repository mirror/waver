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


#include "clienttcphandler.h"

// constructor
ClientTcpHandler::ClientTcpHandler(QObject *parent) : QObject(parent)
{
    // initialization
    tcpSocket = NULL;
}


// destructor
ClientTcpHandler::~ClientTcpHandler()
{
    if (tcpSocket != NULL) {
        close();
        tcpSocket->deleteLater();
    }
}


// public method
bool ClientTcpHandler::isOpen()
{
    if (tcpSocket == NULL) {
        return false;
    }

    if (tcpSocket->state() == QTcpSocket::UnconnectedState) {
        return false;
    }

    // return true even if not connected but connecting now
    return true;
}


// entry point for dedicated thread
void ClientTcpHandler::run()
{
    // instantiate and set up socket

    tcpSocket = new QTcpSocket();

    connect(tcpSocket, SIGNAL(connected()),                         this, SLOT(socketConnected()));
    connect(tcpSocket, SIGNAL(disconnected()),                      this, SLOT(socketDisconnected()));
    connect(tcpSocket, SIGNAL(readyRead()),                         this, SLOT(socketReadyRead()));
    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socketError(QAbstractSocket::SocketError)));

    // connect
    open();
}


// slot to connect to the server
void ClientTcpHandler::open()
{
    if (tcpSocket == NULL) {
        return;
    }

    if (tcpSocket->state() != QTcpSocket::UnconnectedState) {
        return;
    }

    tcpSocket->connectToHost(QHostAddress::LocalHost, IpcMessageUtils::tcpPort());

    // unfortunately QTcpSocket's timeout can not be set
    QTimer::singleShot(TCP_TIMEOUT, this, SLOT(connectTimerTimeout()));
}


// slot to disconnect from server
void ClientTcpHandler::close()
{
    if (tcpSocket == NULL) {
        return;
    }

    if (tcpSocket->state() == QTcpSocket::UnconnectedState) {
        return;
    }

    tcpSocket->flush();
    tcpSocket->close();
}


// slot to send data to the server
void ClientTcpHandler::send(QString ipcString)
{
    if (tcpSocket == NULL) {
        return;
    }

    if (tcpSocket->state() == QTcpSocket::ConnectedState) {
        tcpSocket->write(ipcString.toUtf8());
    }
}


// slot to send multiple data to the server
void ClientTcpHandler::send(QStringList ipcStrings)
{
    if (tcpSocket == NULL) {
        return;
    }

    foreach (QString ipcString, ipcStrings) {
        send(ipcString);
    }
}


// slot to send data to the server
void ClientTcpHandler::send(IpcMessageUtils::IpcMessages ipcMessage)
{
    send(ipcMessageUtils.constructIpcString(ipcMessage));
}


// slot to send data to the server
void ClientTcpHandler::send(IpcMessageUtils::IpcMessages ipcMessage, QJsonDocument ipcJsonData)
{
    send(ipcMessageUtils.constructIpcString(ipcMessage, ipcJsonData));
}


// timer signal handler
void ClientTcpHandler::connectTimerTimeout()
{
    if (tcpSocket->state() != QTcpSocket::ConnectedState) {
        socketError(QAbstractSocket::SocketTimeoutError);
    }
}


// socket signal handler
void ClientTcpHandler::socketConnected()
{
    emit opened();
}


// socket signal handler
void ClientTcpHandler::socketDisconnected()
{
    emit closed();
}


// socket signal handler
void ClientTcpHandler::socketReadyRead()
{
    // read and process data sent
    ipcMessageUtils.processIpcString(tcpSocket->readAll());

    // process each message one by one
    for (int i = 0; i < ipcMessageUtils.processedCount(); i++) {
        // try to get a known message from string data
        IpcMessageUtils::IpcMessages ipcMessage = ipcMessageUtils.processedIpcMessage(i);

        // let others know of the know message if it's a valid
        if (ipcMessage != IpcMessageUtils::Unknown) {
            emit message(ipcMessage, ipcMessageUtils.processedIpcData(i));
        }
    }
}


// socket signal handler
void ClientTcpHandler::socketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    // TODO better error handling
    emit error(false, QString("TCP socket error occured\n%1").arg(tcpSocket->errorString()));
}
