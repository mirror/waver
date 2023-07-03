/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "outputfeeder.h"

OutputFeeder::OutputFeeder(QByteArray *outputBuffer, QMutex *outputBufferMutex, QAudioFormat audioFormat, QAudioOutput *audioOutput, PeakCallback::PeakCallbackInfo peakCallbackInfo, QObject *parent) : QObject(parent)
{
    this->outputBuffer            = outputBuffer;
    this->outputBufferMutex       = outputBufferMutex;
    this->audioFormat             = audioFormat;
    this->audioOutput             = audioOutput;

    outputDevice               = nullptr;
    wideStereoDelayMillisec    = 0;
    newWideStereoDelayMillisec = 0;

    int16Min    = std::numeric_limits<qint16>::min();
    int16Max    = std::numeric_limits<qint16>::max();
    int16Range  = int16Max - int16Min;
    sampleMin   = 0;
    sampleRange = 0;

    frameCount   = 0;
    this->lPeak  = 0;
    this->rPeak  = 0;
    peakDelaySum = 0;

    this->peakCallbackInfo = peakCallbackInfo;

    channelCount = audioFormat.channelCount();
    channelIndex = 0;

    double sampleMax = 0;
    if (audioFormat.sampleType() == QAudioFormat::SignedInt) {
        switch (audioFormat.sampleSize()) {
            case 8:
                sampleMin = std::numeric_limits<qint8>::min();
                sampleMax = std::numeric_limits<qint8>::max();
                dataType  = 1;
                dataBytes = 1;
                break;
            case 16:
                sampleMin = int16Min;
                sampleMax = int16Max;
                dataType  = 2;
                dataBytes = 2;
                break;
            case 32:
                sampleMin = std::numeric_limits<qint32>::min();
                sampleMax = std::numeric_limits<qint32>::max();
                dataType  = 3;
                dataBytes = 4;
        }
    }
    else if (audioFormat.sampleType() == QAudioFormat::UnSignedInt) {
        switch (audioFormat.sampleSize()) {
            case 8:
                sampleMin = std::numeric_limits<quint8>::min();
                sampleMax = std::numeric_limits<quint8>::max();
                dataType  = 4;
                dataBytes = 1;
                break;
            case 16:
                sampleMin = std::numeric_limits<quint16>::min();
                sampleMax = std::numeric_limits<quint16>::max();
                dataType  = 5;
                dataBytes = 2;
                break;
            case 32:
                sampleMin = std::numeric_limits<quint32>::min();
                sampleMax = std::numeric_limits<quint32>::max();
                dataType  = 6;
                dataBytes = 4;
        }
    }
    if (sampleMax != 0) {
        sampleRange = sampleMax - sampleMin;
    }
}


