#include "preprocessor.h"

PreProcessor::PreProcessor(QAudioFormat format, qint64 length) : QObject{nullptr}
{
    preAnalyzerThread.setObjectName("preanalyzer");
    cacheThread.setObjectName("preprocessor_cache");
    equalizerThread.setObjectName("preprocessor_equalizer");

    this->format             = format;
    this->lengthMilliseconds = length;

    firstPCMChunkRequest  = true;
    firstBufferReceived   = 0;
    playStartRequested    = 0;
    preAnalyzer           = nullptr;
    equalizer             = nullptr;
    cache                 = nullptr;
    wideStereoBuffer1     = nullptr;
    wideStereoBuffer2     = nullptr;
    wideStereoBufferOne   = true;
    wideStereoBufferIndex = 0;

    calculateCompressLimit((double)std::numeric_limits<qint16>::max());

    if (1) {   // TODO make this an option: heavy, regular, or light wide stereo
        wideStereoDelayMillisec = WIDE_STEREO_HEAVY_MILLISEC;
    }
    else if (1) {
        wideStereoDelayMillisec = WIDE_STEREO_REGULAR_MILLISEC;
    }
    else {
        wideStereoDelayMillisec = WIDE_STEREO_LIGHT_MILLISEC;
    }
}


PreProcessor::~PreProcessor()
{
    preAnalyzerThread.requestInterruption();
    preAnalyzerThread.quit();
    preAnalyzerThread.wait();
    if (preAnalyzer != nullptr) {
        disconnect(this, &PreProcessor::bufferAvailableToPreAnalyzer, preAnalyzer, &PreAnalyzer::bufferAvailable);
        disconnect(this, &PreProcessor::decoderDoneToPreAnalyzer,     preAnalyzer, &PreAnalyzer::decoderDone);
        disconnect(preAnalyzer, &PreAnalyzer::running,       this, &PreProcessor::preAnalyzerReady);
        disconnect(preAnalyzer, &PreAnalyzer::measuredGains, this, &PreProcessor::preAnalyzerMeasuredGains);
        delete preAnalyzer;
    }

    equalizerThread.requestInterruption();
    equalizerThread.quit();
    equalizerThread.wait();
    if (equalizer != nullptr) {
        disconnect(this,      &PreProcessor::chunkAvailableToEqualizer, equalizer, &Equalizer::chunkAvailable);
        disconnect(equalizer, &Equalizer::chunkEqualized,               this,      &PreProcessor::pcmChunkFromEqualizer);
        delete equalizer;
    }

    cacheThread.requestInterruption();
    cacheThread.quit();
    cacheThread.wait();
    if (cache != nullptr) {
        disconnect(cache, &PCMCache::pcmChunk, this, &PreProcessor::pcmChunkFromCache);
        disconnect(cache, &PCMCache::error,    this, &PreProcessor::cacheError);
        disconnect(this, &PreProcessor::cacheRequestNextPCMChunk,      cache, &PCMCache::requestNextPCMChunk);
        delete cache;
    }

    if (wideStereoBuffer1 != nullptr) {
        delete wideStereoBuffer1;
    }
    if (wideStereoBuffer2 != nullptr) {
        delete wideStereoBuffer2;
    }
}


void PreProcessor::bufferAvailable()
{
    if (firstBufferReceived == 0) {
        firstBufferReceived = QDateTime::currentMSecsSinceEpoch();
    }
    emit bufferAvailableToPreAnalyzer();
}


