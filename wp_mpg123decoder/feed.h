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


#ifndef FEED_H
#define FEED_H

#include <QFile>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QTimer>
#include <QUrl>


// TODO retry download if interrupted

class Feed : public QObject {
        Q_OBJECT

    public:

        explicit Feed(QUrl url, QString userAgent);
        ~Feed();

        size_t read(char *data, size_t maxlen);
        bool   isFinished();


    private:

        QUrl url;

        QFile *file;

        QNetworkAccessManager *networkAccessManager;
        QNetworkReply         *networkReply;

        QVector<QByteArray *> buffer;

        QMutex mutex;

        bool    downloadStarted;
        bool    readyEmitted;
        bool    downloadFinished;
        QString userAgent;


    signals:

        void ready();
        void error(QString errorString);


    public slots:

        void run();


    private slots:

        void networkDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
        void networkError(QNetworkReply::NetworkError code);
        void networkRedirected(QUrl url);

        void fileReadTimer();
        void connectionTimeout();
        void preCacheTimeout();

};

#endif // FEED_H