void OutputFeeder::run()
{
    qint8   *int8;
    qint16  *int16;
    qint32  *int32;
    quint8  *uint8;
    quint16 *uint16;
    quint32 *uint32;

    char   *data;
    int     byteCount;
    double  sampleValue;

    qint64 peakDelayTemp;
    qint64 peakDelay;

    QByteArray wideStereoBuffer1;
    QByteArray wideStereoBuffer2;
    bool       wideStereoBufferOne;
    int        wideStereoBufferIndex;

    peakCallbackInfo.peakFPSMutex->lock();
    int audioFramesPerPeakPeriod = audioFormat.framesForDuration(MICROSECONDS_PER_SECOND / *peakCallbackInfo.peakFPS);
    peakCallbackInfo.peakFPSMutex->unlock();

    int bytesToWrite;
    while (!QThread::currentThread()->isInterruptionRequested()) {
        if (outputDevice == nullptr) {
            QThread::currentThread()->msleep(100);
            continue;
        }

        wideStereoDelayMutex.lock();
        if (wideStereoDelayMillisec != newWideStereoDelayMillisec) {
            if (newWideStereoDelayMillisec == 0) {
                wideStereoBuffer1.resize(0);
                wideStereoBuffer2.resize(0);
            }
            else {
                wideStereoBuffer1.resize(audioFormat.bytesForDuration(newWideStereoDelayMillisec * 1000) / audioFormat.channelCount());
                wideStereoBuffer1.fill(0);
                while (wideStereoBuffer1.size() % dataBytes) {
                    wideStereoBuffer1.append((char)0);
                }
                wideStereoBuffer2.resize(wideStereoBuffer1.size());
                wideStereoBuffer2.fill(0);
            }

            wideStereoDelayMillisec = newWideStereoDelayMillisec;
            wideStereoBufferIndex   = 0;
            wideStereoBufferOne     = true;
        }
        wideStereoDelayMutex.unlock();

        outputBufferMutex->lock();

        bytesToWrite = qMin(audioOutput->bytesFree(), audioOutput->periodSize());
        if (bytesToWrite > outputBuffer->count()) {
            bytesToWrite = outputBuffer->count();
        }

        if (bytesToWrite <= 0) {
            outputBufferMutex->unlock();
            QThread::currentThread()->msleep(10);
            continue;
        }

        if (dataType != 0) {
            byteCount = 0;

            peakDelaySumMutex.lock();
            peakDelayTemp = peakDelaySum;
            peakDelaySumMutex.unlock();

            data = outputBuffer->data();
            while (byteCount < bytesToWrite) {
                if ((channelIndex == 1) && (wideStereoDelayMillisec > 0)) {
                    for (int i = 0; i < dataBytes; i++) {
                        if (wideStereoBufferOne) {
                            wideStereoBuffer2[wideStereoBufferIndex] = *(data + i);
                            *(data + i)                              = wideStereoBuffer1[wideStereoBufferIndex];
                        }
                        else {
                            wideStereoBuffer1[wideStereoBufferIndex] = *(data + i);
                            *(data + i)                              = wideStereoBuffer2[wideStereoBufferIndex];
                        }
                        wideStereoBufferIndex++;
                    }
                    if (wideStereoBufferIndex >= wideStereoBuffer1.size() - 1) {
                        wideStereoBufferOne   = !wideStereoBufferOne;
                        wideStereoBufferIndex = 0;
                    }
                }

                sampleValue = 0;
                switch (dataType) {
                    case 1:
                        int8  = (qint8 *)data;
                        sampleValue = *int8;
                        break;
                    case 2:
                        int16  = (qint16 *)data;
                        sampleValue = *int16;
                        break;
                    case 3:
                        int32  = (qint32 *)data;
                        sampleValue = *int32;
                        break;
                    case 4:
                        uint8  = (quint8 *)data;
                        sampleValue = *uint8;
                        break;
                    case 5:
                        uint16  = (quint16 *)data;
                        sampleValue = *uint16;
                        break;
                    case 6:
                        uint32  = (quint32 *)data;
                        sampleValue = *uint32;
                }
                data      += dataBytes;
                byteCount += dataBytes;

                if (dataType != 2) {
                    sampleValue = (((sampleValue - sampleMin) / sampleRange) * int16Range) + int16Min;
                }

                if (channelIndex == 0) {
                    frameCount++;
                    if (abs(sampleValue) > lPeak) {
                        lPeak = abs(sampleValue);
                    }
                }
                else if (channelIndex == 1) {
                    if (abs(sampleValue) > rPeak) {
                        rPeak = abs(sampleValue);
                    }
                }

                channelIndex++;
                if (channelIndex >= channelCount) {
                    channelIndex = 0;
                }

                if (frameCount == audioFramesPerPeakPeriod) {
                    frameCount = 0;

                    peakCallbackInfo.peakFPSMutex->lock();
                    peakDelayTemp += MICROSECONDS_PER_SECOND / *peakCallbackInfo.peakFPS;
                    peakCallbackInfo.peakFPSMutex->unlock();

                    peakDelay = peakDelayTemp - audioOutput->processedUSecs();

                    (peakCallbackInfo.callbackObject->*peakCallbackInfo.callbackMethod)(lPeak / (int16Range / 2), rPeak / (int16Range / 2), peakDelay < 0 ? 0 : peakDelay, peakCallbackInfo.trackPointer);

                    lPeak = 0;
                    rPeak = 0;

                    peakCallbackInfo.peakFPSMutex->lock();
                    audioFramesPerPeakPeriod = audioFormat.framesForDuration(MICROSECONDS_PER_SECOND / *peakCallbackInfo.peakFPS);
                    peakCallbackInfo.peakFPSMutex->unlock();
                }
            }

            peakDelaySumMutex.lock();
            peakDelaySum = peakDelayTemp;
            peakDelaySumMutex.unlock();
        }

        outputDeviceMutex.lock();
        outputDevice->write(outputBuffer->data(), bytesToWrite);
        outputDeviceMutex.unlock();

        outputBuffer->remove(0, bytesToWrite);

        outputBufferMutex->unlock();

        if (!QThread::currentThread()->isInterruptionRequested()) {
            QThread::currentThread()->usleep(audioFormat.durationForBytes(bytesToWrite) / 10 * 9);
        }
    }
}


void OutputFeeder::setOutputDevice(QIODevice *outputDevice)
{
    outputDeviceMutex.lock();
    this->outputDevice = outputDevice;
    outputDeviceMutex.unlock();

    peakDelaySumMutex.lock();
    peakDelaySum = 0;
    peakDelaySumMutex.unlock();
}


void OutputFeeder::setWideStereoDelayMillisec(int wideStereoDelayMillisec)
{
    if (wideStereoDelayMillisec < 0) {
        wideStereoDelayMillisec = 0;
    }
    else if (wideStereoDelayMillisec > WIDE_STEREO_DELAY_MILLISEC_MAX) {
        wideStereoDelayMillisec = WIDE_STEREO_DELAY_MILLISEC_MAX;
    }

    wideStereoDelayMutex.lock();
    newWideStereoDelayMillisec = wideStereoDelayMillisec;
    wideStereoDelayMutex.unlock();
}
