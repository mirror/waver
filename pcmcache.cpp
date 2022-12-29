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

    file                  = nullptr;
    memory                = nullptr;
    memoryRealSize        = 0;
    maxSize               = 0;
    readPosition          = 0;
    radioFakeReadPosition = 0;
    unfullfilledRequest   = false;
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
        quint64 availPhys = memoryStatus.ullAvailPhys;
        long    longMax   = (std::numeric_limits<long>::max)();

        return (availPhys > static_cast<DWORD>(longMax) ? longMax : static_cast<long>(availPhys));
    }

#elif defined (Q_OS_LINUX)

    QFile              memInfo("/proc/meminfo");
    QRegularExpression availableRegExp("^MemAvailable:\\s+(\\d+)");

    if (memInfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray memInfoContents = memInfo.readAll();
        memInfo.close();

        QString availableMemoryString;

        foreach (QString line, QString(memInfoContents).split("\n")) {
            QRegularExpressionMatch match = availableRegExp.match(line);
            if (match.hasMatch()) {
                availableMemoryString = match.captured(1);
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


qint64 PCMCache::mostSize()
{
    return maxSize;
}


void PCMCache::requestNextPCMChunk()
{
    if (readPosition >= size()) {
        unfullfilledRequest = true;
        return;
    }
    unfullfilledRequest = false;

    mutex.lock();

    qint64 chunkLength       = format.bytesForDuration(BUFFER_CREATE_MILLISECONDS * 1000);
    qint64 startMicroseconds = radioStation ? format.durationForBytes(radioFakeReadPosition) : format.durationForBytes(readPosition);

    QByteArray PCM;

    if (file != nullptr) {
        if (file->isOpen()) {
            if (file->pos() != readPosition) {
                file->seek(readPosition);
            }
            PCM.append(file->read(chunkLength));
            readPosition += PCM.size();
        }
    }
    else if (memory != nullptr) {
        if (memoryRealSize < (readPosition + chunkLength)) {
            chunkLength = memoryRealSize - readPosition;
        }
        if (chunkLength > 0) {
            PCM.append(memory->constData() + readPosition, chunkLength);
            if (radioStation) {
                memory->remove(0, chunkLength);
                memoryRealSize -= chunkLength;
                radioFakeReadPosition += chunkLength;
            }
            else {
                readPosition += PCM.size();
            }
        }
    }

    mutex.unlock();

    if (PCM.size()) {
        emit pcmChunk(PCM, startMicroseconds);
    }
}


void PCMCache::requestTimestampPCMChunk(long milliseconds)
{
    if (radioStation) {
        unfullfilledRequest = true;
        return;
    }

    qint64 currentSize = size();

    mutex.lock();

    qint64 chunkLength = format.bytesForDuration(BUFFER_CREATE_MILLISECONDS * 1000);
    qint64 position    = qMin(static_cast<qint64>(format.bytesForDuration(milliseconds * 1000)), currentSize - chunkLength);
    if (position < 0) {
        position = 0;
    }
    qint64 startMicroseconds = format.durationForBytes(position);

    QByteArray PCM;

    if (file != nullptr) {
        if (file->isOpen()) {
            file->seek(position);
            PCM.append(file->read(chunkLength));
            readPosition = position + PCM.size();
        }
    }
    else if (memory != nullptr) {
        if (memoryRealSize < (position + chunkLength)) {
            chunkLength = memoryRealSize - position;
        }
        if (chunkLength > 0) {
            PCM.append(memory->constData() + position, chunkLength);
            readPosition = position + PCM.size();
        }
    }

    mutex.unlock();

    if (PCM.size()) {
        emit pcmChunk(PCM, startMicroseconds);
    }
}


void PCMCache::run()
{
    qint64 bytesNeeded    = format.bytesForDuration(lengthMilliseconds * 1000);
    long   bytesAvailable = availableMemory();

    if (((lengthMilliseconds <= 0) && !radioStation) || (bytesNeeded > MAX_PCM_MEMORY) || (bytesNeeded > bytesAvailable)) {
        file = new QFile(QString("%1/waver_%2").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), QUuid::createUuid().toString(QUuid::Id128)));

        if (!file->open(QIODevice::ReadWrite)) {
            emit error(tr("Can not create temporary file, using memory"), file->errorString());
            delete file;
            file = nullptr;
        }
    }

    if (file == nullptr) {
        if (radioStation || (lengthMilliseconds <= 0)) {
            memory = new QByteArray();
        }
        else {
            memory = new QByteArray(format.bytesForDuration((lengthMilliseconds + 1000) * 1000), 0);
        }
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
        size = memoryRealSize;
    }
    mutex.unlock();

    if (size > maxSize) {
        maxSize = size;
    }

    return size;
}


void PCMCache::storeBuffer(QAudioBuffer *buffer)
{
    if (file != nullptr) {
        mutex.lock();
        if (!file->atEnd()) {
            file->seek(file->size());
        }
        file->write(static_cast<const char*>(buffer->constData<char>()), buffer->byteCount());
        mutex.unlock();

        if (unfullfilledRequest) {
            requestNextPCMChunk();
        }

        return;
    }

    if (memory != nullptr) {
        mutex.lock();
        if (radioStation || (lengthMilliseconds <= 0)) {
            memory->append(static_cast<const char*>(buffer->constData<char>()), buffer->byteCount());
        }
        else {
            memory->replace(memoryRealSize, buffer->byteCount(), static_cast<const char*>(buffer->constData<char>()), buffer->byteCount());
        }
        memoryRealSize += buffer->byteCount();
        mutex.unlock();

        if (unfullfilledRequest) {
            requestNextPCMChunk();
        }

        return;
    }

    emit error(tr("Can not cache PCM audio data"), tr("Both file and memeory is nullptr"));
}
