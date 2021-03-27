/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "analyzer.h"

Analyzer::Analyzer(QAudioFormat format, QObject *parent) : QObject(parent)
{
    this->format = format;

    decoderFinished      = false;
    resultLastCalculated = 0;
    replayGainFilter     = nullptr;
    replayGainCalculator = nullptr;
    sampleType           = IIRFilter::Unknown;
}


Analyzer::~Analyzer()
{
    if (replayGainFilter != nullptr) {
        delete replayGainFilter;
    }
    if (replayGainCalculator != nullptr) {
        delete replayGainCalculator;
    }
}


void Analyzer::bufferAvailable()
{
    while (bufferQueue->count() > 0) {
        QAudioBuffer *buffer = bufferQueue->at(0);

        if (replayGainFilter != nullptr) {
            replayGainFilter->processPCMData(buffer->data(), buffer->byteCount(), sampleType, buffer->format().channelCount());

            if ((!decoderFinished && (buffer->startTime() >= resultLastCalculated + REPLAY_GAIN_UPDATE_INTERVAL_MICROSECONDS)) || (decoderFinished && (bufferQueue->count() == 1))) {
                resultLastCalculated = buffer->startTime();
                emit replayGain(replayGainCalculator->calculateResult());
            }
        }

        bufferQueueMutex->lock();
        bufferQueue->remove(0);
        bufferQueueMutex->unlock();

        delete buffer;
    }
}


void Analyzer::decoderDone()
{
    decoderFinished = true;
}


void Analyzer::resetReplayGain()
{
    if (replayGainCalculator != nullptr) {
        replayGainCalculator->reset();
    }
}


void Analyzer::run()
{
    sampleType = IIRFilter::getSampleTypeFromAudioFormat(format);

    if ((sampleType != IIRFilter::Unknown) && QVector<int>({ 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000 }).contains(format.sampleRate())) {
        replayGainFilter     = new IIRFilterChain();
        replayGainCalculator = new ReplayGainCalculator(sampleType, format.sampleRate());

        switch (format.sampleRate()) {
            case 96000:
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_96000_YULEWALK_A, REPLAYGAIN_96000_YULEWALK_B));
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_96000_BUTTERWORTH_A, REPLAYGAIN_96000_BUTTERWORTH_B));
                break;
            case 88200:
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_88200_YULEWALK_A, REPLAYGAIN_88200_YULEWALK_B));
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_88200_BUTTERWORTH_A, REPLAYGAIN_88200_BUTTERWORTH_B));
                break;
            case 64000:
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_64000_YULEWALK_A, REPLAYGAIN_64000_YULEWALK_B));
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_64000_BUTTERWORTH_A, REPLAYGAIN_64000_BUTTERWORTH_B));
                break;
            case 48000:
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_48000_YULEWALK_A, REPLAYGAIN_48000_YULEWALK_B));
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_48000_BUTTERWORTH_A, REPLAYGAIN_48000_BUTTERWORTH_B));
                break;
            case 44100:
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_44100_YULEWALK_A, REPLAYGAIN_44100_YULEWALK_B));
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_44100_BUTTERWORTH_A, REPLAYGAIN_44100_BUTTERWORTH_B));
                break;
            case 32000:
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_32000_YULEWALK_A, REPLAYGAIN_32000_YULEWALK_B));
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_32000_BUTTERWORTH_A, REPLAYGAIN_32000_BUTTERWORTH_B));
                break;
            case 24000:
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_24000_YULEWALK_A, REPLAYGAIN_24000_YULEWALK_B));
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_24000_BUTTERWORTH_A, REPLAYGAIN_24000_BUTTERWORTH_B));
                break;
            case 22050:
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_22050_YULEWALK_A, REPLAYGAIN_22050_YULEWALK_B));
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_22050_BUTTERWORTH_A, REPLAYGAIN_22050_BUTTERWORTH_B));
                break;
            case 16000:
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_16000_YULEWALK_A, REPLAYGAIN_16000_YULEWALK_B));
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_16000_BUTTERWORTH_A, REPLAYGAIN_16000_BUTTERWORTH_B));
                break;
            case 12000:
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_12000_YULEWALK_A, REPLAYGAIN_12000_YULEWALK_B));
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_12000_BUTTERWORTH_A, REPLAYGAIN_12000_BUTTERWORTH_B));
                break;
            case 11025:
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_11025_YULEWALK_A, REPLAYGAIN_11025_YULEWALK_B));
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_11025_BUTTERWORTH_A, REPLAYGAIN_11025_BUTTERWORTH_B));
                break;
            case 8000:
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_8000_YULEWALK_A, REPLAYGAIN_8000_YULEWALK_B));
                replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_8000_BUTTERWORTH_A, REPLAYGAIN_8000_BUTTERWORTH_B));
                break;
        }
        replayGainFilter->getFilter(1)->setCallbackFiltered((IIRFilterCallback *)replayGainCalculator, (IIRFilterCallback::FilterCallbackPointer)&ReplayGainCalculator::filterCallback);
    }
}


void Analyzer::setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex)
{
    this->bufferQueue      = bufferQueue;
    this->bufferQueueMutex = bufferQueueMutex;
}
