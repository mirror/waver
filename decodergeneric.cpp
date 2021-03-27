/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "decodergeneric.h"

DecoderGeneric::DecoderGeneric(QObject *parent) : QObject(parent)
{
    audioDecoder        = nullptr;
    file                = nullptr;
    networkSource       = nullptr;
    networkDeviceSet    = false;
    decodedMicroseconds = 0;
    decodeDelay         = 50;
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

    decodedMicroseconds += bufferReady.format().durationForBytes(bufferReady.byteCount());

    emit bufferAvailable(bufferReady);

    if (decodeDelay > 0) {
        QThread::currentThread()->usleep(decodeDelay);
    }

    // must prevent input underrun, reading 0 bytes is show-stopper for the decoder
    if (networkSource != nullptr) {
        waitMutex.lock();
        while ((networkSource->realBytesAvailable() < 4048) && !networkSource->isFinshed() && !QThread::currentThread()->isInterruptionRequested()) {
            waitCondition.wait(&waitMutex, 1000);
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


qint64 DecoderGeneric::getDecodedMicroseconds()
{
    return decodedMicroseconds;
}


bool DecoderGeneric::isFile()
{
    return url.isLocalFile();
}


void DecoderGeneric::networkError(QString errorString)
{
    emit errorMessage(tr("Network error"), errorString);
    emit networkStarting(false);

    if (audioDecoder != nullptr) {
        audioDecoder->stop();
    }
}


void DecoderGeneric::networkRadioTitle(QString title)
{
    emit radioTitle(title);
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


void DecoderGeneric::setParameters(QUrl url, QAudioFormat decodedFormat)
{
    // can be set only once
    if (this->url.isEmpty()) {
        this->url           = url;
        this->decodedFormat = decodedFormat;
    }
}


qint64 DecoderGeneric::size()
{
    if (url.isLocalFile()) {
        if (file != nullptr) {
            return file->size();
        }
    }
    else {
        if (networkSource != nullptr) {
            return networkSource->realBytesAvailable();
        }
    }
    return 0;
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
        networkSource = new DecoderGenericNetworkSource(url, &waitCondition);
        networkSource->setErrorOnUnderrun(false);
        networkSource->moveToThread(&networkThread);

        connect(&networkThread, SIGNAL(started()),  networkSource, SLOT(run()));
        connect(&networkThread, SIGNAL(finished()), networkSource, SLOT(deleteLater()));

        connect(networkSource, SIGNAL(ready()),             this, SLOT(networkReady()));
        connect(networkSource, SIGNAL(error(QString)),      this, SLOT(networkError(QString)));
        connect(networkSource, SIGNAL(radioTitle(QString)), this, SLOT(networkRadioTitle(QString)));

        emit networkStarting(true);

        networkThread.start();
    }
}
