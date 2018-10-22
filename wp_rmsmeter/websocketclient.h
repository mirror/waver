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


#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <QAbstractSocket>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVector>
#include <QWebSocket>


class WebSocketClient : public QObject {
        Q_OBJECT

    public:

        explicit WebSocketClient(int channel);
        ~WebSocketClient();

        QVector<int> tracks();
        double       trackRms(int trackId);
        double       trackPeak(int trackId);

        void cleanup();


    private:

        struct MeasurePoint {
            qint64 timestamp;
            double rms;
            double peak;
        };
        typedef QVector<MeasurePoint> Measurements;

        int audioChannel;

        QWebSocket *webSocket;
        QMutex     *mutex;

        QHash<int, Measurements *> tracksMeasurements;
        QHash<int, qint64>         tracksLastSeen;


    signals:

        void error(QString msg);


    public slots:

        void run();
        void socketConnect(int port);
        void socketDisconnect();


    private slots:

        void socketError(QAbstractSocket::SocketError error);
        void binaryMessageReceived(const QByteArray &message);
};

#endif // WEBSOCKETCLIENT_H
