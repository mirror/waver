#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <QAudioFormat>
#include <QDateTime>
#include <QMutex>
#include <QObject>
#include <QThread>
#include <waveriir/iirfilter.h>
#include <waveriir/iirfilterchain.h>
#include <waveriir/iirfiltercallback.h>

#include "equalizer.h"
#include "pcmcache.h"
#include "preanalyzer.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


class PreProcessor : public QObject, IIRFilterCallback
{
    Q_OBJECT

    public:

        explicit PreProcessor(QAudioFormat format, qint64 length);
        ~PreProcessor();

        void setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex);

        void filterCallback(double *sample, int channelIndex) override;


    private:

        static const int PRE_EQ_BANDS    = 30;
        static const int PRE_EQ_N        = 8;
        static const int WAIT_MS         = 7500;

        static const int WIDE_STEREO_LIGHT_MILLISEC   = 17;
        static const int WIDE_STEREO_REGULAR_MILLISEC = 25;
        static const int WIDE_STEREO_HEAVY_MILLISEC   = 33;

        static constexpr double COMPRESS_LIGHT_MAX           = 29000;   // -0.5dB
        static constexpr double COMPRESS_LIGHT_MIN           = 103;     // -25dB
        static constexpr double COMPRESS_REGULAR_MAX         = 22000;   // -1.75dB
        static constexpr double COMPRESS_REGULAR_MIN         = 325;     // -20dB
        static constexpr double COMPRESS_HEAVY_MAX           = 16500;   // -3dB
        static constexpr double COMPRESS_HEAVY_MIN           = 1036;    // -15dB


        bool   firstPCMChunkRequest;
        bool   decoderFinished;
        qint64 firstBufferReceived;
        qint64 playStartRequested;

        QAudioFormat format;
        qint64       lengthMilliseconds;

        //IIRFilter::SampleTypes sampleType;

        int    dataBytesIndex;
        int    wideStereoDelayMillisec;
        double scaledPeak;
        double compressPosMax;
        double compressPosMin;
        double compressNegMax;
        double compressNegMin;

        BufferQueue *bufferQueue;
        QMutex      *bufferQueueMutex;

        QThread   cacheThread;
        PCMCache *cache;

        QThread      preAnalyzerThread;
        PreAnalyzer *preAnalyzer;

        QThread          equalizerThread;
        Equalizer       *equalizer;
        TimedChunkQueue  equalizerQueue;
        QMutex           equalizerQueueMutex;

        QVector<double> *wideStereoBuffer1;
        QVector<double> *wideStereoBuffer2;
        bool             wideStereoBufferOne;
        int              wideStereoBufferIndex;


    public slots:

        void run();
        void preAnalyzerReady();
        void bufferAvailable();
        void decoderDone();
        void playStartRequest();


    private slots:

        void pcmChunkFromCache(QByteArray PCM, qint64 startMicroseconds);
        void pcmChunkFromEqualizer(TimedChunk chunk);
        void cacheError(QString info, QString errorMessage);
        void preAnalyzerMeasuredGains(QVector<double> gains, double scaledPeak);


    signals:

        void bufferAvailableToPreAnalyzer();
        void decoderDoneToPreAnalyzer();
        void cacheRequestNextPCMChunk();
        void chunkAvailableToEqualizer(int maxToProcess);
        void bufferPreProcessed(QAudioBuffer *buffer);
        void setDecoderDelay(unsigned long microseconds);
};

#endif // PREPROCESSOR_H

/*
    more info on cascading filters
    https://www.earlevel.com/main/2016/09/29/cascading-filters/
    https://www.analog.com/media/en/technical-documentation/application-notes/an27af.pdf
*/
