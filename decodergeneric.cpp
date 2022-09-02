/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "decodergeneric.h"

DecoderGeneric::DecoderGeneric(RadioTitleCallback::RadioTitleCallbackInfo radioTitleCallbackInfo, QObject *parent) : QObject(parent)
{
    this->radioTitleCallbackInfo = radioTitleCallbackInfo;

    audioDecoder        = nullptr;
    file                = nullptr;
    networkSource       = nullptr;
    networkDeviceSet    = false;
    decodedMicroseconds = 0;
    decodeDelay         = 1500;
    waitUnderBytes      = 4096;

    networkThread.setObjectName("decodernetwork");
}


DecoderGeneric::~DecoderGeneric()
{
    if (audioDecoder != nullptr) {
        audioDecoder->stop();
        audioDecoder->deleteLater();
    }

    networkThread.quit();
    networkThread.wait();

    if (file != nullptr) {
        file->close();
        file->deleteLater();
    }
}


void DecoderGeneric::decoderBufferReady()
{
    // just to be on the safe side (seems like it happens sometimes when there's a decoder error)
    if (!audioDecoder->bufferAvailable()) {
        return;
    }

    QAudioBuffer bufferReady = audioDecoder->read();

    if (!bufferReady.isValid()) {
        return;
    }

    decodedMicroseconds += bufferReady.format().durationForBytes(bufferReady.byteCount());

    QAudioBuffer *copy = new QAudioBuffer(QByteArray(static_cast<const char *>(bufferReady.constData()), bufferReady.byteCount()), bufferReady.format(), bufferReady.startTime());

    emit bufferAvailable(copy);

    if (decodeDelay > 0) {
        QThread::currentThread()->usleep(decodeDelay);
    }

    // must prevent input underrun, reading 0 bytes is show-stopper for the decoder
    if (networkSource != nullptr) {
        waitMutex.lock();
        while ((networkSource->realBytesAvailable() < waitUnderBytes) && !networkSource->isDownloadFinished() && !QThread::currentThread()->isInterruptionRequested()) {
            waitCondition.wait(&waitMutex, 1000);
            emit networkBufferChanged();
        }
        waitMutex.unlock();
    }
}


void DecoderGeneric::decoderError(QAudioDecoder::Error error)
{
    QString errorStr;
    if (audioDecoder != nullptr) {
        errorStr = audioDecoder->errorString();
    }
    if (errorStr.length() == 0) {
        switch (error) {
            case QAudioDecoder::NoError:
                errorStr = tr("No error has occurred.");
                break;
            case QAudioDecoder::ResourceError:
                errorStr = tr("A media resource couldn't be resolved.");
                break;
            case QAudioDecoder::FormatError:
                errorStr = tr("The format of a media resource isn't supported.");
                break;
            case QAudioDecoder::AccessDeniedError:
                errorStr = tr("There are not the appropriate permissions to play a media resource.");
                break;
            case QAudioDecoder::ServiceMissingError:
                errorStr = tr("A valid service was not found, decoding cannot proceed.");
                break;
            default:
                errorStr = tr("Unspecified media decoder error.");
        }
    }

    emit errorMessage(tr("Audio decoder error"), errorStr);

    if (audioDecoder != nullptr) {
        audioDecoder->stop();
    }
}


void DecoderGeneric::decoderFinished()
{
    emit finished();
}


double DecoderGeneric::downloadPercent()
{
    if (isFile() || (networkSource == nullptr)) {
        return 0;
    }

    qint64 total = isRadio ? networkSource->mostRealBytesAvailable() : networkSource->size();
    if (total <= 0) {
        return 0;
    }

    double percent = static_cast<double>(isRadio ? networkSource->realBytesAvailable() : networkSource->downloadedSize()) / total;
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 1) {
        percent = 1;
    }

    return percent;
}


qint64 DecoderGeneric::getDecodedMicroseconds()
{
    return decodedMicroseconds;
}


bool DecoderGeneric::isFile()
{
    return url.isLocalFile();
}


void DecoderGeneric::networkChanged()
{
    emit networkBufferChanged();
}


void DecoderGeneric::networkError(QString errorString)
{
    emit errorMessage(tr("Network error"), errorString);
    emit networkStarting(false);

    if (audioDecoder != nullptr) {
        audioDecoder->stop();
    }
}


void DecoderGeneric::networkSessionExpired()
{
    emit sessionExpired();
}


void DecoderGeneric::networkInfo(QString infoString)
{
    emit infoMessage(infoString);
}


void DecoderGeneric::networkReady()
{
    emit networkStarting(false);

    if (audioDecoder->state() == QAudioDecoder::StoppedState) {
        networkSource->open(QIODevice::ReadOnly);
        audioDecoder->setSourceDevice(networkSource);
        audioDecoder->start();
    }
}


void DecoderGeneric::run()
{
    if (url.isEmpty()) {
        emit errorMessage(tr("Can not decode audio"), tr("Decoder URL is empty"));
    }
}


void DecoderGeneric::setDecodeDelay(unsigned long microseconds)
{
    decodeDelay = microseconds;
}


void DecoderGeneric::setParameters(QUrl url, QAudioFormat decodedFormat, qint64 waitUnderBytes, bool isRadio)
{
    // can be set only once
    if (this->url.isEmpty()) {
        this->url            = url;
        this->decodedFormat  = decodedFormat;
        this->waitUnderBytes = waitUnderBytes;
        this->isRadio        = isRadio;
    }
}


void DecoderGeneric::start()
{
    if (url.isEmpty()) {
        emit errorMessage(tr("Can not decode audio"), tr("Decoder URL is empty"));
        return;
    }

    audioDecoder = new QAudioDecoder();
    audioDecoder->setAudioFormat(decodedFormat);

    connect(audioDecoder, SIGNAL(bufferReady()),               this, SLOT(decoderBufferReady()));
    connect(audioDecoder, SIGNAL(finished()),                  this, SLOT(decoderFinished()));
    connect(audioDecoder, SIGNAL(error(QAudioDecoder::Error)), this, SLOT(decoderError(QAudioDecoder::Error)));

    if (url.isLocalFile()) {
        file = new QFile(url.toLocalFile());
        file->open(QFile::ReadOnly);
        audioDecoder->setSourceDevice(file);
        audioDecoder->start();
    }
    else {
        networkSource = new DecoderGenericNetworkSource(url, &waitCondition, radioTitleCallbackInfo);
        networkSource->setErrorOnUnderrun(false);
        networkSource->moveToThread(&networkThread);

        connect(&networkThread, SIGNAL(started()),  networkSource, SLOT(run()));
        connect(&networkThread, SIGNAL(finished()), networkSource, SLOT(deleteLater()));

        connect(networkSource, SIGNAL(ready()),             this, SLOT(networkReady()));
        connect(networkSource, SIGNAL(changed()),           this, SLOT(networkChanged()));
        connect(networkSource, SIGNAL(error(QString)),      this, SLOT(networkError(QString)));
        connect(networkSource, SIGNAL(info(QString)),       this, SLOT(networkInfo(QString)));
        connect(networkSource, SIGNAL(sessionExpired()),    this, SLOT(networkSessionExpired()));

        emit networkStarting(true);

        networkThread.start();
    }
}

