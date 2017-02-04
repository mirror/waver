/*
    This file is part of Waver

    Copyright (C) 2017 Peter Papp <peter.papp.p@gmail.com>

    Please visit https://launchpad.net/waver for details

    Waver is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Waver is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    (GPL.TXT) along with Waver. If not, see <http://www.gnu.org/licenses/>.

*/


#include "analyzer.h"

// overriden virtual function
int Analyzer::pluginType()
{
    return PLUGIN_TYPE_DSP_PRE;
}


// overriden virtual function
QString Analyzer::pluginName()
{
    return "ReplayGain Analyzer";
}


// overrided virtual function
int Analyzer::pluginVersion()
{
    return 1;
}


// overriden virtual function
QUuid Analyzer::persistentUniqueId()
{
    return id;
}


// overriden virtual function
bool Analyzer::hasUI()
{
    return true;
}


// overriden virtual function
int Analyzer::priority()
{
    return 1;
}


// overriden virtual function
void Analyzer::setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex)
{
    this->bufferQueue      = bufferQueue;
    this->bufferQueueMutex = bufferQueueMutex;
}


// constructor
Analyzer::Analyzer()
{
    id = QUuid("{5C60545B-5E0D-4CD8-9296-227442ADC49B}");

    enableTransitions = true;

    filtersSetup                  = false;
    firstNonSilentPositionChecked = false;
    sampleType                    = IIRFilter::Unknown;
    replayGainFilter              = NULL;
    replayGainCalculator          = NULL;
    fadeOutDetector               = NULL;
    resultLastCalculated          = 0;
    decoderFinished               = false;
}


// destructor
Analyzer::~Analyzer()
{
    if (replayGainFilter != NULL) {
        delete replayGainFilter;
    }
    if (replayGainCalculator != NULL) {
        delete replayGainCalculator;
    }
    if (fadeOutDetector != NULL) {
        delete fadeOutDetector;
    }
}


// server event handler
void Analyzer::run()
{
    emit loadConfiguration(id);
}


// server event handler
void Analyzer::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    if (configuration.object().contains("enable_transitions")) {
        enableTransitions = configuration.object().value("enable_transitions").toBool();
    }
}


// server event handler
void Analyzer::getUiQml(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    QFile settingsFile("://EQ_AnalyzerSettings.qml");
    settingsFile.open(QFile::ReadOnly);
    QString settings = settingsFile.readAll();
    settingsFile.close();

    if (!enableTransitions) {
        settings.replace("checked: true", "checked: false");
    }

    emit uiQml(id, settings);
}


// server event handler
void Analyzer::uiResults(QUuid uniqueId, QJsonDocument results)
{
    if (uniqueId != id) {
        return;
    }

    enableTransitions = results.object().value("enable_transitions").toBool();

    emit saveConfiguration(id, results);
}


