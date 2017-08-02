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


#include "feed.h"

// constructor
Feed::Feed(QUrl url) : QObject(0)
{
    this->url           = url;

    file                 = NULL;
    networkAccessManager = NULL;
    networkReply         = NULL;

    downloadStarted  = false;
    readyEmitted     = false;
    downloadFinished = false;
}


// destructor
Feed::~Feed()
{
    if (file != NULL) {
        file->close();
        file->deleteLater();
    }

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
void Feed::run()
{
    if (url.isLocalFile()) {
        file = new QFile(url.toLocalFile());

        if (!file->exists()) {
            downloadFinished = true;
            emit error("File not exists, aborting");
            return;
        }

        file->open(QFile::ReadOnly);
        fileReadTimer();
        return;
    }

    networkAccessManager = new QNetworkAccessManager();
    networkReply = networkAccessManager->get(QNetworkRequest(url));

    connect(networkReply, SIGNAL(downloadProgress(qint64, qint64)),    this, SLOT(networkDownloadProgress(qint64, qint64)));
    connect(networkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(networkError(QNetworkReply::NetworkError)));

    QTimer::singleShot(7500, this, SLOT(connectionTimeout()));
    QTimer::singleShot(15000, this, SLOT(preCacheTimeout()));
}


// network reply signal handler
void Feed::networkDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
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
}


// network reply signal handler
void Feed::networkError(QNetworkReply::NetworkError code)
{
    Q_UNUSED(code);

    downloadFinished = true;
    emit error(networkReply->errorString());
}


// timer signal handler
void Feed::fileReadTimer()
{
    // get the total size of data already read into memory
    qint64 totalBufferBytes = 0;
    mutex.lock();
    foreach (QByteArray *bufferElement, buffer) {
        totalBufferBytes += bufferElement->size();
    }
    mutex.unlock();

    // if it's too much, let's wait until decoder processes some of it
    if (totalBufferBytes >= (10 * 1024 * 1024)) {
        QTimer::singleShot(5000, this, SLOT(fileReadTimer()));
        return;
    }

    // read from file
    QByteArray data = file->read(1024 * 1024);

    // add it to the buffer
    QByteArray *bufferData = new QByteArray(data.constData(), data.count());
    mutex.lock();
    buffer.append(bufferData);
    mutex.unlock();

    // let the world know
    if (!readyEmitted) {
        emit ready();
        readyEmitted = true;
    }

    // finish reading if eof reached
    if (file->atEnd()) {
        file->close();
        downloadFinished = true;
        return;
    }

    // read more later
    QTimer::singleShot(2500, this, SLOT(fileReadTimer()));
}


// timer signal handler
void Feed::connectionTimeout()
{
    // still couldn't start downloading, either this connection sucks or there was some unreported error, let's abort
    if (!downloadStarted) {
        networkReply->abort();
        downloadFinished = true;
        emit error("Connection timeout, aborting");
    }
}


// timer signal handler
void Feed::preCacheTimeout()
{
    // still couldn't pre-cache
    if (!readyEmitted) {
        networkReply->abort();
        downloadFinished = true;
        emit error("Pre-cache timeout, aborting");
    }
}


// public method
size_t Feed::read(char *data, size_t maxlen)
{
    if (buffer.count() < 1) {
        return 0;
    }

    size_t returnPos = 0;

    mutex.lock();
    while ((returnPos < maxlen) && (buffer.count() > 0)) {
        size_t copyCount = qMin(maxlen - returnPos, (size_t)buffer.at(0)->count());

        memcpy(data + returnPos, buffer.at(0)->constData(), copyCount);

        returnPos += copyCount;
        if (copyCount == (size_t)buffer.at(0)->count()) {
            delete buffer.at(0);
            buffer.remove(0);
        }
        else {
            buffer.at(0)->remove(0, copyCount);
        }
    }
    mutex.unlock();

    return returnPos;
}


// public method
bool Feed::isFinished()
{
    return downloadFinished;
}


