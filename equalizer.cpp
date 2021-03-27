/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "equalizer.h"


Equalizer::Equalizer(QAudioFormat format)
{
    this->format = format;

    equalizerFilters  = nullptr;
    replayGain        = 0.0;
    currentReplayGain = replayGain;
    sampleRate        = 0;
    sampleType        = IIRFilter::Unknown;
}


Equalizer::~Equalizer()
{
    if (equalizerFilters != nullptr) {
        delete equalizerFilters;
    }
}


void Equalizer::chunkAvailable(int maxToProcess)
{
    int i = 0;
    while ((chunkQueue->count() > 0) && (i < maxToProcess)) {
        TimedChunk chunk = chunkQueue->at(0);

        if (equalizerFilters == nullptr) {
            return;
        }

        filtersMutex.lock();
        equalizerFilters->processPCMData(chunk.chunkPointer->data(), chunk.chunkPointer->size(), sampleType, format.channelCount());
        filtersMutex.unlock();

        chunkQueueMutex->lock();
        chunkQueue->remove(0);
        chunkQueueMutex->unlock();

        emit chunkEqualized(chunk);

        i++;
    }
}


void Equalizer::createFilters()
{
    QList<CoefficientList> coefficientLists;

    for (int i = 0; i < bands.size(); i++) {
        IIRFilter::FilterTypes filterType =
            i == 0               ? IIRFilter::LowShelf  :
            i < bands.size() - 1 ? IIRFilter::BandShelf :
            IIRFilter::HighShelf;

        coefficientLists.append(IIRFilter::calculateBiquadCoefficients(
            filterType,
            bands.at(i).centerFrequency,
            bands.at(i).bandwidth,
            sampleRate,
            gains.at(i)
        ));
    }

    filtersMutex.lock();

    if (equalizerFilters != nullptr) {
        delete equalizerFilters;
    }

    equalizerFilters = new IIRFilterChain(coefficientLists);
    equalizerFilters->getFilter(0)->setCallbackRaw((IIRFilterCallback *)this, (IIRFilterCallback::FilterCallbackPointer)&Equalizer::filterCallback);

    filtersMutex.unlock();
}


void Equalizer::filterCallback(double *sample, int channelIndex)
{
    // replay gain can change as track being analyzed, must be applied in a gradual fashion to avoid sudden falls and spikes in volume
    if ((channelIndex == 0) && (currentReplayGain != replayGain)) {
        double difference = replayGain - currentReplayGain;
        if (qAbs(difference) < 0.05) {
            currentReplayGain = replayGain;
        }
        else {
            double changePerSec = qMin(3.0, qAbs(difference));
            currentReplayGain = currentReplayGain + ((changePerSec / sampleRate) * (difference < 0 ? -1.0 : 1.0));
        }

        emit replayGainChanged(replayGain, currentReplayGain);
    }

    *sample = *sample * pow(10, (currentReplayGain + preAmp) / 20);
}


QVector<double> Equalizer::getBandCenterFrequencies()
{
    QVector<double> returnValue;
    foreach (Band band, bands) {
        returnValue.append(band.centerFrequency);
    }
    return returnValue;
}


void Equalizer::playBegins()
{
    currentReplayGain = replayGain;
}


void Equalizer::requestForReplayGainInfo()
{
    emit replayGainChanged(replayGain, currentReplayGain);
}


void Equalizer::run()
{
    sampleRate = format.sampleRate();
    sampleType = IIRFilter::getSampleTypeFromAudioFormat(format);

    createFilters();
}


void Equalizer::setChunkQueue(TimedChunkQueue *chunkQueue, QMutex *chunkQueueMutex)
{
    this->chunkQueue      = chunkQueue;
    this->chunkQueueMutex = chunkQueueMutex;
}


void Equalizer::setGains(QVector<double> gains, double preAmp)
{
    this->gains.clear();
    this->gains.append(gains);

    this->preAmp = preAmp;

    // minimum 3 maximum 10 bands
    QVector<double> centerFrequencies;
    if (gains.size() <= 3) {
        centerFrequencies = { 62, 750, 5000 };
    }
    else if (gains.size() == 4) {
        centerFrequencies = { 62, 500, 2500, 7500 };
    }
    else if (gains.size() == 5) {
        centerFrequencies = { 62, 250, 750, 2500, 7500 };
    }
    else if (gains.size() == 6) {
        centerFrequencies = { 31, 62, 125, 250, 2500, 7500 };
    }
    else if (gains.size() == 7) {
        centerFrequencies = { 31, 62, 125, 250, 2500, 5000, 12500 };
    }
    else if (gains.size() == 8) {
        centerFrequencies = { 31, 62, 125, 250, 750, 2500, 5000, 12500 };
    }
    else if (gains.size() == 9) {
        centerFrequencies = { 31, 62, 125, 250, 500, 1000, 2500, 5000, 12500 };
    }
    else {
        centerFrequencies = { 31, 62, 125, 250, 500, 1000, 2500, 5000, 10000, 16000 };
    }

    bands.clear();
    bands.append({ centerFrequencies.at(0), centerFrequencies.at(0) / 2 });
    double previousHigh = centerFrequencies.at(0) * 1.25;
    for (int i = 1; i < centerFrequencies.size(); i++) {
        double bandwidth = (centerFrequencies.at(i) - previousHigh) * 2;
        bands.append({ centerFrequencies.at(i), bandwidth });
        previousHigh = centerFrequencies.at(i) + (bandwidth / 2);
    }

    if (sampleRate) {
        createFilters();
    }
}


void Equalizer::setReplayGain(double replayGain)
{
    this->replayGain = replayGain;
    emit replayGainChanged(replayGain, currentReplayGain);
}
