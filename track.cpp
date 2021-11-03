/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/

#include "track.h"


Track::Track(TrackInfo trackInfo, PeakCallback::PeakCallbackInfo peakCallbackInfo, QObject *parent) : QObject(parent)
{
    decoderThread.setObjectName("decoder");
    analyzerThread.setObjectName("analyzer");
    equalizerThread.setObjectName("equalizer");
    outputThread.setObjectName("output");

    this->trackInfo = trackInfo;

    peakCallbackInfo.trackPointer = static_cast<void *>(this);

    this->peakCallbackInfo = peakCallbackInfo;

    currentStatus            = Idle;
    stopping                 = false;
    decodingDone             = false;
    finishedSent             = false;
    fadeoutStartedSent       = false;
    posMilliseconds          = 0;
    bufferInfoLastSent       = 0;
    networkStartingLastState = false;

    fadeDirection       = FadeDirectionNone;
    fadeDurationSeconds = (trackInfo.attributes.contains("fadeDuration") ? trackInfo.attributes.value("fadeDuration").toInt() : FADE_DURATION_DEFAULT_SECONDS);

    #ifdef Q_OS_WIN
        fadeoutStartMilliseconds = getLengthMilliseconds() > 0 ? getLengthMilliseconds() - ((getFadeDurationSeconds() + 1) * 1000) : 10 * 24 * 60 * 60 * 1000;
    #elif defined (Q_OS_LINUX)
        fadeoutStartMilliseconds = getLengthMilliseconds() > 0 ? getLengthMilliseconds() - ((getFadeDurationSeconds() + 1) * 1000) : std::numeric_limits<qint64>::max();
    #endif


    QSettings settings;
    fadeTags.append(settings.value("options/fade_tags", DEFAULT_FADE_TAGS).toString().split(","));

    desiredPCMFormat.setByteOrder(QSysInfo::ByteOrder == QSysInfo::BigEndian ? QAudioFormat::BigEndian : QAudioFormat::LittleEndian);
    desiredPCMFormat.setChannelCount(2);
    desiredPCMFormat.setCodec("audio/pcm");
    desiredPCMFormat.setSampleRate(44100);
    desiredPCMFormat.setSampleSize(16);
    desiredPCMFormat.setSampleType(QAudioFormat::SignedInt);

    setupDecoder();
    setupCache();
    setupAnalyzer();
    setupEqualizer();
    setupOutput();
}


