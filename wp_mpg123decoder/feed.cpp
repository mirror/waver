/*
    This file is part of Waver

    Copyright (C) 2017-2020 Peter Papp <peter.papp.p@gmail.com>

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
Feed::Feed(QUrl url, QString userAgent) : QObject(0)
{
    this->url       = url;
    this->userAgent = userAgent;

    file                 = NULL;
    networkAccessManager = NULL;
    networkReply         = NULL;

    downloadStarted  = false;
    readyEmitted     = false;
    downloadFinished = false;
    totalRawBytes    = 0;
    totalBufferBytes = 0;

    rawChunkSize = 0;
    rawCount     = 0;
    icyMetaSize  = 0;
    icyCount     = 0;
}


// destructor
Feed::~Feed()
{
    if (file != NULL) {
        file->close();
        delete file;
    }

    if (networkReply != NULL) {
        networkReply->abort();
        delete networkReply;
    }

    if (networkAccessManager != NULL) {
        delete networkAccessManager;
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

    QNetworkRequest networkRequest = QNetworkRequest(url);
    networkRequest.setRawHeader("User-Agent", userAgent.toUtf8());
    networkRequest.setRawHeader("Icy-MetaData", "1");
    networkRequest.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    networkRequest.setMaximumRedirectsAllowed(12);

    // TODO!!! This is temporary for testing purposes. Disables all SLL certificate checking. To be replaced with user-defined whitelist.
    QSslConfiguration sslConfiguration = networkRequest.sslConfiguration();
    sslConfiguration.setPeerVerifyMode(QSslSocket::VerifyNone);
    networkRequest.setSslConfiguration(sslConfiguration);

    networkReply = networkAccessManager->get(networkRequest);

    connect(networkReply, SIGNAL(downloadProgress(qint64, qint64)),   this, SLOT(networkDownloadProgress(qint64, qint64)));
    connect(networkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(networkError(QNetworkReply::NetworkError)));
    connect(networkReply, SIGNAL(redirected(QUrl)),                   this, SLOT(networkRedirected(QUrl)));

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

    // check if SHOUTcast metadata must be extracted (Icy-MetaData header was sent with the request)
    if ((rawChunkSize == 0) && networkReply->hasRawHeader("icy-metaint")) {
        QString icyMetaInt(networkReply->rawHeader("icy-metaint"));
        bool OK = false;
        rawChunkSize = icyMetaInt.toInt(&OK);
        if (!OK) {
            rawChunkSize = 0;
        }
    }

    // extract SHOUTcast metadata
    if (rawChunkSize > 0) {
        int pointer = 0;
        while (pointer < data.count()) {
            // is pointer inside metadata now?
            if (rawCount < 0) {
                // was metadata size obtained yet?
                if (icyMetaSize < 0) {
                    // first byte of metadata is size of the rest of metadata divided by 16
                    char *lenByte = data.data() + pointer;
                    icyMetaSize = 16 * *lenByte;

                    // remove this byte, must not pass it to the decoder, pointer needs not to be increased
                    data.remove(pointer, 1);
                }

                // downloaded data might not contain all the metadata, the rest will be in next download chunk
                int increment = qMin(icyMetaSize - icyCount, data.count() - pointer);

                // add to metadata buffer
                icyBuffer.append(data.data() + pointer, increment);

                // remove from data, must not pass it to the decoder, pointer needs not to be increased
                data.remove(pointer, increment);

                // update metadata bytes counter
                icyCount += increment;

                // is the end of metadata reached?
                if (icyCount == icyMetaSize) {
                    // many times metadata is empty, most stations send only on connection and track change
                    if (icyBuffer.count() > 0) {
                        // search for title
                        QRegExp finder("StreamTitle='(.+)';");
                        finder.setMinimal(true);
                        if (finder.indexIn(QString(icyBuffer)) >= 0) {
                            // got it, let the decoder know
                            emit SHOUTcastTitle(totalRawBytes, finder.cap(1));
                        }
                    }

                    // reset counter, prepare for audio data
                    rawCount = 0;
                }
            }
            else {
                // downloaded data might not contain the entire block of compressed audio data, the rest will be in next download chunk
                int increment = qMin(rawChunkSize - rawCount, data.count() - pointer);

                // increase pointer and counters
                pointer       += increment;
                rawCount      += increment;
                totalRawBytes += increment;

                // is the end of audio block reached?
                if (rawCount == rawChunkSize) {
                    // signals that pointer reached metadata
                    rawCount = -1;

                    // prepare for metadata
                    icyCount    = 0;
                    icyMetaSize = -1;
                    icyBuffer.clear();
                }
            }
        }
    }

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

    // update stats
    updateTotalBufferBytes();
}


// network reply signal handler
void Feed::networkError(QNetworkReply::NetworkError code)
{
    Q_UNUSED(code);

    downloadFinished = true;
    emit error(networkReply->errorString());
}


// network reply signal handler
void Feed::networkRedirected(QUrl url)
{
    // TODO this list should be supplied by source plugins
    QStringList blacklist;
    blacklist.append("JamendoLounge");
    blacklist.append("appblockingus.mp3");

    bool redirectOK = true;
    foreach (QString blacklistItem, blacklist) {
        if (url.toString().contains(blacklistItem)) {
            redirectOK = false;
            break;
        }
    }

    if (!redirectOK) {
        networkReply->abort();
    }
}


// timer signal handler
void Feed::fileReadTimer()
{
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

    // increase counter
    totalRawBytes += data.count();

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

    // update stats
    updateTotalBufferBytes();

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
    // update stats
    updateTotalBufferBytes();

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


// public metod
qint64 Feed::getTotalBufferBytes()
{
    return totalBufferBytes;
}


// helper
void Feed::updateTotalBufferBytes()
{
    totalBufferBytes = 0;
    mutex.lock();
    foreach (QByteArray *bufferElement, buffer) {
        totalBufferBytes += bufferElement->size();
    }
    mutex.unlock();
}
