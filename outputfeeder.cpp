/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "outputfeeder.h"

OutputFeeder::OutputFeeder(QByteArray *outputBuffer, QMutex *outputBufferMutex, QAudioFormat audioFormat, QAudioOutput *audioOutput, PeakCallback::PeakCallbackInfo peakCallbackInfo, QObject *parent) : QObject(parent)
{
    this->outputBuffer      = outputBuffer;
    this->outputBufferMutex = outputBufferMutex;
    this->audioFormat       = audioFormat;
    this->audioOutput       = audioOutput;

    outputDevice = nullptr;

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
                break;
            case 16:
                sampleMin = int16Min;
                sampleMax = int16Max;
                dataType  = 2;
                break;
            case 32:
                sampleMin = std::numeric_limits<qint32>::min();
                sampleMax = std::numeric_limits<qint32>::max();
                dataType  = 3;
        }
    }
    else if (audioFormat.sampleType() == QAudioFormat::UnSignedInt) {
        switch (audioFormat.sampleSize()) {
            case 8:
                sampleMin = std::numeric_limits<quint8>::min();
                sampleMax = std::numeric_limits<quint8>::max();
                dataType  = 4;
                break;
            case 16:
                sampleMin = std::numeric_limits<quint16>::min();
                sampleMax = std::numeric_limits<quint16>::max();
                dataType  = 5;
                break;
            case 32:
                sampleMin = std::numeric_limits<quint32>::min();
                sampleMax = std::numeric_limits<quint32>::max();
                dataType  = 6;
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

    peakCallbackInfo.peakFPSMutex->lock();
    int audioFramesPerPeakPeriod = audioFormat.framesForDuration(MICROSECONDS_PER_SECOND / *peakCallbackInfo.peakFPS);
    peakCallbackInfo.peakFPSMutex->unlock();

    int bytesToWrite;
    while (!QThread::currentThread()->isInterruptionRequested()) {
        if (outputDevice == nullptr) {
            QThread::currentThread()->msleep(100);
            continue;
        }

        bytesToWrite = qMin(audioOutput->bytesFree(), audioOutput->periodSize());

        if (bytesToWrite <= outputBuffer->count()) {
            if (dataType != 0) {
                data      = outputBuffer->data();
                byteCount = 0;

                peakDelaySumMutex.lock();
                peakDelayTemp = peakDelaySum;
                peakDelaySumMutex.unlock();

                while (byteCount < bytesToWrite) {
                    sampleValue = 0;

                    switch (dataType) {
                        case 1:
                            int8  = (qint8 *)data;
                            sampleValue = *int8;
                            data      += 1;
                            byteCount += 1;
                            break;
                        case 2:
                            int16  = (qint16 *)data;
                            sampleValue = *int16;
                            data      += 2;
                            byteCount += 2;
                            break;
                        case 3:
                            int32  = (qint32 *)data;
                            sampleValue = *int32;
                            data      += 4;
                            byteCount += 4;
                            break;
                        case 4:
                            uint8  = (quint8 *)data;
                            sampleValue = *uint8;
                            data      += 1;
                            byteCount += 1;
                            break;
                        case 5:
                            uint16  = (quint16 *)data;
                            sampleValue = *uint16;
                            data      += 2;
                            byteCount += 2;
                            break;
                        case 6:
                            uint32  = (quint32 *)data;
                            sampleValue = *uint32;
                            data      += 4;
                            byteCount += 4;
                    }

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
            outputBufferMutex->lock();
            outputDevice->write(outputBuffer->data(), bytesToWrite);
            outputBuffer->remove(0, bytesToWrite);
            outputBufferMutex->unlock();
            outputDeviceMutex.unlock();
        }

        if (!QThread::currentThread()->isInterruptionRequested()) {
            if (bytesToWrite > 0) {
                QThread::currentThread()->usleep(audioFormat.durationForBytes(bytesToWrite) / 4 * 3);
            }
            else {
                QThread::currentThread()->msleep(50);
            }
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