void PreProcessor::calculateCompressLimit(double scaledPeak)
{
    double int16Max = (double)std::numeric_limits<qint16>::max();

    if (scaledPeak > int16Max) {
        scaledPeak = int16Max;
    }
    if (scaledPeak < 1) {
        scaledPeak = 1;
    }

    double formatMax = std::numeric_limits<double>::max();
    if (format.sampleType() == QAudioFormat::SignedInt) {
        switch (format.sampleSize()) {
        case 8:
            formatMax = std::numeric_limits<qint8>::max();
            break;
        case 16:
            formatMax = std::numeric_limits<qint16>::max();
            break;
        case 32:
            formatMax = std::numeric_limits<qint32>::max();
        }
    }
    else if (format.sampleType() == QAudioFormat::UnSignedInt) {
        switch (format.sampleSize()) {
        case 8:
            formatMax = std::numeric_limits<quint8>::max();
            break;
        case 16:
            formatMax = std::numeric_limits<quint16>::max();
            break;
        case 32:
            formatMax = std::numeric_limits<quint32>::max();
        }
    }

    double peakPercent   = scaledPeak / int16Max;
    double formatPercent = formatMax  / int16Max;

    // scale compress limits to peak value, so all tracks gets compressed the same relative amount
    if (1) {   // TODO make this an option: heavy, regular, or light compressor
        compressTopLimit    = COMPRESS_HEAVY * peakPercent * formatPercent;
        compressBottomLimit = compressTopLimit * -1;
    }
    else if (1) {
        compressTopLimit    = COMPRESS_REGULAR * peakPercent * formatPercent;
        compressBottomLimit = compressTopLimit * -1;
    }
    else {
        compressTopLimit    = COMPRESS_LIGHT * peakPercent * formatPercent;
        compressBottomLimit = compressTopLimit * -1;
    }
}


void PreProcessor::cacheError(QString info, QString errorMessage)
{
    qWarning() << info << errorMessage;
}


void PreProcessor::decoderDone()
{
    decoderFinished      = true;
    firstPCMChunkRequest = false;

    emit decoderDoneToPreAnalyzer();
    emit cacheRequestNextPCMChunk();
}


void PreProcessor::filterCallback(double *sample, int channelIndex)
{
    if (channelIndex == 1) {
        if (wideStereoBufferOne) {
            (*wideStereoBuffer2)[wideStereoBufferIndex] = *sample;
            *sample                                     = wideStereoBuffer1->at(wideStereoBufferIndex);
        }
        else {
            (*wideStereoBuffer1)[wideStereoBufferIndex] = *sample;
            *sample                                     = wideStereoBuffer2->at(wideStereoBufferIndex);
        }
        wideStereoBufferIndex++;

        if (wideStereoBufferIndex >= wideStereoBuffer1->size() - 1) {
            wideStereoBufferOne   = !wideStereoBufferOne;
            wideStereoBufferIndex = 0;
        }
    }

    if (*sample < compressBottomLimit) {
        *sample = compressBottomLimit;
    }
    else if (*sample > compressTopLimit) {
        *sample = compressTopLimit;
    }
}


void PreProcessor::pcmChunkFromCache(QByteArray PCM, qint64 startMicroseconds)
{
    if ((lengthMilliseconds > 0) && playStartRequested && !decoderFinished) {
        unsigned long delay = static_cast<unsigned long>(pow(4, log10(qMax(lengthMilliseconds - startMicroseconds / 1000, 1ll))));
        #ifdef Q_OS_WINDOWS
            delay *= 3;
            if (trackInfo.attributes.contains("radio_station")) {
                delay = qMax(delay, 10000ul);
            }
        #endif
        emit setDecoderDelay(delay);
    }

    QByteArray *copy = new QByteArray(PCM.data(), PCM.size());

    equalizerQueueMutex.lock();
    equalizerQueue.append({ copy, startMicroseconds });
    equalizerQueueMutex.unlock();

    emit chunkAvailableToEqualizer(9999999);
    emit cacheRequestNextPCMChunk();
}


void PreProcessor::pcmChunkFromEqualizer(TimedChunk chunk)
{
    QAudioBuffer *buffer = new QAudioBuffer(*chunk.chunkPointer, format, chunk.startMicroseconds);
    emit bufferPreProcessed(buffer);

    delete chunk.chunkPointer;
}


