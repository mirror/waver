/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef PCMCACHE_H
#define PCMCACHE_H

#include <QAudioBuffer>
#include <QAudioFormat>
#include <QByteArray>
#include <QFile>
#include <QMutex>
#include <QObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtGlobal>
#include <QTimer>
#include <QUuid>
#include <QList>

#include "globals.h"

#ifdef Q_OS_WIN
    #include "windows.h"
#endif

#ifdef QT_DEBUG
    #include <QDebug>
#endif


class PCMCache : public QObject
{
    Q_OBJECT


    public:

        static const qint32 BUFFER_CREATE_MILLISECONDS = 50;
        static const long   DEFAULT_PCM_MEMORY         = 50 * 1024 * 1024;
        static const long   MAX_PCM_MEMORY             = 500 * 1024 * 1024;

        explicit PCMCache(QAudioFormat format, long lengthMilliseconds, bool radioStation, QObject *parent = nullptr);
        ~PCMCache();

        void storeBuffer(QAudioBuffer *buffer);

        qint64 size();
        qint64 mostSize();
        bool   isFile();


    private:

        QAudioFormat format;
        qint64       lengthMilliseconds;
        bool         radioStation;

        QMutex mutex;

        QByteArray *memory;
        QFile      *file;

        qint64 memoryRealSize;
        qint64 maxSize;
        qint64 readPosition;
        qint64 radioFakeReadPosition;

        bool unfullfilledRequest;

        long availableMemory();


    public slots:

        void run();

        void requestNextPCMChunk();
        void requestTimestampPCMChunk(long milliseconds);


    signals:

        void pcmChunk(QByteArray PCM, qint64 startMicroseconds);
        void error(QString info, QString error);

};

#endif // PCMCACHE_H
