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
#include <QList>
#include <QWaitCondition>

#include "decodergenericnetworksource.h"
#include "radiotitlecallback.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


class DecoderGeneric : public QObject
{
    Q_OBJECT

    public:

        explicit DecoderGeneric(RadioTitleCallback::RadioTitleCallbackInfo radioTitleCallbackInfo, QObject *parent = nullptr);
        ~DecoderGeneric();

        void   setParameters(QUrl url, QAudioFormat decodedFormat, qint64 waitUnderBytes, bool isRadio);
        qint64 getDecodedMicroseconds();

        double downloadPercent();
        bool   isFile();


    private:

        QAudioDecoder *audioDecoder;

        QUrl         url;
        QAudioFormat decodedFormat;
        qint64       waitUnderBytes;
        bool         isRadio;

        QFile                       *file;
        QThread                      networkThread;
        DecoderGenericNetworkSource *networkSource;

        QMutex         waitMutex;
        QWaitCondition waitCondition;

        bool          networkDeviceSet;
        unsigned long decodeDelay;
        qint64        decodedMicroseconds;

        RadioTitleCallback::RadioTitleCallbackInfo radioTitleCallbackInfo;


    private slots:

        void networkReady();
        void networkChanged();
        void networkError(QString errorString);
        void networkInfo(QString infoString);
        void networkSessionExpired();

        void decoderBufferReady();
        void decoderFinished();
        void decoderError(QAudioDecoder::Error error);


    public slots:

        void run();
        void start();
        void setDecodeDelay(unsigned long microseconds);


    signals:

        void bufferAvailable(QAudioBuffer *buffer);
        void networkBufferChanged();
        void finished();
        void errorMessage(QString info, QString error);
        void infoMessage(QString info);
        void sessionExpired();
        void networkStarting(bool starting);

};

#endif // DECODERGENERIC_H
