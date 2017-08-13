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


#ifndef MPG123DECODER_H
#define MPG123DECODER_H

#include "wp_mpg123decoder_global.h"

#include <QtMath>

#include <QAudioBuffer>
#include <QAudioFormat>
#include <QCoreApplication>
#include <QMutex>
#include <QSysInfo>
#include <QThread>
#include <QVector>

#include "feed.h"
#include "mpg123lib/mpg123.h"
#include "../waver/API/0.0.1/plugindecoder.h"
#include "../waver/pluginfactory.h"


extern "C" WP_MPG123DECODER_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);


class WP_MPG123DECODER_EXPORT Mpg123Decoder : public PluginDecoder {
        Q_OBJECT

    public:

        int     pluginType()                   override;
        QString pluginName()                   override;
        int     pluginVersion()                override;
        QString waverVersionAPICompatibility() override;
        QUuid   persistentUniqueId()           override;
        bool    hasUI()                        override;
        void    setUrl(QUrl url)               override;

        explicit Mpg123Decoder();
        ~Mpg123Decoder();


    private:

        static const unsigned long MAX_MEMORY   = 50 * 1024 * 1024;
        static const unsigned long USEC_PER_SEC = 1000000;
        static const size_t        INPUT_SIZE   = 4 * 1024;
        static const size_t        OUTPUT_SIZE  = 16 * 1024;

        QUuid id;
        QUrl  url;

        mpg123_handle *mpg123Handle;

        QVector<QAudioBuffer *> audioBuffers;

        QThread  feedThread;
        Feed    *feed;

        int    memoryUsage;
        qint64 decodedMicroSeconds;


    public slots:

        void run()                 override;
        void start(QUuid uniqueId) override;

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration) override;

        void getUiQml(QUuid uniqueId)                         override;
        void uiResults(QUuid uniqueId, QJsonDocument results) override;

        void bufferDone(QUuid uniqueId, QAudioBuffer *buffer) override;


    private slots:

        void feedReady();
        void feedError(QString errorString);

};

#endif // MPG123DECODER_H