Track::~Track()
{
    outputThread.requestInterruption();

    outputQueueMutex.lock();
    outputQueue.clear();
    outputQueueMutex.unlock();

    outputThread.quit();
    outputThread.wait();
    if (soundOutput != nullptr) {
        disconnect(soundOutput, &SoundOutput::needChunk,       this, &Track::outputNeedChunk);
        disconnect(soundOutput, &SoundOutput::positionChanged, this, &Track::outputPositionChanged);
        disconnect(soundOutput, &SoundOutput::bufferUnderrun,  this, &Track::outputBufferUnderrun);
        disconnect(soundOutput, &SoundOutput::error,           this, &Track::outputError);

        disconnect(this, &Track::chunkAvailableToOutput, soundOutput, &SoundOutput::chunkAvailable);
        disconnect(this, &Track::pause,                  soundOutput, &SoundOutput::pause);
        disconnect(this, &Track::resume,                 soundOutput, &SoundOutput::resume);

        delete soundOutput;
    }

    equalizerThread.requestInterruption();
    equalizerThread.quit();
    equalizerThread.wait();
    if (equalizer != nullptr) {
        disconnect(equalizer, &Equalizer::chunkEqualized,    this, &Track::pcmChunkFromEqualizer);
        disconnect(equalizer, &Equalizer::replayGainChanged, this, &Track::equalizerReplayGainChanged);

        disconnect(this, &Track::chunkAvailableToEqualizer, equalizer, &Equalizer::chunkAvailable);
        disconnect(this, &Track::updateReplayGain,          equalizer, &Equalizer::setReplayGain);
        disconnect(this, &Track::playBegins,                equalizer, &Equalizer::playBegins);
        disconnect(this, &Track::requestReplayGainInfo,     equalizer, &Equalizer::requestForReplayGainInfo);

        delete equalizer;
    }

    analyzerThread.requestInterruption();
    analyzerThread.quit();
    analyzerThread.wait();
    if (analyzer != nullptr) {
        disconnect(analyzer, &Analyzer::replayGain, this, &Track::analyzerReplayGain);

        disconnect(this, &Track::bufferAvailableToAnalyzer, analyzer, &Analyzer::bufferAvailable);
        disconnect(this, &Track::decoderDone,               analyzer, &Analyzer::decoderDone);
        disconnect(this, &Track::resetReplayGain,           analyzer, &Analyzer::resetReplayGain);

        delete analyzer;
    }

    cacheThread.requestInterruption();
    cacheThread.quit();
    cacheThread.wait();
    if (cache != nullptr) {
        disconnect(cache, &PCMCache::pcmChunk, this, &Track::pcmChunkFromCache);
        disconnect(cache, &PCMCache::error,    this, &Track::cacheError);

        disconnect(this, &Track::cacheRequestNextPCMChunk,      cache, &PCMCache::requestNextPCMChunk);
        disconnect(this, &Track::cacheRequestTimestampPCMChunk, cache, &PCMCache::requestTimestampPCMChunk);

        delete cache;
    }

    decoderThread.requestInterruption();
    decoderThread.quit();
    decoderThread.wait();
    if (decoder != nullptr) {
        disconnect(decoder, &DecoderGeneric::bufferAvailable, this, &Track::bufferAvailableFromDecoder);
        disconnect(decoder, &DecoderGeneric::networkStarting, this, &Track::decoderNetworkStarting);
        disconnect(decoder, &DecoderGeneric::finished,        this, &Track::decoderFinished);
        disconnect(decoder, &DecoderGeneric::errorMessage,    this, &Track::decoderError);

        disconnect(this, &Track::startDecode, decoder, &DecoderGeneric::start);

        delete decoder;
    }
}


void Track::analyzerReplayGain(double replayGain)
{
    emit updateReplayGain(replayGain);
}


void Track::applyFade(QByteArray *chunk)
{
    double framesPerPercent = static_cast<double>(desiredPCMFormat.framesForDuration(fadeDurationSeconds * 1000000)) / 100;
    double framesPerSample  = 1.0 / desiredPCMFormat.channelCount();

    int dataType = 0;
    if (desiredPCMFormat.sampleType() == QAudioFormat::SignedInt) {
        if (desiredPCMFormat.sampleSize() == 8) {
            dataType = 1;
        }
        else if (desiredPCMFormat.sampleSize() == 16) {
            dataType = 2;
        }
        else if (desiredPCMFormat.sampleSize() == 32) {
            dataType = 3;
        }
    }
    else if (desiredPCMFormat.sampleType() == QAudioFormat::UnSignedInt) {
        if (desiredPCMFormat.sampleSize() == 8) {
            dataType = 4;
        }
        else if (desiredPCMFormat.sampleSize() == 16) {
            dataType = 5;
        }
        else if (desiredPCMFormat.sampleSize() == 32) {
            dataType = 6;
        }
    }

    qint8   *int8;
    qint16  *int16;
    qint32  *int32;
    quint8  *uint8;
    quint16 *uint16;
    quint32 *uint32;

    // simple linear fade
    char  *data      = chunk->data();
    int    byteCount = 0;
    while (byteCount < chunk->size()) {
        if (dataType != 0) {
            switch (dataType) {
                case 1:
                    int8  = (qint8 *)data;
                    *int8 = (fadePercent * *int8) / 100;
                    data      += 1;
                    byteCount += 1;
                    break;
                case 2:
                    int16  = (qint16 *)data;
                    *int16 = (fadePercent * *int16) / 100;
                    data      += 2;
                    byteCount += 2;
                    break;
                case 3:
                    int32  = (qint32 *)data;
                    *int32 = (fadePercent * *int32) / 100;
                    data      += 4;
                    byteCount += 4;
                    break;
                case 4:
                    uint8  = (quint8 *)data;
                    *uint8 = (fadePercent * *uint8) / 100;
                    data      += 1;
                    byteCount += 1;
                    break;
                case 5:
                    uint16  = (quint16 *)data;
                    *uint16 = (fadePercent * *uint16) / 100;
                    data      += 2;
                    byteCount += 2;
                    break;
                case 6:
                    uint32  = (quint32 *)data;
                    *uint32 = (fadePercent * *uint32) / 100;
                    data      += 4;
                    byteCount += 4;
            }
        }
        else {
            byteCount += desiredPCMFormat.sampleSize() / 8;
        }

        fadeFrameCount += framesPerSample;

        if (fadeFrameCount >= framesPerPercent) {
            fadeFrameCount = 0;

            if ((fadeDirection == FadeDirectionIn) && (fadePercent < 100)) {
                fadePercent++;

                if (fadePercent == 100) {
                    fadeDirection = FadeDirectionNone;
                }
            }

            if ((fadeDirection == FadeDirectionOut) && (fadePercent > 0)) {
                fadePercent--;

                if (fadePercent == 0) {
                    QTimer::singleShot(soundOutput->remainingMilliseconds() + PCMCache::BUFFER_CREATE_MILLISECONDS, this, SLOT(sendFinished()));
                    break;
                }
            }
        }
    }
}


