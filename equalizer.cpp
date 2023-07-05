/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "equalizer.h"


Equalizer::Equalizer(QAudioFormat format)
{
    this->format                 = format;

    on                = true;
    equalizerFilters  = nullptr;
    replayGain        = 0.0;
    currentReplayGain = replayGain;
    sampleRate        = 0;
    sampleType        = IIRFilter::Unknown;

    eqOffMinValue       = std::numeric_limits<qint16>::min();
    eqOffMaxValue       = std::numeric_limits<qint16>::max();
    eqOffCurrentChannel = 0;
}


Equalizer::~Equalizer()
{
    if (equalizerFilters != nullptr) {
        delete equalizerFilters;
    }
}


Equalizer::Bands Equalizer::calculateBands(QVector<double> centerFrequencies)
{
    Bands bands;

    if (centerFrequencies.size() < 1) {
        return bands;
    }

    bands.append({ centerFrequencies.at(0), centerFrequencies.at(0) / 2 });
    double previousHigh = centerFrequencies.at(0) * 1.25;
    for (int i = 1; i < centerFrequencies.size(); i++) {
        double bandwidth = (centerFrequencies.at(i) - previousHigh) * 2;
        bands.append({ centerFrequencies.at(i), bandwidth });
        previousHigh = centerFrequencies.at(i) + (bandwidth / 2);
    }

    return bands;
}


Equalizer::Bands Equalizer::calculateBands(std::initializer_list<double> centerFrequencies)
{
    return calculateBands(QVector<double>(centerFrequencies));
}


void Equalizer::chunkAvailable(int maxToProcess)
{
    int i = 0;
    while ((chunkQueue->count() > 0) && (i < maxToProcess)) {
        TimedChunk chunk = chunkQueue->at(0);

        if (equalizerFilters == nullptr) {
            return;
        }

        if (on) {
            filtersMutex.lock();
            equalizerFilters->processPCMData(chunk.chunkPointer->data(), chunk.chunkPointer->size(), sampleType, format.channelCount());
            filtersMutex.unlock();
        }
        else {
            // eq is off, but replay gain still must be applied
            double  sample;
            qint16 *temp;
            int     processedCount = 0;
            while (processedCount < chunk.chunkPointer->size()) {
                temp   = (qint16*)((char *)chunk.chunkPointer->data() + processedCount);
                sample = *temp;

                filterCallback(&sample, eqOffCurrentChannel);
                if (sample < eqOffMinValue) {
                    sample = eqOffMinValue;
                }
                if (sample > eqOffMaxValue) {
                    sample = eqOffMaxValue;
                }

                *temp = static_cast<qint16>(sample);

                processedCount += sizeof(qint16);
                eqOffCurrentChannel++;
                if (eqOffCurrentChannel >= 2) {
                    eqOffCurrentChannel = 0;
                }
            }
        }

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

        emit replayGainChanged(currentReplayGain);
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
    emit replayGainChanged(currentReplayGain);
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


void Equalizer::setGains(bool on, QVector<double> gains, double preAmp)
{
    this->gains.clear();
    this->gains.append(gains);

    this->on     = on;
    this->preAmp = preAmp;

    // minimum 3 maximum 10 bands
    bands.clear();
    if (gains.size() <= 3) {
        bands.append(calculateBands(BANDS_3));
    }
    else if (gains.size() == 4) {
        bands.append(calculateBands(BANDS_4));
    }
    else if (gains.size() == 5) {
        bands.append(calculateBands(BANDS_5));
    }
    else if (gains.size() == 6) {
        bands.append(calculateBands(BANDS_6));
    }
    else if (gains.size() == 7) {
        bands.append(calculateBands(BANDS_7));
    }
    else if (gains.size() == 8) {
        bands.append(calculateBands(BANDS_8));
    }
    else if (gains.size() == 9) {
        bands.append(calculateBands(BANDS_9));
    }
    else {
        bands.append(calculateBands(BANDS_10));
    }

    if (sampleRate) {
        createFilters();
    }
}


void Equalizer::setReplayGain(double replayGain)
{
    this->replayGain = replayGain;
}
