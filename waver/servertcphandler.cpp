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


#include "servertcphandler.h"


// constructor
ServerTcpHandler::ServerTcpHandler(QObject *parent) : QObject(parent)
{
    // initialization
    tcpServer    = NULL;
    noClientSent = false;
}


// destructor
ServerTcpHandler::~ServerTcpHandler()
{
    // close connections to all clients, and do housekeeping
    mutex.lock();
    foreach (TcpClient tcpClient, tcpClients) {
        tcpClient.tcpSocket->abort();
        tcpClient.tcpSocket->deleteLater();

        delete tcpClient.ipcMessageUtils;
    }
    mutex.unlock();

    // close the server socket and do housekeeping
    if (tcpServer != NULL) {
        tcpServer->close();
        tcpServer->deleteLater();
    }
}


// entry point for dedicated thread
void ServerTcpHandler::run()
{
    // instantiate, set up, and start server socket

    tcpServer = new QTcpServer();

    connect(tcpServer, SIGNAL(acceptError(QAbstractSocket::SocketError)), this, SLOT(acceptError(QAbstractSocket::SocketError)));
    connect(tcpServer, SIGNAL(newConnection()),                           this, SLOT(newConnection()));

    tcpServer->listen(QHostAddress::LocalHost, IpcMessageUtils::tcpPort());
}


// slot to send data to a single client
void ServerTcpHandler::send(TcpClient tcpClient, QString ipcString)
{
    if (tcpClient.tcpSocket->state() == QTcpSocket::ConnectedState) {
        tcpClient.tcpSocket->write(ipcString.toUtf8());
        sendCount++;
    }
}


// slot to send data to all clients
void ServerTcpHandler::send(QString ipcString)
{
    sendCount = 0;

    mutex.lock();
    foreach (TcpClient tcpClient, tcpClients) {
        send(tcpClient, ipcString);
    }
    mutex.unlock();

    if (sendCount == 0) {
        if (!noClientSent) {
            emit noClient();
            noClientSent = true;
        }
    }
    else {
        noClientSent = false;
    }
}


// slot to send multiple data to all clients
void ServerTcpHandler::send(QStringList ipcStrings)
{
    foreach (QString ipcString, ipcStrings) {
        send(ipcString);
    }
}


// server socket signal handler
void ServerTcpHandler::acceptError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);

    // TODO better error handling
    emit error(false, "TCP server error occured");
}


// server socket signal handler
void ServerTcpHandler::newConnection()
{
    TcpClient newClient;

    newClient.tcpSocket       = tcpServer->nextPendingConnection();
    newClient.ipcMessageUtils = new IpcMessageUtils();

    connect(newClient.tcpSocket, SIGNAL(readyRead()), this, SLOT(socketReadyRead()));

    mutex.lock();
    tcpClients.append(newClient);
    mutex.unlock();
}


// client signal handler
void ServerTcpHandler::socketReadyRead()
{
    // get the client that sent this signal
    QTcpSocket *sender = (QTcpSocket *) QObject::sender();

    // find it in our clients
    TcpClient thisClient;
    thisClient.ipcMessageUtils = NULL;
    thisClient.tcpSocket       = NULL;
    int i = 0;
    mutex.lock();
    while ((i < tcpClients.count()) && (thisClient.tcpSocket == NULL)) {
        if (tcpClients.at(i).tcpSocket == sender) {
            thisClient = tcpClients.at(i);
        }
        i++;
    }
    mutex.unlock();
    if (thisClient.tcpSocket == NULL) {
        // this should never happen
        return;
    }

    // read and process data sent by the client
    thisClient.ipcMessageUtils->processIpcString(thisClient.tcpSocket->readAll());

    // process each message one by one
    for (int i = 0; i < thisClient.ipcMessageUtils->processedCount(); i++) {

        // let's see what kind of message we got
        IpcMessageUtils::IpcMessages ipcMessage = thisClient.ipcMessageUtils->processedIpcMessage(i);

        // let's reply if possible without further processing
        if (ipcMessage == IpcMessageUtils::AreYouAlive) {
            send(thisClient, thisClient.ipcMessageUtils->constructIpcString(IpcMessageUtils::ImAlive));
            continue;
        }

        // let others know of the know message if that's what we got
        if (ipcMessage != IpcMessageUtils::Unknown) {
            emit message(ipcMessage, thisClient.ipcMessageUtils->processedIpcData(i));
            continue;
        }

        // not a known message, should be a url (local or remote), let others know or discard if invalid
        QUrl tcpUrl(thisClient.ipcMessageUtils->processedRaw(i));
        if (!tcpUrl.isValid()) {
            continue;
        }
        emit url(tcpUrl);
    }
}