void Track::attributeAdd(QString key, QVariant value)
{
    trackInfo.attributes.insert(key, value);
}


void Track::attributeRemove(QString key)
{
    trackInfo.attributes.remove(key);
}


void Track::bufferAvailableFromDecoder(QAudioBuffer *buffer)
{
    cache->storeBuffer(buffer);

    if (QDateTime::currentMSecsSinceEpoch() >= (bufferInfoLastSent + 40)) {
        emit bufferInfo(trackInfo.id, decoder->isFile(), decoder->size(), cache->isFile(), cache->size());
        bufferInfoLastSent = QDateTime::currentMSecsSinceEpoch();
    }

    analyzerQueueMutex.lock();
    analyzerQueue.append(buffer);
    analyzerQueueMutex.unlock();

    emit bufferAvailableToAnalyzer();
}


void Track::cacheError(QString info, QString errorMessage)
{
    emit error(trackInfo.id, info, errorMessage);
}


void Track::changeStatus(Status status)
{
    currentStatus = status;
    emit statusChanged(trackInfo.id, currentStatus, getStatusText());
}


void Track::decoderError(QString info, QString errorMessage)
{
    emit error(trackInfo.id, info, errorMessage);

    if ((currentStatus == Playing) && ((decoder->getDecodedMicroseconds() / 1000 - 1000) > posMilliseconds)) {
        decoderFinished();
        return;
    }

    sendFinished();
}


void Track::decoderFinished()
{
    decodingDone = true;

    emit decoded(trackInfo.id, decoder->getDecodedMicroseconds() / 1000);

    emit decoderDone();
    emit bufferInfo(trackInfo.id, decoder->isFile(), decoder->size(), cache->isFile(), cache->size());

    trackInfo.attributes.insert("lengthMilliseconds", decoder->getDecodedMicroseconds() / 1000);
    emit trackInfoUpdated(trackInfo.id);

    fadeoutStartMilliseconds = (decoder->getDecodedMicroseconds() / 1000) - ((getFadeDurationSeconds() + 1) * 1000);

    if (currentStatus == Paused) {
        emit playPosition(trackInfo.id, true, decoder->getDecodedMicroseconds() / 1000, posMilliseconds, decoder->getDecodedMicroseconds() / 1000);
    }
}


void Track::decoderSessionExpired()
{
    emit sessionExpired(trackInfo.id);
}


void Track::decoderInfo(QString info)
{
    emit this->info(trackInfo.id, info);
}


void Track::decoderNetworkStarting(bool starting)
{
    networkStartingLastState = starting;
    emit networkConnecting(trackInfo.id, starting);
}


void Track::decoderNetworkBufferChanged()
{
    if (QDateTime::currentMSecsSinceEpoch() >= (bufferInfoLastSent + 40)) {
        emit bufferInfo(trackInfo.id, decoder->isFile(), decoder->size(), cache->isFile(), cache->size());
        bufferInfoLastSent = QDateTime::currentMSecsSinceEpoch();
    }
}


