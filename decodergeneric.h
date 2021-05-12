/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef DECODERGENERIC_H
#define DECODERGENERIC_H

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QtGlobal>
#include <QtMath>
#include <QThread>
#include <QUrl>
#include <QVector>
#include <QWaitCondition>

#include "decodergenericnetworksource.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


class DecoderGeneric : public QObject
{
    Q_OBJECT

    public:

        explicit DecoderGeneric(QObject *parent = nullptr);
        ~DecoderGeneric();

        void   setParameters(QUrl url, QAudioFormat decodedFormat);
        qint64 getDecodedMicroseconds();

        qint64 size();
        bool   isFile();


    private:

        QAudioDecoder *audioDecoder;

        QUrl                         url;
        QAudioFormat                 decodedFormat;
        QFile                       *file;
        QThread                      networkThread;
        DecoderGenericNetworkSource *networkSource;

        QMutex         waitMutex;
        QWaitCondition waitCondition;

        bool          networkDeviceSet;
        unsigned long decodeDelay;
        qint64        decodedMicroseconds;


    private slots:

        void networkReady();
        void networkError(QString errorString);
        void networkInfo(QString infoString);
        void networkRadioTitle(QString title);

        void decoderBufferReady();
        void decoderFinished();
        void decoderError(QAudioDecoder::Error error);


    public slots:

        void run();
        void start();
        void setDecodeDelay(unsigned long microseconds);


    signals:

        void bufferAvailable(QAudioBuffer *buffer);
        void radioTitle(QString title);
        void finished();
        void errorMessage(QString info, QString error);
        void infoMessage(QString info);
        void networkStarting(bool starting);

};

#endif // DECODERGENERIC_H
