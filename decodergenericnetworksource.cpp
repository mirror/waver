/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "decodergenericnetworksource.h"


DecoderGenericNetworkSource::DecoderGenericNetworkSource(QUrl url, QWaitCondition *waitCondition) : QIODevice()
{
    this->url           = url;
    this->waitCondition = waitCondition;

    fakePosition         = 0;
    firstBufferPosition  = 0;
    totalDownloadedBytes = 0;
    totalExpectedBytes   = std::numeric_limits<qint64>::max();

    networkAccessManager = nullptr;
    networkReply         = nullptr;

    downloadStarted  = false;
    downloadFinished = false;
    readyEmitted     = false;
    errorOnUnderrun  = true;

    rawChunkSize   = 0;
    rawCount       = 0;
    metaSize       = 0;
    metaCount      = 0;
    totalMetaBytes = 0;
}


DecoderGenericNetworkSource::~DecoderGenericNetworkSource()
{
    if (networkReply != nullptr) {
        networkReply->abort();
        delete networkReply;
    }

    if (networkAccessManager != nullptr) {
        delete networkAccessManager;
    }

    foreach (QByteArray *bufferData, buffer) {
        delete bufferData;
    }
}


bool DecoderGenericNetworkSource::atEnd() const
{

    return (fakePosition >= totalExpectedBytes);
}


bool DecoderGenericNetworkSource::bufferIndexPositionFromPosition(qint64 position, int *bufferIndex, int *bufferPosition)
{
    // this is a helper for the random access mode

    *bufferIndex    = 0;
    *bufferPosition = 0;

    // some raw data possibly already deleted to keep memory usage low
    qint64 bufferBytesCount = firstBufferPosition;

    mutex.lock();

    // save this for after the mutex is unlocked
    int bufferCount = buffer.count();

    // find the byte array that contains the position we're looking for
    while (*bufferIndex < bufferCount) {
        if (bufferBytesCount + buffer.at(*bufferIndex)->count() >= position) {
            break;
        }

        bufferBytesCount += buffer.at(*bufferIndex)->count();
        *bufferIndex      = *bufferIndex + 1;
    }

    mutex.unlock();

    // this means it's not found
    if (*bufferIndex >= bufferCount) {
        return false;
    }

    // this is where the position is, inside the found byte array
    *bufferPosition = position - bufferBytesCount;

    return true;
}


qint64 DecoderGenericNetworkSource::bytesAvailable() const
{
    if (downloadFinished && (buffer.count() < 1)) {
         return 0;
    }
    return totalDownloadedBytes - fakePosition;
}


void DecoderGenericNetworkSource::connectionTimeout()
{
    // still couldn't start downloading, either this connection sucks or there was some unreported error, let's abort
    if (!downloadStarted) {
        networkReply->abort();
        downloadFinished = true;
        emit error(tr("Connection timeout, aborting"));
    }
}


void DecoderGenericNetworkSource::emitReady()
{
    emit ready();
}


bool DecoderGenericNetworkSource::isFinshed()
{
    return downloadFinished;
}


bool DecoderGenericNetworkSource::isSequential() const
{
    // Windows decoder needs random access :(
    #ifdef Q_OS_WINDOWS
        return false;
    #endif
    return true;
}


