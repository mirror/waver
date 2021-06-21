/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef DECODERGENERICNETWORKSOURCE_H
#define DECODERGENERICNETWORKSOURCE_H

#include <QByteArray>
#include <QGuiApplication>
#include <QIODevice>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVector>
#include <QWaitCondition>

#ifdef QT_DEBUG
    #include <QDebug>
#endif


class DecoderGenericNetworkSource : public QIODevice
{
    Q_OBJECT

    public:

        DecoderGenericNetworkSource(QUrl url, QWaitCondition *waitCondition);
        ~DecoderGenericNetworkSource();

        qint64 readData(char *data, qint64 maxlen)     override;
        qint64 writeData(const char *data, qint64 len) override;
        bool   seek(qint64 pos)                        override;

        bool   atEnd()          const override;
        bool   isSequential()   const override;
        qint64 bytesAvailable() const override;
        qint64 pos()            const override;
        qint64 size()           const override;

        qint64 realBytesAvailable();
        bool   isFinshed();
        void   setErrorOnUnderrun(bool errorOnUnderrun);


    private:

        static const int CONNECTION_ATTEMPTS = 3;
        static const int CONNECTION_TIMEOUT  = 7500;
        static const int PRE_CACHE_TIMEOUT   = 15000;

        QUrl            url;
        QWaitCondition *waitCondition;

        QNetworkAccessManager *networkAccessManager;
        QNetworkReply         *networkReply;

        QVector<QByteArray *> buffer;
        qint64                totalDownloadedBytes;
        qint64                totalExpectedBytes;
        qint64                fakePosition;
        qint64                firstBufferPosition;
        QVector<qint64>       seekHistory;

        QMutex mutex;

        QTimer *connectionTimer;
        QTimer *preCacheTimer;

        int  connectionAttempt;
        bool downloadStarted;
        bool downloadFinished;
        bool readyEmitted;
        bool errorOnUnderrun;

        int        rawChunkSize;
        int        rawCount;
        int        metaSize;
        int        metaCount;
        QByteArray metaBuffer;
        qint64     totalMetaBytes;

        bool   bufferIndexPositionFromPosition(qint64 position, int *bufferIndex, int *bufferPosition);

        QNetworkRequest buildNetworkRequest();


    signals:

        void error(QString errorString);
        void info(QString errorString);
        void sessionExpired();
        void ready();
        void radioTitle(QString title);


    public slots:

        void run();


    private slots:

        void emitReady();

        void networkDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
        void networkError(QNetworkReply::NetworkError code);

        void connectionTimeout();
        void preCacheTimeout();
};

#endif // DECODERGENERICNETWORKSOURCE_H
