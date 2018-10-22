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


#ifndef WEBSOCKETSERVER_H
#define WEBSOCKETSERVER_H

#include <QObject>
#include <QVector>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QVector>

#ifdef QT_DEBUG
    #include <QDebug>
#endif


class WebSocketServer : public QObject {
        Q_OBJECT

    public:

        explicit WebSocketServer();
        ~WebSocketServer();

        int port();


    private:

        QWebSocketServer      *server;
        QVector<QWebSocket *>  clients;


    public slots:

        void run();
        void rms(int instanceId, qint64 position, int channelIndex, double rms, double peak);
        void position(int instanceId, qint64 position);


    private slots:

        void newConnection();
};

#endif // WEBSOCKETSERVER_H