void Track::equalizerReplayGainChanged(double target, double current)
{
    emit replayGainInfo(trackInfo.id, target, current);
}


qint64 Track::getDecodedMilliseconds()
{
    return decoder->getDecodedMicroseconds() / 1000;
}


QVector<double> Track::getEqualizerBandCenterFrequencies()
{
    if (equalizer == nullptr) {
        return QVector<double>();
    }
    return equalizer->getBandCenterFrequencies();
}


int Track::getFadeDurationSeconds()
{
    return fadeDurationSeconds;
}


qint64 Track::getLengthMilliseconds()
{
    if (decodingDone) {
        return decoder->getDecodedMicroseconds() / 1000;
    }

    if (trackInfo.attributes.contains("lengthMilliseconds")) {
        bool OK                 = false;
        int  lengthMilliseconds = trackInfo.attributes.value("lengthMilliseconds").toInt(&OK);
        if (OK) {
            return lengthMilliseconds;
        }
    }

    return 0;
}


bool Track::getNetworkStartingLastState()
{
    return networkStartingLastState;
}


qint64 Track::getPlayedMillseconds()
{
    return posMilliseconds;
}


Track::Status Track::getStatus()
{
    return currentStatus;
}


QString Track::getStatusText()
{
    switch (currentStatus) {
        case Idle:
            return tr("Idle");
        case Decoding:
            return tr("Decoding");
        case Playing:
            return tr("Playing");
        case Paused:
            return tr("Paused");
    }
    return tr("Stopped");
}


Track::TrackInfo Track::getTrackInfo()
{
    return trackInfo;
}