// server event handler
void Analyzer::bufferAvailable(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    while (bufferQueue->count() > 0) {
        // Replay Gain analysis does change the signal, so let's make a copy of the buffer
        QAudioBuffer buffer = QAudioBuffer(QByteArray((char*)bufferQueue->at(0)->constData(), bufferQueue->at(0)->byteCount()), bufferQueue->at(0)->format(), bufferQueue->at(0)->startTime());

        // had to wait with filters setup for the first buffer because format needed
        if (!filtersSetup) {
            filtersSetup = true;

            sampleType = IIRFilter::getSampleTypeFromAudioFormat(buffer.format());

            // TODO support more sample rates
            QVector<int> supportedSampleRates({44100});

            // set up only if supported
            if ((sampleType != IIRFilter::Unknown) && supportedSampleRates.contains(buffer.format().sampleRate())) {
                replayGainFilter     = new IIRFilterChain();
                replayGainCalculator = new ReplayGainCalculator(sampleType, buffer.format().sampleRate());
                fadeOutDetector      = new FadeOutDetector(sampleType, buffer.format().sampleRate());

                switch (buffer.format().sampleRate()) {
                case 44100:
                    replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_44100_YULEWALK_A, REPLAYGAIN_44100_YULEWALK_B));
                    replayGainFilter->appendFilter(CoefficientList(REPLAYGAIN_44100_BUTTERWORTH_A, REPLAYGAIN_44100_BUTTERWORTH_B));
                }
                replayGainFilter->getFilter(0)->setCallbackRaw((IIRFilterCallback*)fadeOutDetector, (IIRFilterCallback::FilterCallbackPointer)&FadeOutDetector::filterCallback);
                replayGainFilter->getFilter(1)->setCallbackFiltered((IIRFilterCallback*)replayGainCalculator, (IIRFilterCallback::FilterCallbackPointer)&ReplayGainCalculator::filterCallback);
            }
        }

        // process buffer
        if (replayGainFilter != NULL) {
            replayGainFilter->processPCMData(buffer.data(), buffer.byteCount(), sampleType, buffer.format().channelCount());

            // get the Replay Gain value periodically and send it to the Equalizer
            if ((buffer.startTime() >= (resultLastCalculated + 4000000)) || (decoderFinished && (bufferQueue->count() == 1))) {
                resultLastCalculated = buffer.startTime();

                double result = replayGainCalculator->calculateResult();
                emit messageToDspPlugin(id, QUuid("{8D25249B-4D29-4279-80B5-4DCDD23A5809}"), DSP_MESSAGE_REPLAYGAIN, result);
            }

            // check some stuff that's needed for transitions
            if (enableTransitions) {
                // usually there's at least one tenth of a second silence at the begining of tracks; if not, chances are it's a live recording or a medley or something similar
                if (!firstNonSilentPositionChecked && (fadeOutDetector->getFirstNonSilentMSec() > 0) && (fadeOutDetector->getFirstNonSilentMSec() < 100)) {
                    firstNonSilentPositionChecked = true;
                    emit requestFadeIn(id, Track::INTERRUPT_FADE_SECONDS * 1000);
                }

                // track transitions can be made after the entire track has been analyzed (this might not happen here, see decoderDone)
                if (decoderFinished && (bufferQueue->count() == 1)) {
                    transition();
                }
            }
        }

        // need this pointer now
        QAudioBuffer *bufferPointer = bufferQueue->at(0);

        // remove from queue
        bufferQueueMutex->lock();
        bufferQueue->remove(0);
        bufferQueueMutex->unlock();

        // this makes the track pass it on to the next plugin
        emit bufferDone(id, bufferPointer);
    }
}


// server event handler
void Analyzer::decoderDone(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    decoderFinished = true;

    // track transitions can be made after the entire track has been analyzed (this signal can come before or after the last buffer is processed)
    if ((replayGainFilter != NULL) && enableTransitions && (bufferQueue->count() == 0)) {
        transition();
    }
}


// private method (must be called only after all buffers are analyzed)
void Analyzer::transition()
{
    qint64 fadeOutStart  = fadeOutDetector->getFadeOutStartPoisitionMSec();
    qint64 fadeOutLength = (fadeOutDetector->getLastNonSilentMSec() - fadeOutStart);

    // crossfade
    if (fadeOutLength >= 10000) {
        emit requestAboutToFinishSend(id, fadeOutStart + (fadeOutLength / 3));
        emit requestFadeInForNextTrack(id, fadeOutDetector->getLastNonSilentMSec() - fadeOutStart);
        return;
    }

    // live recording, medley, etc
    if ((fadeOutDetector->getFirstNonSilentMSec() > 0) && (fadeOutDetector->getFirstNonSilentMSec() < 100)) {
        emit requestInterrupt(id, fadeOutDetector->getLastNonSilentMSec() - (Track::INTERRUPT_FADE_SECONDS * 1000), true);
    }

    // gapless play
    emit requestAboutToFinishSend(id, fadeOutDetector->getLastNonSilentMSec());
}
