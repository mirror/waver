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


#include "websocketclient.h"

// constructor
WebSocketClient::WebSocketClient(int channel)
{
    audioChannel = channel;

    webSocket = nullptr;
    mutex     = nullptr;
}


// destructor
WebSocketClient::~WebSocketClient()
{
    if (webSocket != nullptr) {
        webSocket->close();
        webSocket->deleteLater();
    }
    if (mutex != nullptr) {
        delete mutex;
    }
    foreach (int trackId, tracksMeasurements.keys()) {
        delete tracksMeasurements.value(trackId);
    }
}


// thread entry point
void WebSocketClient::run()
{
    webSocket = new QWebSocket();
    mutex     = new QMutex();

    connect(webSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socketError(QAbstractSocket::SocketError)));
    connect(webSocket, SIGNAL(binaryMessageReceived(QByteArray)),   this, SLOT(binaryMessageReceived(QByteArray)));
}


// signal from QML component
void WebSocketClient::socketConnect(int port)
{
    if (webSocket->state() != QAbstractSocket::UnconnectedState) {
        webSocket->close();
    }

    webSocket->open(QUrl(QString("ws://127.0.0.1:%1").arg(port)));
}


// signal from QML component
void WebSocketClient::socketDisconnect()
{
    webSocket->close();
}


// return number of tracks
QVector<int> WebSocketClient::tracks()
{
    if (mutex == nullptr) {
        return QVector<int>();
    }

    mutex->lock();
    QVector<int> returnValue = QVector<int>::fromList(tracksMeasurements.keys());
    mutex->unlock();

    qSort(returnValue.begin(), returnValue.end());

    return returnValue;
}


// return rms value
double WebSocketClient::trackRms(int trackId)
{
    if (!tracksMeasurements.contains(trackId) || (mutex == nullptr)) {
        return 0;
    }

    Measurements *measurements = tracksMeasurements.value(trackId);

    double rms = 0;

    if (measurements->count() > 0) {
        mutex->lock();
        rms = measurements->at(0).rms;
        mutex->unlock();
    }
    else {
        mutex->lock();
        tracksMeasurements.remove(trackId);
        mutex->unlock();
        delete measurements;
    }

    return rms;
}


// return peak value
double WebSocketClient::trackPeak(int trackId)
{
    if (!tracksMeasurements.contains(trackId) || (mutex == nullptr)) {
        return 0;
    }

    Measurements *measurements = tracksMeasurements.value(trackId);

    double peak = 0;

    if (measurements->count() > 0) {
        mutex->lock();
        peak = measurements->at(0).peak;
        mutex->unlock();
    }
    else {
        mutex->lock();
        tracksMeasurements.remove(trackId);
        mutex->unlock();
        delete measurements;
    }

    return peak;
}


// websocket signal handler
void WebSocketClient::socketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);

    emit this->error(webSocket->errorString());
}


// websocket signal handler
void WebSocketClient::binaryMessageReceived(const QByteArray &message)
{
    const char *address = message.constData();

    char   messageType = *address;
    int    instanceId  = *reinterpret_cast<const int *>(address + 1);;
    qint64 position    = *reinterpret_cast<const qint64 *>(address + 1 + sizeof(int));

    if (messageType == 'P') {
        if (!tracksMeasurements.contains(instanceId)) {
            return;
        }

        Measurements *measurements = tracksMeasurements.value(instanceId);
        int i = 0;
        while ((i < measurements->count()) && (measurements->at(i).timestamp < position)) {
            i++;
        }
        if (i > 0) {
            mutex->lock();
            measurements->remove(0, i - 1);
            mutex->unlock();
        }

        tracksLastSeen[instanceId] = QDateTime::currentMSecsSinceEpoch();

        return;
    }

    int channelIndex = *reinterpret_cast<const int *>(address + 1 + sizeof(int) + sizeof(qint64));

    if (channelIndex == audioChannel) {
        double rms  = *reinterpret_cast<const double *>(address + 1 + (sizeof(int) * 2) + sizeof(qint64));
        double peak = *reinterpret_cast<const double *>(address + 1 + (sizeof(int) * 2) + sizeof(qint64) + sizeof(double));

        if (tracksMeasurements.contains(instanceId)) {
            mutex->lock();
            tracksMeasurements[instanceId]->append({ position, rms, peak });
            mutex->unlock();
            return;
        }

        Measurements *measurements = new Measurements();

        measurements->append({ position, rms, peak });

        mutex->lock();
        tracksMeasurements.insert(instanceId, measurements);
        mutex->unlock();
    }
}


// timer slot
void WebSocketClient::cleanup()
{
    QVector<int> toBeRemoved;

    foreach (int trackId, tracksMeasurements.keys()) {
        if (tracksLastSeen.contains(trackId) && (tracksLastSeen.value(trackId) < (QDateTime::currentMSecsSinceEpoch() - 100))) {
            toBeRemoved.append(trackId);
        }
    }

    foreach (int trackId, toBeRemoved) {
        Measurements *measurements = tracksMeasurements.value(trackId);

        mutex->lock();
        tracksMeasurements.remove(trackId);
        mutex->unlock();

        delete measurements;
    }
}
