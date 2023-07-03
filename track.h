/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/

#ifndef TRACK_H
#define TRACK_H

#include <QAudioFormat>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QRegExp>
#include <QSettings>
#include <QStandardPaths>
#include <QtGlobal>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include "analyzer.h"
#include "decodergeneric.h"
#include "decodingcallback.h"
#include "equalizer.h"
#include "globals.h"
#include "peakcallback.h"
#include "pcmcache.h"
#include "radiotitlecallback.h"
#include "soundoutput.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


class Track : public QObject, RadioTitleCallback
{
    Q_OBJECT

    public:

        enum Status {
            Idle,
            Decoding,
            Playing,
            Paused
        };

        struct TrackInfo {
            QString      id;
            QUrl         url;
            QList<QUrl>  arts;
            QString      title;
            QString      artistId;
            QString      artist;
            QString      albumId;
            QString      album;
            int          year;
            int          track;
            QStringList  tags;
            QVariantHash attributes;
        };
        typedef QVector<TrackInfo> TracksInfo;

        static const int EQ_CHOOSER_COMMON = 0;
        static const int EQ_CHOOSER_ALBUM  = 1;
        static const int EQ_CHOOSER_TRACK  = 2;


        explicit Track(TrackInfo trackInfo, PeakCallback::PeakCallbackInfo peakCallbackInfo, DecodingCallback::DecodingCallbackInfo decodingCallbackInfo, QObject *parent = 0);
        ~Track();

        Status  getStatus();
        QString getStatusText();
        void    setStatus(Status status);
        void    setPosition(double percent);

        TrackInfo       getTrackInfo();
        void            attributeAdd(QString key, QVariant value);
        void            attributeRemove(QString key);
        qint64          getDecodedMilliseconds();
        qint64          getLengthMilliseconds();
        qint64          getPlayedMillseconds();
        int             getFadeDurationSeconds();
        QVector<double> getEqualizerBandCenterFrequencies();
        bool            getNetworkStartingLastState();

        void optionsUpdated();
        void requestDecodingCallback();

        void radioTitleCallback(QString title);


    private:

        enum FadeDirection {
            FadeDirectionNone,
            FadeDirectionIn,
            FadeDirectionOut
        };

        static const int  FADE_DURATION_DEFAULT_SECONDS  = 4;
        static const long USEC_PER_SEC                   = 1000 * 1000;
        static const int  DECODING_CB_DELAY_MILLISECONDS = 40;
        static const int  UNDERRUN_DELAY_MILLISECONDS    = 5000;

        struct RadioTitlePosition {
            qint64  microsecondsTimestamp;
            QString title;
        };

        TrackInfo   trackInfo;
        QStringList fadeTags;

        PeakCallback::PeakCallbackInfo         peakCallbackInfo;
        DecodingCallback::DecodingCallbackInfo decodingCallbackInfo;

        QAudioFormat desiredPCMFormat;

        BufferQueue     analyzerQueue;
        TimedChunkQueue equalizerQueue;
        TimedChunkQueue outputQueue;

        QMutex analyzerQueueMutex;
        QMutex equalizerQueueMutex;
        QMutex outputQueueMutex;

        QThread decoderThread;
        QThread cacheThread;
        QThread analyzerThread;
        QThread equalizerThread;
        QThread outputThread;

        DecoderGeneric *decoder;
        PCMCache       *cache;
        Analyzer       *analyzer;
        Equalizer      *equalizer;
        SoundOutput    *soundOutput;

        Status currentStatus;
        bool   stopping;
        bool   decodingDone;
        bool   finishedSent;
        bool   fadeoutStartedSent;
        qint64 decodingInfoLastSent;
        bool   networkStartingLastState;

        int    fadeDurationSeconds;
        qint64 fadeoutStartMilliseconds;
        int    fadeDirection;
        qint64 fadePercent;
        double fadeFrameCount;

        qint64 decodedMillisecondsAtUnderrun;
        qint64 posMillisecondsAtUnderrun;
        qint64 posMilliseconds;

        QVector<RadioTitlePosition> radioTitlePositions;

        void setupDecoder();
        void setupCache();
        void setupAnalyzer();
        void setupEqualizer();
        void setupOutput();

        bool isDoFade();
        void applyFade(QByteArray *chunk);

        void changeStatus(Status status);

        double  decodedPercent();
        QString equalizerSettingsPrefix();


    public slots:

        void requestForBufferReplayGainInfo();


    private slots:

        void bufferAvailableFromDecoder(QAudioBuffer *buffer);
        void pcmChunkFromCache(QByteArray PCM, qint64 startMicroseconds);
        void pcmChunkFromEqualizer(TimedChunk chunk);

        void sendFinished();
        void sendFadeoutStarted();

        void decoderFinished();
        void decoderError(QString info, QString errorMessage);
        void decoderInfo(QString info);
        void decoderSessionExpired();
        void decoderNetworkStarting(bool starting);
        void decoderNetworkBufferChanged();
        void underrunTimeout();

        void cacheError(QString info, QString errorMessage);

        void analyzerReplayGain(double replayGain);

        void equalizerReplayGainChanged(double current);

        void outputPositionChanged(qint64 posMilliseconds);
        void outputNeedChunk();
        void outputBufferUnderrun();
        void outputError(QString errorMessage);


    signals:

        void error(QString id, QString info, QString error);
        void info(QString id, QString info);
        void sessionExpired(QString id);

        void playPosition(QString id, bool decoderFinished, long knownDurationMilliseconds, long positionMilliseconds);
        void replayGainInfo(QString id, double current);
        void decoded(QString id, qint64 length);
        void fadeoutStarted(QString id);
        void finished(QString id);
        void trackInfoUpdated(QString id);
        void statusChanged(QString id, Track::Status status, QString statusString);

        void startDecode();
        void decoderDone();
        void networkConnecting(QString id, bool busy);

        void cacheRequestNextPCMChunk();
        void cacheRequestTimestampPCMChunk(long milliseconds);

        void bufferAvailableToAnalyzer();
        void chunkAvailableToEqualizer(int maxToProcess);
        void chunkAvailableToOutput();

        void playBegins();
        void requestReplayGainInfo();
        void pause();
        void resume();

        void updateReplayGain(double replayGain);
        void resetReplayGain();
};

#endif // TRACK_H