void PreProcessor::preAnalyzerMeasuredGains(QVector<double> gains, double scaledPeak)
{
    equalizer->setGains(true, gains, 0);
    calculateCompressLimit(scaledPeak);

    if (firstPCMChunkRequest && (QDateTime::currentMSecsSinceEpoch() >= playStartRequested + WAIT_MS) && (QDateTime::currentMSecsSinceEpoch() >= firstBufferReceived + WAIT_MS)) {
        firstPCMChunkRequest = false;
        emit cacheRequestNextPCMChunk();
    }
}


void PreProcessor::preAnalyzerReady()
{
    QVector<double> gains;
    for(int i = 0; i < preAnalyzer->getbandCount(); i++) {
        gains.append(0);
    }

    equalizer = new Equalizer(format, (IIRFilterCallback *)this, (IIRFilterCallback::FilterCallbackPointer)&PreProcessor::filterCallback);

    equalizer->setChunkQueue(&equalizerQueue, &equalizerQueueMutex);
    equalizer->setGains(true, gains, 0);
    equalizer->moveToThread(&equalizerThread);

    connect(&equalizerThread, &QThread::started, equalizer, &Equalizer::run);

    connect(this,      &PreProcessor::chunkAvailableToEqualizer, equalizer, &Equalizer::chunkAvailable);
    connect(equalizer, &Equalizer::chunkEqualized,               this,      &PreProcessor::pcmChunkFromEqualizer);

    equalizerThread.start();
}


void PreProcessor::playStartRequest()
{
    playStartRequested = QDateTime::currentMSecsSinceEpoch();
}


void PreProcessor::run()
{
    int dataBytes = 0;
    if (format.sampleType() == QAudioFormat::SignedInt) {
        switch (format.sampleSize()) {
            case 8:
                dataBytes = 1;
                break;
            case 16:
                dataBytes = 2;
                break;
            case 32:
                dataBytes = 4;
        }
    }
    else if (format.sampleType() == QAudioFormat::UnSignedInt) {
        switch (format.sampleSize()) {
            case 8:
                dataBytes = 1;
                break;
            case 16:
                dataBytes = 2;
                break;
            case 32:
                dataBytes = 4;
        }
    }

    wideStereoBuffer1 = new QVector<double>(format.framesForDuration(wideStereoDelayMillisec * 1000), 0);
    while (wideStereoBuffer1->size() % dataBytes) {
        wideStereoBuffer1->append((char)0);
    }
    wideStereoBuffer2 = new QVector<double>(wideStereoBuffer1->size(), 0);

    cache = new PCMCache(format, 0, true);
    cache->moveToThread(&cacheThread);

    connect(&cacheThread, &QThread::started, cache, &PCMCache::run);

    connect(cache, &PCMCache::pcmChunk,                     this,  &PreProcessor::pcmChunkFromCache);
    connect(cache, &PCMCache::error,                        this,  &PreProcessor::cacheError);
    connect(this,  &PreProcessor::cacheRequestNextPCMChunk, cache, &PCMCache::requestNextPCMChunk);

    preAnalyzer = new PreAnalyzer(format, cache, PRE_EQ_BANDS, PRE_EQ_N);

    connect(&preAnalyzerThread, &QThread::started, preAnalyzer, &PreAnalyzer::run);

    connect(this, &PreProcessor::bufferAvailableToPreAnalyzer, preAnalyzer, &PreAnalyzer::bufferAvailable);
    connect(this, &PreProcessor::decoderDoneToPreAnalyzer,     preAnalyzer, &PreAnalyzer::decoderDone);

    connect(preAnalyzer, &PreAnalyzer::running,       this, &PreProcessor::preAnalyzerReady);
    connect(preAnalyzer, &PreAnalyzer::measuredGains, this, &PreProcessor::preAnalyzerMeasuredGains);

    preAnalyzer->setBufferQueue(bufferQueue, bufferQueueMutex);

    cacheThread.start();
    preAnalyzerThread.start();
}


void PreProcessor::setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex)
{
    this->bufferQueue      = bufferQueue;
    this->bufferQueueMutex = bufferQueueMutex;
}
