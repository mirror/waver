/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "pcmcache.h"

PCMCache::PCMCache(QAudioFormat format, long lengthMilliseconds, bool radioStation, QObject *parent) : QObject(parent)
{
    this->format             = format;
    this->lengthMilliseconds = lengthMilliseconds;
    this->radioStation       = radioStation;

    file                = nullptr;
    memory              = nullptr;
    readPosition        = 0;
    unfullfilledRequest = false;
}


PCMCache::~PCMCache()
{
    if (file != nullptr) {
        if (file->isOpen()) {
            mutex.lock();
            file->close();
            mutex.unlock();
        }
        file->remove();

        delete file;
        file = nullptr;

        return;
    }

    if (memory != nullptr) {
        mutex.lock();
        memory->clear();
        mutex.unlock();

        delete memory;
        memory = nullptr;
    }
}


long PCMCache::availableMemory()
{
#if defined (Q_OS_WIN)

    MEMORYSTATUSEX memoryStatus;

    ZeroMemory(&memoryStatus, sizeof(MEMORYSTATUSEX));
    memoryStatus.dwLength = sizeof(MEMORYSTATUSEX);

    if (GlobalMemoryStatusEx(&memoryStatus)) {
        return memoryStatus.ullAvailPhys;
    }

#elif defined (Q_OS_LINUX)

    QFile   memInfo("/proc/meminfo");
    QRegExp availableRegExp("^MemAvailable:\\s+(\\d+)");

    if (memInfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray memInfoContents = memInfo.readAll();
        memInfo.close();

        QString availableMemoryString;

        foreach (QString line, QString(memInfoContents).split("\n")) {
            if (availableRegExp.indexIn(line) >= 0) {
                availableMemoryString = availableRegExp.cap(1);
                break;
            }
        }

        bool OK = false;
        long availableMemoryKB = availableMemoryString.toLong(&OK);
        if (OK) {
            return availableMemoryKB * 1024;
        }
    }

#endif

    return DEFAULT_PCM_MEMORY;
}


bool PCMCache::isFile()
{
    return file != nullptr;
}


void PCMCache::requestNextPCMChunk()
{
    if (readPosition >= size()) {
        unfullfilledRequest = true;
        return;
    }
    unfullfilledRequest = false;

    qint64 chunkLength       = format.bytesForDuration(BUFFER_CREATE_MILLISECONDS * 1000);
    qint64 startMicroseconds = format.durationForBytes(readPosition);

    if (file != nullptr) {
        if (!file->isOpen()) {
            return;
        }

        mutex.lock();
        if (file->pos() != readPosition) {
            file->seek(readPosition);
        }
        QByteArray PCM = file->read(chunkLength);
        readPosition += PCM.size();
        mutex.unlock();

        emit pcmChunk(PCM, startMicroseconds, false);
        return;
    }

    if (memory != nullptr) {
        if (memory->size() < (readPosition + chunkLength)) {
            chunkLength = memory->size() - readPosition;
        }
        if (chunkLength <= 0) {
            return;
        }

        mutex.lock();
        QByteArray PCM = QByteArray(memory->data() + readPosition, chunkLength);
        if (radioStation) {
            memory->remove(0, chunkLength);
        }
        else {
            readPosition += PCM.size();
        }
        mutex.unlock();

        emit pcmChunk(PCM, startMicroseconds, false);
    }
}


void PCMCache::requestTimestampPCMChunk(long milliseconds)
{
    qint64 chunkLength = format.bytesForDuration(BUFFER_CREATE_MILLISECONDS * 1000);
    qint64 position    = qMin(static_cast<qint64>(format.bytesForDuration(milliseconds * 1000)), size() - chunkLength);
    if (position < 0) {
        position = 0;
    }
    qint64 startMicroseconds = format.durationForBytes(position);

    if (file != nullptr) {
        if (!file->isOpen()) {
            return;
        }

        mutex.lock();
        file->seek(position);
        QByteArray PCM = file->read(chunkLength);
        readPosition = position + PCM.size();
        mutex.unlock();

        emit pcmChunk(PCM, startMicroseconds, true);
        return;
    }

    if (memory != nullptr) {
        if (memory->size() < (position + chunkLength)) {
            chunkLength = memory->size() - position;
        }

        mutex.lock();
        QByteArray PCM = QByteArray(memory->data() + position, chunkLength);
        readPosition = position + PCM.size();
        mutex.unlock();

        emit pcmChunk(PCM, startMicroseconds, true);
    }
}


void PCMCache::run()
{
    if (((lengthMilliseconds <= 0) && !radioStation) || (format.bytesForDuration(lengthMilliseconds * 1000) > availableMemory())) {
        file = new QFile(QString("%1/waver_%2").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), QUuid::createUuid().toString(QUuid::Id128)));

        if (!file->open(QIODevice::ReadWrite)) {
            emit error(tr("Can not create temporary file, using memory"), file->errorString());
            delete file;
            file = nullptr;
        }
    }

    if (file == nullptr) {
        memory = new QByteArray();
    }
}


qint64 PCMCache::size()
{
    qint64 size = 0;

    mutex.lock();
    if (file != nullptr) {
        size = file->size();
    }
    else if (memory != nullptr) {
        size = memory->size();
    }
    mutex.unlock();

    return size;
}


void PCMCache::storeBuffer(QAudioBuffer buffer)
{
    if (file != nullptr) {
        mutex.lock();
        if (!file->atEnd()) {
            file->seek(file->size());
        }
        file->write(static_cast<char*>(buffer.data()), buffer.byteCount());
        mutex.unlock();

        if (unfullfilledRequest) {
            requestNextPCMChunk();
        }

        return;
    }

    if (memory != nullptr) {
        mutex.lock();
        memory->append(static_cast<char*>(buffer.data()), buffer.byteCount());
        mutex.unlock();

        if (unfullfilledRequest) {
            requestNextPCMChunk();
        }

        return;
    }

    emit error(tr("Can not cache PCM audio data"), tr("Both file and memeory is nullptr"));
}
