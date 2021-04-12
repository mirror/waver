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

    currentStatus       = Idle;
    stopping            = false;
    decodingDone        = false;
    finishedSent        = false;
    fadeoutStartedSent  = false;
    posMilliseconds     = 0;
    bufferInfoLastSent  = 0;

    fadeDirection            = FadeDirectionNone;
    fadeDurationSeconds      = (trackInfo.attributes.contains("fadeDuration") ? trackInfo.attributes.value("fadeDuration").toInt() : FADE_DURATION_DEFAULT_SECONDS);
    fadeoutStartMilliseconds = getLengthMilliseconds() > 0 ? getLengthMilliseconds() - ((getFadeDurationSeconds() + 1) * 1000) : std::numeric_limits<qint64>::max();

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
        delete soundOutput;
    }

    equalizerThread.requestInterruption();
    equalizerThread.quit();
    equalizerThread.wait();
    if (equalizer != nullptr) {
        delete equalizer;
    }

    analyzerThread.requestInterruption();
    analyzerThread.quit();
    analyzerThread.wait();
    if (analyzer != nullptr) {
        delete analyzer;
    }

    cacheThread.requestInterruption();
    cacheThread.quit();
    cacheThread.wait();
    if (cache != nullptr) {
        delete cache;
    }

    decoderThread.requestInterruption();
    decoderThread.quit();
    decoderThread.wait();
    if (decoder != nullptr) {
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


void Track::bufferAvailableFromDecoder(QAudioBuffer buffer)
{
    cache->storeBuffer(buffer);

    if (QDateTime::currentMSecsSinceEpoch() >= (bufferInfoLastSent + 40)) {
        emit bufferInfo(trackInfo.id, decoder->isFile(), decoder->size(), cache->isFile(), cache->size());
        bufferInfoLastSent = QDateTime::currentMSecsSinceEpoch();
    }

    QAudioBuffer *copy = new QAudioBuffer(QByteArray(static_cast<char *>(buffer.data()), buffer.byteCount()), buffer.format(), buffer.startTime());

    analyzerQueueMutex.lock();
    analyzerQueue.append(copy);
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


void Track::decoderError(QString errorMessage)
{
    emit error(trackInfo.id, tr("Decoder error"), errorMessage);

    if ((currentStatus == Playing) && ((decoder->getDecodedMicroseconds() / 1000 - 1000) > posMilliseconds)) {
        decoderFinished();
        return;
    }

    sendFinished();
}


void Track::decoderFinished()
{
    decodingDone = true;

    emit decoderDone();
    emit bufferInfo(trackInfo.id, decoder->isFile(), decoder->size(), cache->isFile(), cache->size());

    trackInfo.attributes.insert("lengthMilliseconds", decoder->getDecodedMicroseconds() / 1000);
    emit trackInfoUpdated(trackInfo.id);

    fadeoutStartMilliseconds = (decoder->getDecodedMicroseconds() / 1000) - ((getFadeDurationSeconds() + 1) * 1000);

    if (currentStatus == Paused) {
        emit playPosition(trackInfo.id, true, decoder->getDecodedMicroseconds() / 1000, posMilliseconds, decoder->getDecodedMicroseconds() / 1000);
    }
}


void Track::decoderNetworkStarting(bool starting)
{
    emit networkConnecting(trackInfo.id, starting);
}


void Track::decoderRadioTitle(QString title)
{
    radioTitlePositions.append({ decoder->getDecodedMicroseconds() + equalizerQueue.size() * PCMCache::BUFFER_CREATE_MILLISECONDS * 1000 + outputQueue.size() * PCMCache::BUFFER_CREATE_MILLISECONDS * 1000 + soundOutput->remainingMilliseconds() * 1000 - PCMCache::BUFFER_CREATE_MILLISECONDS * 1000, title });
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

        double preAmp = settings.value("eq/pre_amp", DEFAULT_PREAMP).toDouble();

        equalizer->setGains(gains, preAmp);
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
        decoder->setDecodeDelay(delay);
    }

    if (decodingDone && (posMilliseconds >= decoder->getDecodedMicroseconds() / 1000)) {
        sendFinished();
        return;
    }
    if ((posMilliseconds >= fadeoutStartMilliseconds) && (fadeDirection == FadeDirectionOut)) {
        sendFadeoutStarted();
        return;
    }

    if ((radioTitlePositions.size() > 0) && (radioTitlePositions.first().microSecondsTimestamp <= (posMilliseconds * 1000))) {
        trackInfo.title = radioTitlePositions.first().title;
        radioTitlePositions.removeFirst();
        emit trackInfoUpdated(trackInfo.id);
        emit resetReplayGain();
    }
}


void Track::pcmChunkFromCache(QByteArray chunk, qint64 startMicroseconds, bool fromTimestamp)
{
    QByteArray *copy = new QByteArray(chunk.data(), chunk.size());

    equalizerQueueMutex.lock();
    equalizerQueue.append({ copy, startMicroseconds, fromTimestamp });
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

    emit finished(trackInfo.id);
    finishedSent = true;
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
        cacheThread.start();
        decoderThread.start();
        analyzerThread.start();

        emit startDecode();

        changeStatus(Decoding);
        return;
    }

    if ((status == Playing) && (currentStatus == Idle)) {
        cacheThread.start();
        decoderThread.start();
        analyzerThread.start();
        equalizerThread.start();
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
    decoder = new DecoderGeneric();

    decoder->setParameters(trackInfo.url, desiredPCMFormat);
    decoder->setDecodeDelay(1500);
    decoder->moveToThread(&decoderThread);

    connect(&decoderThread, &QThread::started,  decoder, &DecoderGeneric::run);

    connect(decoder, &DecoderGeneric::bufferAvailable, this, &Track::bufferAvailableFromDecoder);
    connect(decoder, &DecoderGeneric::networkStarting, this, &Track::decoderNetworkStarting);
    connect(decoder, &DecoderGeneric::radioTitle,      this, &Track::decoderRadioTitle);
    connect(decoder, &DecoderGeneric::finished,        this, &Track::decoderFinished);
    connect(decoder, &DecoderGeneric::errorMessage,    this, &Track::decoderError);

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

    double preAmp = settings.value("eq/pre_amp", DEFAULT_PREAMP).toDouble();

    equalizer = new Equalizer(desiredPCMFormat);

    equalizer->setChunkQueue(&equalizerQueue, &equalizerQueueMutex);
    equalizer->setGains(gains, preAmp);
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
