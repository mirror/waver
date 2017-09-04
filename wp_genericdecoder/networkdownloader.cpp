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


#include "networkdownloader.h"

// constructor
NetworkDownloader::NetworkDownloader(QUrl url, QWaitCondition *waitCondition)
{
    this->url           = url;
    this->waitCondition = waitCondition;

    fakePosition = 0;

    networkAccessManager = NULL;
    networkReply         = NULL;

    downloadStarted  = false;
    readyEmitted     = false;
    downloadFinished = false;
}


// destructor
NetworkDownloader::~NetworkDownloader()
{
    if (networkReply != NULL) {
        networkReply->abort();
        networkReply->close();
        networkReply->deleteLater();
    }

    if (networkAccessManager != NULL) {
        networkAccessManager->deleteLater();
    }

    foreach (QByteArray *bufferData, buffer) {
        delete bufferData;
    }
}


// thread entry piont
void NetworkDownloader::run()
{
    networkAccessManager = new QNetworkAccessManager();
    networkReply         = networkAccessManager->get(QNetworkRequest(url));

    connect(networkReply, SIGNAL(downloadProgress(qint64, qint64)),    this, SLOT(networkDownloadProgress(qint64, qint64)));
    connect(networkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(networkError(QNetworkReply::NetworkError)));

    QTimer::singleShot(7500, this, SLOT(connectionTimeout()));
    QTimer::singleShot(15000, this, SLOT(preCacheTimeout()));
}


// network reply signal handler
void NetworkDownloader::networkDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    // many times some headers will be received even when there's an error
    if (bytesReceived < 1024) {
        return;
    }

    // downloading
    downloadStarted = true;

    // check if this is the last chunk
    if (bytesReceived == bytesTotal) {
        downloadFinished = true;
    }

    // read the data
    QByteArray data = networkReply->readAll();

    // add it the buffer
    QByteArray *bufferData = new QByteArray(data.constData(), data.count());
    mutex.lock();
    buffer.append(bufferData);
    mutex.unlock();

    // let the world know when pre-caching is done
    if ((bytesReceived > 10240) && !readyEmitted) {
        emit ready();
        readyEmitted = true;
    }

    // wake up the decoder thread
    waitCondition->wakeAll();
}


// network reply signal handler
void NetworkDownloader::networkError(QNetworkReply::NetworkError code)
{
    Q_UNUSED(code);

    downloadFinished = true;
    emit error(networkReply->errorString());
}


// timer signal handler
void NetworkDownloader::connectionTimeout()
{
    // still couldn't start downloading, either this connection sucks or there was some unreported error, let's abort
    if (!downloadStarted) {
        networkReply->abort();
        downloadFinished = true;
        emit error("Connection timeout, aborting");
    }
}


// timer signal handler
void NetworkDownloader::preCacheTimeout()
{
    // still couldn't pre-cache
    if (!readyEmitted) {
        networkReply->abort();
        downloadFinished = true;
        emit error("Pre-cache timeout, aborting");
    }
}


// override
qint64 NetworkDownloader::readData(char *data, qint64 maxlen)
{
    if (buffer.count() < 1) {
        return 0;
    }

    qint64 returnPos = 0;

    mutex.lock();
    while ((returnPos < maxlen) && (buffer.count() > 0)) {
        qint64 copyCount = qMin(maxlen - returnPos, (qint64)buffer.at(0)->count());

        memcpy(data + returnPos, buffer.at(0)->constData(), copyCount);

        returnPos += copyCount;
        if (copyCount == buffer.at(0)->count()) {
            delete buffer.at(0);
            buffer.remove(0);
        }
        else {
            buffer.at(0)->remove(0, copyCount);
        }
    }
    mutex.unlock();

    fakePosition += returnPos;

    return returnPos;
}


// override
qint64 NetworkDownloader::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);

    return -1;
}


// override
bool NetworkDownloader::isSequential() const
{
    return true;
}


// override
qint64 NetworkDownloader::bytesAvailable() const
{
    if (downloadFinished && (buffer.count() < 1)) {
        return 0;
    }

    return std::numeric_limits<qint64>::max() - fakePosition;
}


// override
qint64 NetworkDownloader::pos() const
{
    return fakePosition;
}


// public method
qint64 NetworkDownloader::realBytesAvailable()
{
    qint64 returnValue = 0;

    mutex.lock();
    foreach (QByteArray *bufferData, buffer) {
        returnValue += bufferData->count();
    }
    mutex.unlock();

    return returnValue;
}


// public method
bool NetworkDownloader::isFinshed()
{
    return downloadFinished;
}


