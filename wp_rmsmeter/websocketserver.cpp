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


#include "websocketserver.h"


// constructor
WebSocketServer::WebSocketServer() : QObject(NULL)
{
    server = nullptr;
}


// destructor
WebSocketServer::~WebSocketServer()
{
    foreach (QWebSocket *client, clients) {
        client->abort();
        client->deleteLater();
    }

    if (server != nullptr) {
        server->close();
        server->deleteLater();
    }
}


// thread entry point
void WebSocketServer::run()
{
    server = new QWebSocketServer("WaverRMSMeter", QWebSocketServer::NonSecureMode);

    connect(server, SIGNAL(newConnection()), this, SLOT(newConnection()));

    server->listen(QHostAddress::LocalHost);
}


// public method
int WebSocketServer::port()
{
    return server->serverPort();
}


// signal from meter
void WebSocketServer::rms(int instanceId, qint64 position, int channelIndex, double rms, double peak)
{
    char messageType = 'V';

    QByteArray binaryMessage(&messageType, 1);
    binaryMessage.append(reinterpret_cast<char *>(&instanceId), sizeof(int));
    binaryMessage.append(reinterpret_cast<char *>(&position), sizeof(qint64));
    binaryMessage.append(reinterpret_cast<char *>(&channelIndex), sizeof(int));
    binaryMessage.append(reinterpret_cast<char *>(&rms), sizeof(double));
    binaryMessage.append(reinterpret_cast<char *>(&peak), sizeof(double));

    foreach (QWebSocket *client, clients) {
        client->sendBinaryMessage(binaryMessage);
    }
}


// signal from meter
void WebSocketServer::position(int instanceId, qint64 position)
{
    char messageType = 'P';

    QByteArray binaryMessage(&messageType, 1);
    binaryMessage.append(reinterpret_cast<char *>(&instanceId), sizeof(int));
    binaryMessage.append(reinterpret_cast<char *>(&position), sizeof(qint64));

    foreach (QWebSocket *client, clients) {
        client->sendBinaryMessage(binaryMessage);
    }
}


// signal from meter
void WebSocketServer::clean(int instanceId)
{
    char messageType = 'C';

    QByteArray binaryMessage(&messageType, 1);
    binaryMessage.append(reinterpret_cast<char *>(&instanceId), sizeof(int));

    foreach (QWebSocket *client, clients) {
        client->sendBinaryMessage(binaryMessage);
    }
}


// signal from websocket server
void WebSocketServer::newConnection()
{
    clients.append(server->nextPendingConnection());
}
