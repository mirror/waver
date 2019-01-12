/*
    This file is part of Waver

    Copyright (C) 2017-2019 Peter Papp <peter.papp.p@gmail.com>

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


#include "equalizer.h"

// overriden virtual function
int Equalizer::pluginType()
{
    return PLUGIN_TYPE_DSP;
}


// overriden virtual function
QString Equalizer::pluginName()
{
    return "Equalizer";
}


// overrided virtual function
int Equalizer::pluginVersion()
{
    return 2;
}


// overrided virtual function
QString Equalizer::waverVersionAPICompatibility()
{
    return "0.0.4";
}


// overriden virtual function
QUuid Equalizer::persistentUniqueId()
{
    return id;
}


// overriden virtual function
bool Equalizer::hasUI()
{
    return true;
}


// overriden virtual function
int Equalizer::priority()
{
    return 100;
}


// overriden virtual function
void Equalizer::setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex)
{
    this->bufferQueue      = bufferQueue;
    this->bufferQueueMutex = bufferQueueMutex;
}


// constructor
Equalizer::Equalizer()
{
    id = QUuid("{8D25249B-4D29-4279-80B5-4DC0D23A5809}");

    equalizerFilters = NULL;

    // calculate bands
    QVector<double> centerFrequencies({31, 62, 125, 250, 500, 1000, 2500, 5000, 10000, 16000});
    bands.append({ centerFrequencies.at(0), centerFrequencies.at(0) / 2 });
    double previousHigh = centerFrequencies.at(0) * 1.25;
    for (int i = 1; i < centerFrequencies.size(); i++) {
        double bandwidth = (centerFrequencies.at(i) - previousHigh) * 2;
        bands.append({ centerFrequencies.at(i), bandwidth });
        previousHigh = centerFrequencies.at(i) + (bandwidth / 2);
    }

    replayGain        = 0.0;
    currentReplayGain = replayGain;
    sampleRate        = 0;
    sampleType        = IIRFilter::Unknown;
    playBegan         = false;
    sendDiagnostics   = false;
}


// destructor
Equalizer::~Equalizer()
{
    if (equalizerFilters != NULL) {
        delete equalizerFilters;
    }
}


// apply replay gain
void Equalizer::filterCallback(double *sample, int channelIndex)
{
    // replay gain can change as track being analyzed
    if ((channelIndex == 0) && (currentReplayGain != replayGain)) {
        double difference = replayGain - currentReplayGain;
        if (qAbs(difference) < 0.05) {
            currentReplayGain = replayGain;
        }
        else {
            double changePerSec = qMin(3.0, qAbs(difference));
            currentReplayGain = currentReplayGain + ((changePerSec / sampleRate) * (difference < 0 ? -1.0 : 1.0));
        }

        if (sendDiagnostics) {
            sendDiagnosticsData();
        }
    }

    // apply replay gain
    *sample = *sample * pow(10, (currentReplayGain + preAmp) / 20);
}


// server event handler
void Equalizer::run()
{
    emit loadGlobalConfiguration(id);
}


// server event handler
void Equalizer::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// server event handler
void Equalizer::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    if (uniqueId != id) {
        return;
    }

    if (configuration.isEmpty()) {
        // default values
        preAmp = 3;
        gains.clear();
        gains.append(6.0);
        gains.append(3.0);
        gains.append(1.5);
        gains.append(0.0);
        gains.append(-1.5);
        gains.append(0.0);
        gains.append(3.0);
        gains.append(6.0);
        gains.append(9.0);
        gains.append(12.0);
    }
    else {
        // get values from configuration
        preAmp = configuration.object().value("pre_amp").toDouble();
        gains.clear();
        gains.append(configuration.object().value("gain_31").toDouble());
        gains.append(configuration.object().value("gain_62").toDouble());
        gains.append(configuration.object().value("gain_125").toDouble());
        gains.append(configuration.object().value("gain_250").toDouble());
        gains.append(configuration.object().value("gain_500").toDouble());
        gains.append(configuration.object().value("gain_1000").toDouble());
        gains.append(configuration.object().value("gain_2500").toDouble());
        gains.append(configuration.object().value("gain_5000").toDouble());
        gains.append(configuration.object().value("gain_10000").toDouble());
        gains.append(configuration.object().value("gain_16000").toDouble());
    }

    if (sampleRate > 0) {
        createFilters();
    }
}


// configuration
void Equalizer::sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void Equalizer::globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void Equalizer::sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(error);
}


// server event handler
void Equalizer::getUiQml(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    QFile settingsFile("://EqualizerSettings.qml");
    settingsFile.open(QFile::ReadOnly);
    QString settings = settingsFile.readAll();
    settingsFile.close();

    settings.replace("replace_pre_amp_value",    QString("%1").arg(preAmp));
    settings.replace("replace_gain_31_value",    QString("%1").arg(gains.at(0)));
    settings.replace("replace_gain_62_value",    QString("%1").arg(gains.at(1)));
    settings.replace("replace_gain_125_value",   QString("%1").arg(gains.at(2)));
    settings.replace("replace_gain_250_value",   QString("%1").arg(gains.at(3)));
    settings.replace("replace_gain_500_value",   QString("%1").arg(gains.at(4)));
    settings.replace("replace_gain_1000_value",  QString("%1").arg(gains.at(5)));
    settings.replace("replace_gain_2500_value",  QString("%1").arg(gains.at(6)));
    settings.replace("replace_gain_5000_value",  QString("%1").arg(gains.at(7)));
    settings.replace("replace_gain_10000_value", QString("%1").arg(gains.at(8)));
    settings.replace("replace_gain_16000_value", QString("%1").arg(gains.at(9)));

    emit uiQml(id, settings);
}


// server event handler
void Equalizer::uiResults(QUuid uniqueId, QJsonDocument results)
{
    if (uniqueId != id) {
        return;
    }

    saveGlobalConfiguration(id, results);
    loadedGlobalConfiguration(id, results);
}


// server event handler
void Equalizer::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = true;
    sendDiagnosticsData();
}


// server event handler
void Equalizer::stopDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = false;
}


// server event handler
void Equalizer::bufferAvailable(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    while (bufferQueue->count() > 0) {
        // just to make things easier
        QAudioBuffer *buffer = bufferQueue->at(0);

        // had to wait with filters setup for the first buffer because format needed
        if ((equalizerFilters == NULL) && (gains.count() == 10)) {
            sampleRate = buffer->format().sampleRate();
            sampleType = IIRFilter::getSampleTypeFromAudioFormat(buffer->format());

            createFilters();
        }

        // process signal
        if (equalizerFilters != NULL) {
            equalizerFilters->processPCMData(buffer->data(), buffer->byteCount(), sampleType, buffer->format().channelCount());
        }

        // remove from queue
        bufferQueueMutex->lock();
        bufferQueue->remove(0);
        bufferQueueMutex->unlock();

        // this makes the track pass it on to the next plugin
        emit bufferDone(id, buffer);
    }
}


// server event handler
void Equalizer::playBegin(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    playBegan         = true;
    currentReplayGain = replayGain;

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// server event handler
void Equalizer::messageFromDspPrePlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value)
{
    if (uniqueId != id) {
        return;
    }
    if (sourceUniqueId != QUuid("{5C60545B-5E0D-4CD8-9296-227442ADC49B}")) {
        return;
    }

    if (messageId == Analyzer::DSP_MESSAGE_REPLAYGAIN) {
        replayGain = value.toDouble();

        if (sendDiagnostics) {
            sendDiagnosticsData();
        }

        return;
    }
}


// private method
void Equalizer::createFilters()
{
    // list to hold coefficients
    QList<CoefficientList> coefficientLists;

    // calculate coefficients for each filter
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

    // housekeeping
    if (equalizerFilters != NULL) {
        delete equalizerFilters;
    }

    // create new filter chain
    equalizerFilters = new IIRFilterChain(coefficientLists);

    // install Replay Gain applier callback
    equalizerFilters->getFilter(0)->setCallbackRaw((IIRFilterCallback *)this, (IIRFilterCallback::FilterCallbackPointer)&Equalizer::filterCallback);
}


// private method
void Equalizer::sendDiagnosticsData()
{
    DiagnosticData diagnosticData;
    diagnosticData.append({"ReplayGain target", QString("%1 dB").arg(replayGain, 0, 'f', 2)});
    if (playBegan) {
        diagnosticData.append({"ReplayGain applying", QString("%1 dB").arg(currentReplayGain, 0, 'f', 2) });
    }
    emit diagnostics(id, diagnosticData);
}