bool Track::isDoFade()
{
    foreach(QString fadeTag, fadeTags) {
        if (trackInfo.tags.contains(fadeTag, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}


void Track::optionsUpdated()
{
    QSettings settings;

    fadeTags.clear();
    fadeTags.append(settings.value("options/fade_tags", DEFAULT_FADE_TAGS).toString().split(","));

    if (equalizer != nullptr) {
        QVector<double> gains;

        gains.append(settings.value("eq/eq1",  DEFAULT_EQ1).toDouble());
        gains.append(settings.value("eq/eq2",  DEFAULT_EQ2).toDouble());
        gains.append(settings.value("eq/eq3",  DEFAULT_EQ3).toDouble());
        gains.append(settings.value("eq/eq4",  DEFAULT_EQ4).toDouble());
        gains.append(settings.value("eq/eq5",  DEFAULT_EQ5).toDouble());
        gains.append(settings.value("eq/eq6",  DEFAULT_EQ6).toDouble());
        gains.append(settings.value("eq/eq7",  DEFAULT_EQ7).toDouble());
        gains.append(settings.value("eq/eq8",  DEFAULT_EQ8).toDouble());
        gains.append(settings.value("eq/eq9",  DEFAULT_EQ9).toDouble());
        gains.append(settings.value("eq/eq10", DEFAULT_EQ10).toDouble());

        bool   on     = settings.value("eq/on", DEFAULT_EQON).toBool();
        double preAmp = settings.value("eq/pre_amp", DEFAULT_PREAMP).toDouble();

        equalizer->setGains(on, gains, preAmp);
    }
}


void Track::outputNeedChunk()
{
    emit cacheRequestNextPCMChunk();

    if (equalizerQueue.count() > 0) {
        emit chunkAvailableToEqualizer(1);
    }
    if (outputQueue.size() > 0) {
        emit chunkAvailableToOutput();
    }
}


void Track::outputBufferUnderrun()
{
    if (decodingDone && (posMilliseconds >= (decoder->getDecodedMicroseconds() / 1000 - 1000))) {
        sendFinished();
        return;
    }

    decodedMillisecondsAtUnderrun = decoder->getDecodedMicroseconds() / 1000;
    QTimer::singleShot(5000, this, SLOT(underrunTimeout()));
}


void Track::outputError(QString errorMessage)
{
    emit error(trackInfo.id, tr("Sound output error"), errorMessage);
    sendFinished();
}


void Track::outputPositionChanged(qint64 posMilliseconds)
{
    this->posMilliseconds = posMilliseconds;

    emit playPosition(trackInfo.id, decodingDone, getLengthMilliseconds(), posMilliseconds, decoder->getDecodedMicroseconds() / 1000);

    if (!decodingDone) {
        unsigned long delay = static_cast<unsigned long>(pow(4, log10(qMax(decoder->getDecodedMicroseconds() / 1000 - posMilliseconds, 1ll))));
        #ifdef Q_OS_WINDOWS
            delay *= 3;
            if (trackInfo.attributes.contains("radio_station")) {
                delay = qMax(delay, 10000ul);
            }
        #endif
        decoder->setDecodeDelay(delay);
    }

    while ((radioTitlePositions.size() > 0) && (radioTitlePositions.first().microsecondsTimestamp <= posMilliseconds * 1000)) {
        trackInfo.title = radioTitlePositions.first().title;
        radioTitlePositions.removeFirst();
        emit trackInfoUpdated(trackInfo.id);
        emit resetReplayGain();
    }

    if (decodingDone && (posMilliseconds >= decoder->getDecodedMicroseconds() / 1000)) {
        sendFinished();
        return;
    }
    if ((posMilliseconds >= fadeoutStartMilliseconds) && (fadeDirection == FadeDirectionOut)) {
        sendFadeoutStarted();
        return;
    }
}


void Track::pcmChunkFromCache(QByteArray chunk, qint64 startMicroseconds)
{
    QByteArray *copy = new QByteArray(chunk.data(), chunk.size());

    equalizerQueueMutex.lock();
    equalizerQueue.append({ copy, startMicroseconds });
    equalizerQueueMutex.unlock();

    emit chunkAvailableToEqualizer(1);
}


void Track::pcmChunkFromEqualizer(TimedChunk chunk)
{
    if ((chunk.startMicroseconds / 1000 >= fadeoutStartMilliseconds) && (fadeDirection == FadeDirectionNone) && isDoFade()) {
        fadeDirection  = FadeDirectionOut;
        fadePercent    = 100;
        fadeFrameCount = 0;
    }

    if (fadeDirection != FadeDirectionNone) {
        applyFade(chunk.chunkPointer);
    }

    outputQueueMutex.lock();
    outputQueue.append(chunk);
    outputQueueMutex.unlock();

    if (outputThread.isRunning()) {
        emit chunkAvailableToOutput();
    }
}


void Track::radioTitleCallback(QString title)
{
    int decodingCompensation = 2500;
    #if defined (Q_OS_WINDOWS)
        decodingCompensation *= -1;
    #endif

    radioTitlePositions.append({ decoder->getDecodedMicroseconds() + decodingCompensation, title });
}


void Track::requestForBufferReplayGainInfo()
{
    emit bufferInfo(trackInfo.id, decoder->isFile(), decoder->size(), cache->isFile(), cache->size());
    emit requestReplayGainInfo();
}


void Track::sendFadeoutStarted()
{
    if (fadeoutStartedSent) {
        return;
    }

    emit fadeoutStarted(trackInfo.id);
    fadeoutStartedSent = true;
}


void Track::sendFinished()
{
    if (finishedSent) {
        return;
    }
    finishedSent = true;

    emit finished(trackInfo.id);
}


void Track::setPosition(double percent)
{
    if (getStatus() != Playing) {
        return;
    }

    qint64 length = getLengthMilliseconds();
    if (length > 0) {
        double newPosition = percent * length;

        emit pause();
        emit resume();
        emit cacheRequestTimestampPCMChunk(static_cast<long>(newPosition));
    }
}


void Track::setStatus(Status status)
{
    if (status == Idle) {
        if ((currentStatus == Playing) && !stopping) {
            stopping = true;

            fadeDirection       = FadeDirectionOut;
            fadePercent         = 100;
            fadeFrameCount      = 0;
            return;
        }

        if (!stopping) {
            sendFinished();
        }
        return;
    }

    if ((status == Decoding) && (currentStatus == Idle)) {
        analyzerThread.start();
        cacheThread.start();
        decoderThread.start();

        emit startDecode();

        changeStatus(Decoding);
        return;
    }

    if ((status == Playing) && (currentStatus == Idle)) {
        analyzerThread.start();
        equalizerThread.start();
        cacheThread.start();
        decoderThread.start();
        outputThread.start(QThread::HighestPriority);

        emit startDecode();
        emit playBegins();

        if (isDoFade()) {
            fadeDirection       = FadeDirectionIn;
            fadePercent         = 0;
            fadeFrameCount      = 0;
        }

        if (equalizerQueue.size() > 0) {
            emit chunkAvailableToEqualizer(5);
        }
        if (outputQueue.size() > 0) {
            emit chunkAvailableToOutput();
        }

        changeStatus(Playing);

        emit bufferInfo(trackInfo.id, decoder->isFile(), decoder->size(), cache->isFile(), cache->size());

        return;
    }

    if ((status == Playing) && (currentStatus == Decoding)) {
        equalizerThread.start();
        outputThread.start(QThread::HighestPriority);

        emit playBegins();

        if (isDoFade()) {
            fadeDirection       = FadeDirectionIn;
            fadePercent         = 0;
            fadeFrameCount      = 0;
        }

        if (equalizerQueue.size() > 0) {
            emit chunkAvailableToEqualizer(5);
        }
        if (outputQueue.size() > 0) {
            emit chunkAvailableToOutput();
        }

        changeStatus(Playing);
        return;
    }

    if ((status == Paused) && (currentStatus == Playing)) {
        emit pause();

        changeStatus(Paused);
        return;
    }

    if ((status == Playing) && (currentStatus == Paused)) {
        if (decodingDone && (posMilliseconds >= decoder->getDecodedMicroseconds() / 1000 - 15000)) {
            sendFinished();
            return;
        }

        fadeDirection       = FadeDirectionIn;
        fadePercent         = 0;
        fadeFrameCount      = 0;

        emit resume();

        if (equalizerQueue.size() > 0) {
            emit chunkAvailableToEqualizer(5);
        }
        if (outputQueue.size() > 0) {
            emit chunkAvailableToOutput();
        }

        changeStatus(Playing);
        return;
    }
}


void Track::setupAnalyzer()
{
    analyzer = new Analyzer(desiredPCMFormat);

    analyzer->setBufferQueue(&analyzerQueue, &analyzerQueueMutex);
    analyzer->moveToThread(&analyzerThread);

    connect(&analyzerThread, &QThread::started,  analyzer, &Analyzer::run);

    connect(analyzer, &Analyzer::replayGain, this, &Track::analyzerReplayGain);

    connect(this, &Track::bufferAvailableToAnalyzer, analyzer, &Analyzer::bufferAvailable);
    connect(this, &Track::decoderDone,               analyzer, &Analyzer::decoderDone);
    connect(this, &Track::resetReplayGain,           analyzer, &Analyzer::resetReplayGain);
}


void Track::setupCache()
{
    cache = new PCMCache(desiredPCMFormat, getLengthMilliseconds(), trackInfo.attributes.contains("radio_station"));

    cache->moveToThread(&cacheThread);

    connect(&cacheThread, &QThread::started,  cache, &PCMCache::run);

    connect(cache, &PCMCache::pcmChunk, this, &Track::pcmChunkFromCache);
    connect(cache, &PCMCache::error,    this, &Track::cacheError);

    connect(this, &Track::cacheRequestNextPCMChunk,      cache, &PCMCache::requestNextPCMChunk);
    connect(this, &Track::cacheRequestTimestampPCMChunk, cache, &PCMCache::requestTimestampPCMChunk);
}


void Track::setupDecoder()
{
    decoder = new DecoderGeneric({ this, (RadioTitleCallback::RadioTitleCallbackPointer)&Track::radioTitleCallback });

    qint64 waitUnderBytes = 4096;
    #ifdef Q_OS_WINDOWS
        waitUnderBytes = 65536;
    #endif

    decoder->setParameters(trackInfo.url, desiredPCMFormat, waitUnderBytes);
    decoder->setDecodeDelay(1500);
    decoder->moveToThread(&decoderThread);

    connect(&decoderThread, &QThread::started,  decoder, &DecoderGeneric::run);

    connect(decoder, &DecoderGeneric::bufferAvailable,      this, &Track::bufferAvailableFromDecoder);
    connect(decoder, &DecoderGeneric::networkBufferChanged, this, &Track::decoderNetworkBufferChanged);
    connect(decoder, &DecoderGeneric::networkStarting,      this, &Track::decoderNetworkStarting);
    connect(decoder, &DecoderGeneric::finished,             this, &Track::decoderFinished);
    connect(decoder, &DecoderGeneric::errorMessage,         this, &Track::decoderError);
    connect(decoder, &DecoderGeneric::infoMessage,          this, &Track::decoderInfo);
    connect(decoder, &DecoderGeneric::sessionExpired,       this, &Track::decoderSessionExpired);

    connect(this, &Track::startDecode, decoder, &DecoderGeneric::start);
}


void Track::setupEqualizer()
{
    QSettings settings;

    QVector<double> gains;
    gains.append(settings.value("eq/eq1",  DEFAULT_EQ1).toDouble());
    gains.append(settings.value("eq/eq2",  DEFAULT_EQ2).toDouble());
    gains.append(settings.value("eq/eq3",  DEFAULT_EQ3).toDouble());
    gains.append(settings.value("eq/eq4",  DEFAULT_EQ4).toDouble());
    gains.append(settings.value("eq/eq5",  DEFAULT_EQ5).toDouble());
    gains.append(settings.value("eq/eq6",  DEFAULT_EQ6).toDouble());
    gains.append(settings.value("eq/eq7",  DEFAULT_EQ7).toDouble());
    gains.append(settings.value("eq/eq8",  DEFAULT_EQ8).toDouble());
    gains.append(settings.value("eq/eq9",  DEFAULT_EQ9).toDouble());
    gains.append(settings.value("eq/eq10", DEFAULT_EQ10).toDouble());

    bool   on     = settings.value("eq/on", DEFAULT_EQON).toBool();
    double preAmp = settings.value("eq/pre_amp", DEFAULT_PREAMP).toDouble();

    equalizer = new Equalizer(desiredPCMFormat);

    equalizer->setChunkQueue(&equalizerQueue, &equalizerQueueMutex);
    equalizer->setGains(on, gains, preAmp);
    equalizer->moveToThread(&equalizerThread);

    connect(&equalizerThread, &QThread::started,  equalizer, &Equalizer::run);

    connect(equalizer, &Equalizer::chunkEqualized,    this, &Track::pcmChunkFromEqualizer);
    connect(equalizer, &Equalizer::replayGainChanged, this, &Track::equalizerReplayGainChanged);

    connect(this, &Track::chunkAvailableToEqualizer, equalizer, &Equalizer::chunkAvailable);
    connect(this, &Track::updateReplayGain,          equalizer, &Equalizer::setReplayGain);
    connect(this, &Track::playBegins,                equalizer, &Equalizer::playBegins);
    connect(this, &Track::requestReplayGainInfo,     equalizer, &Equalizer::requestForReplayGainInfo);
}


void Track::setupOutput()
{
    soundOutput = new SoundOutput(desiredPCMFormat, peakCallbackInfo);

    soundOutput->setBufferQueue(&outputQueue, &outputQueueMutex);
    soundOutput->moveToThread(&outputThread);

    connect(&outputThread, &QThread::started,  soundOutput, &SoundOutput::run);

    connect(soundOutput, &SoundOutput::needChunk,       this, &Track::outputNeedChunk);
    connect(soundOutput, &SoundOutput::positionChanged, this, &Track::outputPositionChanged);
    connect(soundOutput, &SoundOutput::bufferUnderrun,  this, &Track::outputBufferUnderrun);
    connect(soundOutput, &SoundOutput::error,           this, &Track::outputError);

    connect(this, &Track::chunkAvailableToOutput, soundOutput, &SoundOutput::chunkAvailable);
    connect(this, &Track::pause,                  soundOutput, &SoundOutput::pause);
    connect(this, &Track::resume,                 soundOutput, &SoundOutput::resume);
}


void Track::underrunTimeout()
{
    if (!decodingDone && (decoder != nullptr) && (decodedMillisecondsAtUnderrun >= decoder->getDecodedMicroseconds())) {
        emit error(trackInfo.id, tr("Buffer underrun."), tr("Possible download interruption due to a network error."));
    }
}