void DecoderGenericNetworkSource::networkDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
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

    // check if metadata must be extracted
    if ((rawChunkSize == 0) && networkReply->hasRawHeader("icy-metaint")) {
        QString icyMetaInt(networkReply->rawHeader("icy-metaint"));
        bool OK = false;
        rawChunkSize = icyMetaInt.toInt(&OK);
        if (!OK) {
            rawChunkSize = 0;
        }
    }

    // extract metadata
    if (rawChunkSize > 0) {
        int pointer = 0;
        while (pointer < data.count()) {
            // is pointer inside metadata now?
            if (rawCount < 0) {
                // was metadata size obtained yet?
                if (metaSize < 0) {
                    // first byte of metadata is size of the rest of metadata divided by 16
                    char *lenByte = data.data() + pointer;
                    metaSize = 16 * *lenByte;

                    // remove this byte, must not pass it to the decoder, pointer needs not to be increased
                    data.remove(pointer, 1);
                    totalMetaBytes++;
                }

                // downloaded data might not contain all the metadata, the rest will be in next download chunk
                int increment = qMin(metaSize - metaCount, data.count() - pointer);

                // add to metadata buffer
                metaBuffer.append(data.data() + pointer, increment);

                // remove from data, must not pass it to the decoder, pointer needs not to be increased
                data.remove(pointer, increment);
                totalMetaBytes += increment;

                // update metadata bytes counter
                metaCount += increment;

                // is the end of metadata reached?
                if (metaCount == metaSize) {
                    // many times metadata is empty, most stations send only on connection and track change
                    if (metaBuffer.count() > 0) {
                        // search for title
                        QRegExp finder("StreamTitle='(.+)';");
                        finder.setMinimal(true);
                        if (finder.indexIn(QString(metaBuffer)) >= 0) {
                            // got it, let the decoder know
                            emit radioTitle(finder.cap(1));
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

                // is the end of audio block reached?
                if (rawCount == rawChunkSize) {
                    // signals that pointer reached metadata
                    rawCount = -1;

                    // prepare for metadata
                    metaCount = 0;
                    metaSize  = -1;
                    metaBuffer.clear();
                }
            }
        }
    }

    // add it the buffer
    QByteArray *bufferData = new QByteArray(data.constData(), data.count());
    mutex.lock();
    buffer.append(bufferData);
    mutex.unlock();

    // for available bytes
    totalDownloadedBytes += bufferData->size();

    // this is used in size()
    if (bytesTotal > 0) {
        totalExpectedBytes = bytesTotal - totalMetaBytes;
    }
    else {
        totalExpectedBytes = totalDownloadedBytes;
    }

    // let the world know when pre-caching is done, bytesTotal is unknown (zero) for radio stations
    if (!readyEmitted && (bytesReceived > (bytesTotal > 0 ? qMin(bytesTotal, (qint64)1024 * 1024) : 65536))) {
        QTimer::singleShot(250, this, &DecoderGenericNetworkSource::emitReady);
        readyEmitted = true;
    }

    // wake up the decoder thread
    waitCondition->wakeAll();
}


void DecoderGenericNetworkSource::networkError(QNetworkReply::NetworkError code)
{
    Q_UNUSED(code);

    downloadFinished = true;
    emit error(networkReply->errorString());
}


qint64 DecoderGenericNetworkSource::pos() const
{
    return fakePosition;
}


void DecoderGenericNetworkSource::preCacheTimeout()
{
    // still couldn't pre-cache
    if (!readyEmitted) {
        networkReply->abort();
        downloadFinished = true;
        emit error("Pre-cache timeout, aborting");
    }
}


qint64 DecoderGenericNetworkSource::readData(char *data, qint64 maxlen)
{
    #ifdef Q_OS_LINUX
        if ((buffer.count() < 1) && (maxlen > 0) && errorOnUnderrun) {
            debugFile.close();
            emit error(tr("Download buffer underrun"));
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


        debugFile.write(QString("readData read size %1\n\r").arg(returnPos).toUtf8().data());
        debugFile.close();

        return returnPos;
    #endif

    #ifdef Q_OS_WINDOWS
        int bufferIndex;
        int bufferPosition;

        if (!bufferIndexPositionFromPosition(fakePosition, &bufferIndex, &bufferPosition)) {
            emit error(tr("Download buffer underrun"));
            return 0;
        }

        qint64 returnPos = 0;

        mutex.lock();
        while ((returnPos < maxlen) && (bufferIndex < buffer.count())) {
            qint64 copyCount = qMin(maxlen - returnPos, (qint64)buffer.at(bufferIndex)->count() - bufferPosition);

            memcpy(data + returnPos, buffer.at(bufferIndex)->constData() + bufferPosition, copyCount);
            returnPos += copyCount;

            bufferIndex++;
            bufferPosition = 0;
        }
        mutex.unlock();

        fakePosition += returnPos;

        return returnPos;

    #endif

    return 0;
}


qint64 DecoderGenericNetworkSource::realBytesAvailable()
{
    qint64 returnValue = 0;
    int    bufferIndex = 0;

    if (!isSequential()) {
        int bufferPosition = 0;
        if (!bufferIndexPositionFromPosition(fakePosition, &bufferIndex, &bufferPosition)) {
            return 0;
        }
        returnValue += buffer.at(bufferIndex)->size() - bufferPosition;
        bufferIndex++;
    }

    mutex.lock();
    int i = bufferIndex;
    while (i < buffer.size()) {
        returnValue += buffer.at(i)->size();
        i++;
    }
    mutex.unlock();

    return returnValue;
}


void DecoderGenericNetworkSource::run()
{
    networkAccessManager = new QNetworkAccessManager();

    QNetworkRequest networkRequest = QNetworkRequest(url);
    networkRequest.setRawHeader("User-Agent", QGuiApplication::instance()->applicationName().toUtf8());
    networkRequest.setRawHeader("Icy-MetaData", "1");
    networkRequest.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    networkRequest.setMaximumRedirectsAllowed(12);

    networkReply = networkAccessManager->get(networkRequest);

    connect(networkReply, SIGNAL(downloadProgress(qint64,qint64)),    this, SLOT(networkDownloadProgress(qint64,qint64)));
    connect(networkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(networkError(QNetworkReply::NetworkError)));

    QTimer::singleShot(CONNECTION_TIMEOUT, this, SLOT(connectionTimeout()));
    QTimer::singleShot(PRE_CACHE_TIMEOUT, this, SLOT(preCacheTimeout()));
}


bool DecoderGenericNetworkSource::seek(qint64 pos)
{
    // make sure seek is possible
    if (isSequential() || (pos > totalDownloadedBytes)) {
        return false;
    }

    // do seek a.k.a. set current position
    fakePosition = pos;

    // prevent buffer from growing forever
    // this is based on observation of Windows decoder calling 'seek' throughout the process of decoding: it goes backwards frequently,
    // but it seems to be safe to delete the bytes from the beginning that are not beyond current position and were not positioned to in past five 'seek' calls
    if (pos > 0) {
        seekHistory.append(pos);

        if (seekHistory.size() > 5) {
            qint64 first = seekHistory.first();
            seekHistory.removeFirst();

            if ((first < fakePosition) && !seekHistory.contains(first)) {
                int bufferIndex;
                int bufferPosition;

                if (bufferIndexPositionFromPosition(first, &bufferIndex, &bufferPosition)) {
                    int i = 0;
                    while (i < bufferIndex) {
                        delete buffer.at(0);
                        buffer.remove(0);
                        i++;
                    }
                    buffer.at(0)->remove(0, bufferPosition);

                    firstBufferPosition = first;
                }
            }
        }
    }

    return true;
}


qint64 DecoderGenericNetworkSource::size() const
{
    return totalExpectedBytes;
}


void DecoderGenericNetworkSource::setErrorOnUnderrun(bool errorOnUnderrun)
{
    this->errorOnUnderrun = errorOnUnderrun;
}


qint64 DecoderGenericNetworkSource::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);

    return -1;
}

