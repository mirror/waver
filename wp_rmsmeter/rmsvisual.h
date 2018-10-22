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

#ifndef RMSVISUAL_H
#define RMSVISUAL_H

#include <QByteArray>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <QQuickPaintedItem>
#include <QThread>
#include <QTimer>
#include <math.h>

#include "websocketclient.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif

class RMSVisual : public QQuickPaintedItem {
        Q_OBJECT

        Q_PROPERTY(int port    READ port    WRITE setPort    NOTIFY portChanged)
        Q_PROPERTY(int channel READ channel WRITE setChannel NOTIFY channelChanged)

    public:

        RMSVisual(QQuickItem *parent = nullptr);
        ~RMSVisual();

        int  port() const;
        void setPort(const int port);

        int  channel() const;
        void setChannel(const int channel);

        void paint(QPainter *painter);


    private:

        int webSocketPort;
        int audioChannel;

        QString socketErrorMessage;

        WebSocketClient *webSocketClient;
        QThread          webSocketThread;

        QTimer timer;


    signals:

        void portChanged();
        void channelChanged();

        void socketConnect(int port);


    private slots:

        void timerTimeout();
        void socketError(QString msg);
};

#endif // RMSVISUAL_H
