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


#ifndef STREAMINGIOBUFFER_H
#define STREAMINGIOBUFFER_H

#include <QByteArray>
#include <QIODevice>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVector>
#include <QWaitCondition>

#ifdef QT_DEBUG
    #include <QDebug>
#endif


// TODO retry download if interrupted


class NetworkDownloader : public QIODevice {
        Q_OBJECT

    public:

        NetworkDownloader(QUrl url, QWaitCondition *waitCondition, QString userAgent);
        ~NetworkDownloader();

        qint64 readData(char *data, qint64 maxlen)     override;
        qint64 writeData(const char *data, qint64 len) override;

        bool   isSequential() const override;
        qint64 bytesAvailable() const override;
        qint64 pos() const override;

        qint64 realBytesAvailable();
        bool   isFinshed();


    private:

        QUrl            url;
        QString         userAgent;
        QWaitCondition *waitCondition;

        QNetworkAccessManager *networkAccessManager;
        QNetworkReply         *networkReply;

        QVector<QByteArray *> buffer;
        qint64               fakePosition;

        QMutex mutex;

        bool downloadStarted;
        bool readyEmitted;
        bool downloadFinished;


    signals:

        void ready();
        void error(QString errorString);


    public slots:

        void run();


    private slots:

        void networkDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
        void networkError(QNetworkReply::NetworkError code);

        void connectionTimeout();
        void preCacheTimeout();

};

#endif // STREAMINGIOBUFFER_H
