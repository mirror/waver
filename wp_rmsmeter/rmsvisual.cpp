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

#include "rmsvisual.h"

// constructor
RMSVisual::RMSVisual(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    webSocketPort = -1;
    audioChannel  = -1;

    peakHold     = -99;
    peakHoldTime = 0;

    socketErrorMessage = "";

    webSocketClient = nullptr;

    connect(&timer, SIGNAL(timeout()), this, SLOT(timerTimeout()));

    timer.setInterval(1000 / 24);
    timer.setSingleShot(false);
    timer.setTimerType(Qt::PreciseTimer);
    timer.start();
}


// destructor
RMSVisual::~RMSVisual()
{
    timer.stop();
    webSocketThread.quit();
    webSocketThread.wait();
}


// property get
int  RMSVisual::port() const
{
    return webSocketPort;
}


// property set
void RMSVisual::setPort(const int port)
{
    if (port != webSocketPort) {
        webSocketPort = port;

        emit socketConnect(webSocketPort);

        emit portChanged();
    }
}


// property get
int  RMSVisual::channel() const
{
    return audioChannel;
}


// property set
void RMSVisual::setChannel(const int channel)
{
    if (channel != audioChannel) {
        audioChannel = channel;

        if (webSocketClient == nullptr) {
            webSocketClient = new WebSocketClient(channel);

            webSocketClient->moveToThread(&webSocketThread);

            connect(&webSocketThread, SIGNAL(started()),  webSocketClient, SLOT(run()));
            connect(&webSocketThread, SIGNAL(finished()), webSocketClient, SLOT(deleteLater()));

            connect(this, SIGNAL(socketConnect(int)), webSocketClient, SLOT(socketConnect(int)));

            webSocketThread.start();

            if (webSocketPort != 0) {
                emit socketConnect(webSocketPort);
            }
        }

        emit channelChanged();
    }
}


// paint it
void RMSVisual::paint(QPainter *painter)
{
    if (webSocketClient == nullptr) {
        return;
    }

    QVector<int> tracks = webSocketClient->tracks();
    if (tracks.count() < 1) {
        return;
    }

    painter->setRenderHints(QPainter::Antialiasing, true);
    painter->setPen(QPen(Qt::NoPen));

    double trackHeight = height() / tracks.count();

    double count = 0;
    foreach (int track, tracks) {
        if (peakHoldTime < (QDateTime::currentMSecsSinceEpoch() - 500)) {
            peakHold = -99;
        }

        double peak = webSocketClient->trackPeak(track);

        // TODO peakhold per track
        if (peak > peakHold) {
            peakHold     = peak;
            peakHoldTime = QDateTime::currentMSecsSinceEpoch();
        }

        double rmsRatio      = pow(10.0, webSocketClient->trackRms(track) / 20);
        double peakRatio     = pow(10.0, peak / 20);
        double peakHoldRatio = pow(10.0, peakHold / 20);

        painter->setBrush(QBrush(QColor(0, 0, 0, 127)));
        painter->drawRect(0, trackHeight * count, width() * rmsRatio, trackHeight * (count + 1));

        painter->setBrush(QBrush(QColor(0, 0, 0, 255)));
        painter->drawRect(width() * peakRatio, trackHeight * count, (width() * peakHoldRatio - 3) - width() * peakRatio, trackHeight * (count + 1));
        painter->drawRect(width() * peakHoldRatio, trackHeight * count, width(), trackHeight * (count + 1));

        count++;
    }

    if (tracks.count() > 1) {
        webSocketClient->cleanup();
    }

    if (!socketErrorMessage.isEmpty()) {
        painter->setPen(QPen(QColor(255, 0, 0, 255), 1));
        painter->setFont(QFont("Sans Serif", 8));
        painter->drawText(7, height() - 7, socketErrorMessage);
    }
}


// refresh
void RMSVisual::timerTimeout()
{
    if (webSocketClient != nullptr) {
        update();
    }
}


// signal handler;
void RMSVisual::socketError(QString msg)
{
    socketErrorMessage = msg;
}
