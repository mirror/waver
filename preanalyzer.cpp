#include "preanalyzer.h"

PreAnalyzer::PreAnalyzer(QAudioFormat format, PCMCache *cache, int bandCount, int N)
{
    if (bandCount < 3) {
        bandCount = 3;
    }
    if (bandCount > 10) {
        bandCount = 10;
    }

    if (N % 2) {
        N++;
    }

    this->format    = format;
    this->cache     = cache;
    this->bandCount = bandCount;
    this->N         = N;

    absolutePeak         = 0.0;
    measurementFilters   = nullptr;
    resultLastCalculated = 0;
}


PreAnalyzer::~PreAnalyzer()
{
    if (measurementFilters != nullptr) {
        delete measurementFilters;
    }
    foreach (ReplayGainCalculator *replayGainCalculator, replayGainCalculators) {
        delete replayGainCalculator;
    }
}


void PreAnalyzer::bufferAvailable()
{
    QThread *currentThread = QThread::currentThread();

    while (bufferQueue->count() > 0) {
        if (currentThread->isInterruptionRequested()) {
            break;
        }

        QAudioBuffer *buffer = bufferQueue->at(0);

        measurementFilters->processPCMData(buffer->data(), buffer->byteCount(), sampleType, buffer->format().channelCount());

        if ((!decoderFinished && (buffer->startTime() >= resultLastCalculated + PRE_EQ_CALC_US)) || (decoderFinished && (bufferQueue->count() == 1))) {
            resultLastCalculated = buffer->startTime();

            QVector<double> gains;
            for (int i = 0; i < replayGainCalculators.size(); i++) {
                ReplayGainCalculator *replayGainCalculator = replayGainCalculators.at(i);

                double x = replayGainCalculator->calculateResult();

                double correction = referenceLevels.at(i) - x;
                if (correction > PRE_EQ_MAX_DB) {
                    correction = PRE_EQ_MAX_DB;
                }
                else if (correction < PRE_EQ_MIN_DB) {
                    correction = PRE_EQ_MIN_DB;
                }
                gains.append(correction);
            }
            emit measuredGains(gains, replayGainCalculators.at(0)->getScaledPeak());
        }

        cache->storeBuffer(buffer);

        bufferQueueMutex->lock();
        bufferQueue->remove(0);
        bufferQueueMutex->unlock();

        delete buffer;

        if (!currentThread->isInterruptionRequested()) {
            currentThread->usleep(25);
        }
    }
}


int PreAnalyzer::getbandCount()
{
    return measurementFilters->getFilterCount();
}


void PreAnalyzer::run()
{
    Equalizer::Bands bands;
    if (bandCount <= 3) {
        bands.append(Equalizer::calculateBands(BANDS_3));
        referenceLevels.append(PRE_EQ_REF_3);
    }
    else if (bandCount == 4) {
        bands.append(Equalizer::calculateBands(BANDS_4));
        referenceLevels.append(PRE_EQ_REF_4);
    }
    else if (bandCount == 5) {
        bands.append(Equalizer::calculateBands(BANDS_5));
        referenceLevels.append(PRE_EQ_REF_5);
    }
    else if (bandCount == 6) {
        bands.append(Equalizer::calculateBands(BANDS_6));
        referenceLevels.append(PRE_EQ_REF_6);
    }
    else if (bandCount == 7) {
        bands.append(Equalizer::calculateBands(BANDS_7));
        referenceLevels.append(PRE_EQ_REF_7);
    }
    else if (bandCount == 8) {
        bands.append(Equalizer::calculateBands(BANDS_8));
        referenceLevels.append(PRE_EQ_REF_8);
    }
    else if (bandCount == 9) {
        bands.append(Equalizer::calculateBands(BANDS_9));
        referenceLevels.append(PRE_EQ_REF_9);
    }
    else if (bandCount == 10) {
        bands.append(Equalizer::calculateBands(BANDS_10));
        referenceLevels.append(PRE_EQ_REF_10);
    }
    else {
        double octavePerBand = 10 / bandCount;

        QVector<double> centerFreqs;
        centerFreqs.append(15.625);
        referenceLevels.append(0);
        for (int i = 1; i < bandCount; i++) {
            centerFreqs.append(15.625 * pow(2, i * octavePerBand));
            referenceLevels.append(0);
        }

        bands.append(Equalizer::calculateBands(centerFreqs));
    }

    sampleType = IIRFilter::getSampleTypeFromAudioFormat(format);
    if ((sampleType == IIRFilter::Unknown) || !QVector<int>({ 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000 }).contains(format.sampleRate())) {
        return;
    }

    measurementFilters = new IIRFilterChain();

    for (int i = 0; i < bands.size(); i++) {
        // cascading biquads for steeper cutoff
        CoefficientList coefficients;
        for (int j = 0; j < N / 2; j++) {
            Equalizer::Band band = bands.at(i);

            double Q = (sqrt((band.centerFrequency - band.bandwidth / 2) * (band.centerFrequency + band.bandwidth / 2)) / band.bandwidth) * sqrt((double)pow(2, (double)1 / N) - 1);
            double K = tan(M_PI * (band.centerFrequency / format.sampleRate()));

            CoefficientList coefficientsN = IIRFilter::calculateBiquadCoefficients(IIRFilter::BandPass, Q, K, 3);

            for(int k = 0; k < coefficientsN.aSize(); k++) {
                coefficients.appendA(coefficientsN.aValue(k));
            }
            if (coefficients.bSize() > 0) {
                coefficients.appendB(1);
            }
            for(int k = 0; k < coefficientsN.bSize(); k++) {
                coefficients.appendB(coefficientsN.bValue(k));
            }
        }

        replayGainCalculators.append(new ReplayGainCalculator(sampleType, format.sampleRate(), i == 0));

        measurementFilters->appendFilter(coefficients);
        measurementFilters->getFilter(i)->disableUpdateData();
        measurementFilters->getFilter(i)->setCallbackFiltered((IIRFilterCallback *)replayGainCalculators.at(i), (IIRFilterCallback::FilterCallbackPointer)&ReplayGainCalculator::filterCallback);
    }

    emit running();
}


void PreAnalyzer::decoderDone()
{
    decoderFinished = true;
}


void PreAnalyzer::setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex)
{
    this->bufferQueue      = bufferQueue;
    this->bufferQueueMutex = bufferQueueMutex;
}
