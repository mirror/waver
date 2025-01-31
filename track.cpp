/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/

#include "track.h"


Track::Track(TrackInfo trackInfo, PeakCallback::PeakCallbackInfo peakCallbackInfo, DecodingCallback::DecodingCallbackInfo decodingCallbackInfo, QObject *parent) : QObject(parent)
{
    decoderThread.setObjectName("decoder");
    cacheThread.setObjectName("cache");
    analyzerThread.setObjectName("analyzer");
    equalizerThread.setObjectName("equalizer");

    this->trackInfo = trackInfo;

    peakCallbackInfo.trackPointer     = static_cast<void *>(this);
    decodingCallbackInfo.trackPointer = static_cast<void *>(this);

    this->peakCallbackInfo     = peakCallbackInfo;
    this->decodingCallbackInfo = decodingCallbackInfo;

    currentStatus            = Idle;
    stopping                 = false;
    decodingDone             = false;
    finishedSent             = false;
    fadeoutStartedSent       = false;
    posMilliseconds          = 0;
    decodingInfoLastSent     = 0;
    networkStartingLastState = false;

    QSettings settings;

    fadeDirection       = FadeDirectionNone;
    shortFadeBeginning  = false;
    shortFadeEnd        = false;

    updateFadeoutStartMilliseconds();

    fadeTags.append(settings.value("options/fade_tags", DEFAULT_FADE_TAGS).toString().split(","));
    skipLongSilence             = settings.value("options/skip_long_silence", DEFAULT_SKIP_LONG_SILENCE).toBool();
    skipLongSilenceMicroseconds = settings.value("options/skip_long_silence_seconds", DEFAULT_SKIP_LONG_SILENCE_SECONDS).toInt() * USEC_PER_SEC;

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
        disconnect(analyzer, &Analyzer::silences,   this, &Track::analyzerSilences);

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


void Track::analyzerSilences(ReplayGainCalculator::Silences silences)
{
    this->silences = silences;
    updateFadeoutStartMilliseconds();
}


void Track::applyFade(QByteArray *chunk)
{
    double framesPerPercent = static_cast<double>(desiredPCMFormat.framesForDuration(getFadeDurationSeconds(fadeDirection) * 1000000)) / 100;
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


void Track::artistInfoAdd(QString summary, QString art)
{
    trackInfo.artistSummary = summary;

    if (!art.isEmpty()) {
        QUrl artUrl = QUrl(art);
        if (!trackInfo.arts.contains(artUrl)) {
            trackInfo.arts.append(artUrl);
        }
    }

    emit trackInfoUpdated(trackInfo.id);
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
    if (QDateTime::currentMSecsSinceEpoch() >= (decodingInfoLastSent + DECODING_CB_DELAY_MILLISECONDS)) {
        (decodingCallbackInfo.callbackObject->*decodingCallbackInfo.callbackMethod)(decoder->downloadPercent(), decodedPercent(), this);
        decodingInfoLastSent = QDateTime::currentMSecsSinceEpoch();
    }

    cache->storeBuffer(buffer);

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


double Track::decodedPercent()
{
    bool isRadio = trackInfo.attributes.contains("radio_station");

    qint64 total = isRadio ? cache->mostSize() : getLengthMilliseconds();
    if (total <= 0) {
        // -1 indicates indeterminate progress
        return -1;
    }

    double percent = static_cast<double>(isRadio ? cache->size() : getDecodedMilliseconds()) / total;
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 1) {
        percent = 1;
    }

    return percent;
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
    (decodingCallbackInfo.callbackObject->*decodingCallbackInfo.callbackMethod)(decoder->downloadPercent(), decodedPercent(), this);

    trackInfo.attributes.insert("lengthMilliseconds", decoder->getDecodedMicroseconds() / 1000);
    emit trackInfoUpdated(trackInfo.id);

    if ((silences.count() == 0) || (silences.last().type != ReplayGainCalculator::SilenceAtEnd)) {
        QTimer::singleShot(100, this, &Track::requestSilencesUpdate);
    }

    if (currentStatus == Paused) {
        emit playPosition(trackInfo.id, true, decoder->getDecodedMicroseconds() / 1000, posMilliseconds);
    }

    updateFadeoutStartMilliseconds();
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
    if (QDateTime::currentMSecsSinceEpoch() >= (decodingInfoLastSent + DECODING_CB_DELAY_MILLISECONDS)) {
        (decodingCallbackInfo.callbackObject->*decodingCallbackInfo.callbackMethod)(decoder->downloadPercent(), decodedPercent(), this);
        decodingInfoLastSent = QDateTime::currentMSecsSinceEpoch();
    }
}


QString Track::equalizerSettingsPrefix()
{
    QString prefix = "eq";
    if (trackInfo.attributes.contains("serverSettingsId")) {
        QSettings settings;

        QString settingsId = trackInfo.attributes.value("serverSettingsId").toString();
        QString trackId    = trackInfo.id.split("|").at(0).mid(1);

        if (settings.contains(QString("%1/track/%2/eq/pre_amp").arg(settingsId, trackId))) {
            prefix = QString("%1/track/%2/eq").arg(settingsId, trackId);
        }
        else if (settings.contains(QString("%1/album/%2/eq/pre_amp").arg(settingsId, trackInfo.albumId))) {
            prefix = QString("%1/album/%2/eq").arg(settingsId, trackInfo.albumId);
        }
    }
    return prefix;
}


void Track::equalizerReplayGainChanged(double current)
{
    emit replayGainInfo(trackInfo.id, current);
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


int Track::getFadeDurationSeconds(FadeDirection fadeDirection)
{
    if ((fadeDirection == FadeDirectionIn) && shortFadeBeginning) {
        return SHORT_FADE_SECONDS;
    }
    if ((fadeDirection == FadeDirectionOut) && shortFadeEnd) {
        return SHORT_FADE_SECONDS;
    }

    QSettings settings;
    return settings.value("options/fade_seconds", DEFAULT_FADE_SECONDS).toInt();
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
    if (fadeTags.contains("*")) {
        return true;
    }

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

    updateFadeoutStartMilliseconds();

    skipLongSilence             = settings.value("options/skip_long_silence", DEFAULT_SKIP_LONG_SILENCE).toBool();
    skipLongSilenceMicroseconds = settings.value("options/skip_long_silence_seconds", DEFAULT_SKIP_LONG_SILENCE_SECONDS).toInt() * USEC_PER_SEC;

    if (equalizer != nullptr) {
        QVector<double> gains;

        QString prefix = equalizerSettingsPrefix();

        gains.append(settings.value(prefix + "/eq1",  DEFAULT_EQ1).toDouble());
        gains.append(settings.value(prefix + "/eq2",  DEFAULT_EQ2).toDouble());
        gains.append(settings.value(prefix + "/eq3",  DEFAULT_EQ3).toDouble());
        gains.append(settings.value(prefix + "/eq4",  DEFAULT_EQ4).toDouble());
        gains.append(settings.value(prefix + "/eq5",  DEFAULT_EQ5).toDouble());
        gains.append(settings.value(prefix + "/eq6",  DEFAULT_EQ6).toDouble());
        gains.append(settings.value(prefix + "/eq7",  DEFAULT_EQ7).toDouble());
        gains.append(settings.value(prefix + "/eq8",  DEFAULT_EQ8).toDouble());
        gains.append(settings.value(prefix + "/eq9",  DEFAULT_EQ9).toDouble());
        gains.append(settings.value(prefix + "/eq10", DEFAULT_EQ10).toDouble());

        bool   on     = settings.value("eq/on", DEFAULT_EQON).toBool();
        double preAmp = settings.value(prefix + "/pre_amp", DEFAULT_PREAMP).toDouble();

        equalizer->setGains(on, gains, preAmp);
    }

    if (soundOutput != nullptr) {
        soundOutput->wideStereoDelayChanged(settings.value("options/wide_stereo_delay_millisec").toInt());
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

    emit info(trackInfo.id, tr("Buffer underrun, waiting..."));
    decodedMillisecondsAtUnderrun = decoder->getDecodedMicroseconds() / 1000;
    posMillisecondsAtUnderrun = posMilliseconds;
    QTimer::singleShot(UNDERRUN_DELAY_MILLISECONDS, this, SLOT(underrunTimeout()));
}


void Track::outputError(QString errorMessage)
{
    emit error(trackInfo.id, tr("Sound output error"), errorMessage);
    sendFinished();
}


void Track::outputPositionChanged(qint64 posMilliseconds)
{
    this->posMilliseconds     = posMilliseconds;
    posMillisecondsAtUnderrun = 0;

    emit playPosition(trackInfo.id, decodingDone, getLengthMilliseconds(), posMilliseconds);

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

    if (decodingDone && (posMilliseconds >= decoder->getDecodedMicroseconds() / 1000 - 50)) {
        sendFinished();
        return;
    }
    if ((posMilliseconds >= fadeoutStartMilliseconds) && (fadeDirection == FadeDirectionOut)) {
        sendFadeoutStarted();
        return;
    }
    if (skipLongSilence && !trackInfo.attributes.contains("radio_station")) {
        foreach(ReplayGainCalculator::SilenceRange silence, silences) {
                if ((silence.type == ReplayGainCalculator::SilenceIntermediate) && (silence.endMicroseconds - silence.startMicroseconds >= skipLongSilenceMicroseconds) && (posMilliseconds >= silence.startMicroseconds / 1000 + 2000) && (posMilliseconds <= silence.endMicroseconds / 1000)) {
                setPosition(silence.endMicroseconds);
                break;
            }
        }
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
    if ((fadeDirection == FadeDirectionNone) && isDoFade() && ((chunk.startMicroseconds + desiredPCMFormat.durationForBytes(chunk.chunkPointer->size())) / 1000 >= fadeoutStartMilliseconds)) {
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
    (decodingCallbackInfo.callbackObject->*decodingCallbackInfo.callbackMethod)(decoder->downloadPercent(), decodedPercent(), this);
    emit requestReplayGainInfo();
}


void Track::requestDecodingCallback()
{
    double dlp = decoder->downloadPercent();
    double dcp = decodedPercent();

    if ((dlp > 0) || (dcp > 0)) {
        (decodingCallbackInfo.callbackObject->*decodingCallbackInfo.callbackMethod)(dlp, dcp, this);
    }
}


void Track::requestSilencesUpdate()
{
    if (soundOutput == nullptr) {
        return;
    }
    if ((silences.count() > 0) && (silences.last().type == ReplayGainCalculator::SilenceAtEnd)) {
        return;
    }

    emit requestSilencesFromAnalyzer(true);
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


void Track::setPosition(qint64 microSecond)
{
    if (getStatus() != Playing) {
        return;
    }

    emit pause();
    emit resume();
    emit cacheRequestTimestampPCMChunk(microSecond / 1000);
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


void Track::setShortFadeBeginning(bool shortFade)
{
    shortFadeBeginning = shortFade;
}


void Track::setShortFadeEnd(bool shortFade)
{
    shortFadeEnd = shortFade;

    if ((silences.count() > 0) && (silences.last().type == ReplayGainCalculator::SilenceAtEnd)) {
        fadeoutStartMilliseconds = (silences.last().startMicroseconds / 1000) - ((getFadeDurationSeconds(FadeDirectionOut) + 1) * 1000);
    }
    else {
        #ifdef Q_OS_WIN
            fadeoutStartMilliseconds = getLengthMilliseconds() > 0 ? getLengthMilliseconds() - ((getFadeDurationSeconds(FadeDirectionOut) + 1) * 1000) : 10 * 24 * 60 * 60 * 1000;
        #elif defined (Q_OS_LINUX)
            fadeoutStartMilliseconds = getLengthMilliseconds() > 0 ? getLengthMilliseconds() - ((getFadeDurationSeconds(FadeDirectionOut) + 1) * 1000) : std::numeric_limits<qint64>::max();
        #endif
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

        (decodingCallbackInfo.callbackObject->*decodingCallbackInfo.callbackMethod)(decoder->downloadPercent(), decodedPercent(), this);

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
        if (decodingDone && (posMilliseconds >= decoder->getDecodedMicroseconds() / 1000 - 2500)) {
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
    connect(analyzer, &Analyzer::silences,   this, &Track::analyzerSilences);

    connect(this, &Track::bufferAvailableToAnalyzer,   analyzer, &Analyzer::bufferAvailable);
    connect(this, &Track::decoderDone,                 analyzer, &Analyzer::decoderDone);
    connect(this, &Track::resetReplayGain,             analyzer, &Analyzer::resetReplayGain);
    connect(this, &Track::requestSilencesFromAnalyzer, analyzer, &Analyzer::silencesRequested);
}


void Track::setupCache()
{
    cache = new PCMCache(desiredPCMFormat, getLengthMilliseconds(), trackInfo.attributes.contains("radio_station"));

    cache->moveToThread(&cacheThread);

    connect(&cacheThread, &QThread::started, cache, &PCMCache::run);

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

    decoder->setParameters(trackInfo.url, desiredPCMFormat, waitUnderBytes, trackInfo.attributes.contains("radio_station"), !trackInfo.attributes.contains("radio_station"));
    decoder->moveToThread(&decoderThread);

    connect(&decoderThread, &QThread::started, decoder, &DecoderGeneric::run);

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
    QSettings       settings;
    QVector<double> gains;

    QString prefix = equalizerSettingsPrefix();

    gains.append(settings.value(prefix + "/eq1",  DEFAULT_EQ1).toDouble());
    gains.append(settings.value(prefix + "/eq2",  DEFAULT_EQ2).toDouble());
    gains.append(settings.value(prefix + "/eq3",  DEFAULT_EQ3).toDouble());
    gains.append(settings.value(prefix + "/eq4",  DEFAULT_EQ4).toDouble());
    gains.append(settings.value(prefix + "/eq5",  DEFAULT_EQ5).toDouble());
    gains.append(settings.value(prefix + "/eq6",  DEFAULT_EQ6).toDouble());
    gains.append(settings.value(prefix + "/eq7",  DEFAULT_EQ7).toDouble());
    gains.append(settings.value(prefix + "/eq8",  DEFAULT_EQ8).toDouble());
    gains.append(settings.value(prefix + "/eq9",  DEFAULT_EQ9).toDouble());
    gains.append(settings.value(prefix + "/eq10", DEFAULT_EQ10).toDouble());

    bool   on     = settings.value("eq/on", DEFAULT_EQON).toBool();
    double preAmp = settings.value(prefix + "/pre_amp", DEFAULT_PREAMP).toDouble();

    equalizer = new Equalizer(desiredPCMFormat);

    equalizer->setChunkQueue(&equalizerQueue, &equalizerQueueMutex);
    equalizer->setGains(on, gains, preAmp);
    equalizer->moveToThread(&equalizerThread);

    connect(&equalizerThread, &QThread::started, equalizer, &Equalizer::run);

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

    connect(&outputThread, &QThread::started, soundOutput, &SoundOutput::run);

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
    if ((!decodingDone && (decoder != nullptr) && (decodedMillisecondsAtUnderrun >= decoder->getDecodedMicroseconds() / 1000)) || (decodingDone && (posMilliseconds == posMillisecondsAtUnderrun))) {
        emit error(trackInfo.id, tr("Buffer underrun."), tr("Possible download interruption due to a network error."));
        sendFinished();
    }
}


void Track::updateFadeoutStartMilliseconds()
{
    if ((silences.count() > 0) && (silences.last().type == ReplayGainCalculator::SilenceAtEnd)) {
        fadeoutStartMilliseconds = (silences.last().startMicroseconds / 1000) - ((getFadeDurationSeconds(FadeDirectionOut) + 1) * 1000);
    }
    else {
        #ifdef Q_OS_WIN
            fadeoutStartMilliseconds = getLengthMilliseconds() > 0 ? getLengthMilliseconds() - ((getFadeDurationSeconds(FadeDirectionOut) + 1) * 1000) : 10 * 24 * 60 * 60 * 1000;
        #elif defined (Q_OS_LINUX)
            fadeoutStartMilliseconds = getLengthMilliseconds() > 0 ? getLengthMilliseconds() - ((getFadeDurationSeconds(FadeDirectionOut) + 1) * 1000) : std::numeric_limits<qint64>::max();
        #endif
    }
}
