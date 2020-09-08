/*
    This file is part of Waver

    Copyright (C) 2018-2019 Peter Papp <peter.papp.p@gmail.com>

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


#include "rmsmeter.h"


// plugin factory
void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal)
{
    if (pluginTypesMask & PLUGIN_TYPE_OUTPUT) {
        retVal->append((QObject *) new RMSMeter());
    }
}


// static members
int           RMSMeter::instanceCount;
const QString RMSMeter::SHAREDMEMORY_KEY = "WaverRMSMeter";


// constructor
RMSMeter::RMSMeter()
{
    id = QUuid("{CE401580-0FB6-46F3-B83D-5EECC8021985}");

    audioFramePerVideoFrame = 0;

    int16Min   = std::numeric_limits<qint16>::min();
    int16Max   = std::numeric_limits<qint16>::max();
    int16Range = int16Max - int16Min;

    dataType     = 0;
    channelCount = 0;
    channelIndex = 0;
    lRmsSum      = 0;
    rRmsSum      = 0;
    frameCount   = 0;
    lPeak        = 0;
    rPeak        = 0;

    sharedMemory = nullptr;

    instanceCount++;
    instanceId = instanceCount;
}


// destructor
RMSMeter::~RMSMeter()
{
    if (sharedMemory != nullptr) {

        int     *shMemInstanceCount = static_cast<int *>(sharedMemory->data());
        RMSData *shMemRMSData       = reinterpret_cast<RMSData *>(static_cast<char *>(sharedMemory->data()) + sizeof(int));

        if (shMemInstanceCount == nullptr) {
            return;
        }

        sharedMemory->lock();

        RMSData *seeker = shMemRMSData;
        bool     found  = false;
        int      i      = 0;
        while (!found && (i < *shMemInstanceCount)) {
            if (seeker->instanceId == instanceId) {
                found = true;
            }
            else {
                i++;
                seeker++;
            }
        }

        if (found) {
            while (i < (*shMemInstanceCount - 1)) {
                RMSData *temp = seeker;
                temp++;
                seeker = temp;
                i++;
            }
        }

        *shMemInstanceCount -= 1;

        sharedMemory->unlock();

        if (sendDiagnostics) {
            sendDiagnosticsData();
        }

        sharedMemory->detach();
        delete sharedMemory;
    }
}


// overrided virtual function
int RMSMeter::pluginType()
{
    return PLUGIN_TYPE_OUTPUT;
}


// overrided virtual function
QString RMSMeter::pluginName()
{
    return "RMS Meter";
}


// overrided virtual function
int RMSMeter::RMSMeter::pluginVersion()
{
    return 1;
}


// overrided virtual function
QString RMSMeter::waverVersionAPICompatibility()
{
    return "0.0.6";
}


// overrided virtual function
void RMSMeter::setBufferQueue(BufferQueue *bufferQueue, QMutex *bufferQueueMutex)
{
    this->bufferQueue      = bufferQueue;
    this->bufferQueueMutex = bufferQueueMutex;
}


// overrided virtual function
bool RMSMeter::isMainOutput()
{
    return false;
}


// overrided virtual function
QUuid RMSMeter::persistentUniqueId()
{
    return id;
}


// overrided virtual function
bool RMSMeter::hasUI()
{
    return true;
}


// thread entry point
void RMSMeter::run()
{
    sharedMemory = new QSharedMemory(SHAREDMEMORY_KEY);
    if (!sharedMemory->attach()) {
        if (sharedMemory->create(sizeof(int) + SHAREDMEMORY_MAX_INSTANCES * sizeof(RMSData))) {
            sharedMemory->lock();
            memset(sharedMemory->data(), 0, static_cast<size_t>(sharedMemory->size()));
            sharedMemory->unlock();
        }
        else {
            emit infoMessage(id, sharedMemory->errorString());
        }
    }

    if (instanceId == 1) {
        QTimer::singleShot(750, this, SLOT(requestWindow()));
    }

    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// timer slot
void RMSMeter::requestWindow()
{
    QFile meterFile("://RMSMeter.qml");
    meterFile.open(QFile::ReadOnly);
    QString meter = meterFile.readAll();
    meterFile.close();

    emit window(meter);
}


// signal handler
void RMSMeter::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// signal handler
void RMSMeter::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// configuration
void RMSMeter::sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void RMSMeter::globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void RMSMeter::sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(error);
}


// message from another plugin
void RMSMeter::messageFromPlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(sourceUniqueId);
    Q_UNUSED(messageId);
    Q_UNUSED(value);
}


// signal handler
void RMSMeter::getUiQml(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    requestWindow();
}


// signal handler
void RMSMeter::uiResults(QUuid uniqueId, QJsonDocument results)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(results);
}


// client wants to receive updates of this plugin's diagnostic information
void RMSMeter::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = true;
    sendDiagnosticsData();
}


// client doesnt want to receive updates of this plugin's diagnostic information anymore
void RMSMeter::stopDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = false;
}


// signal handler
void RMSMeter::bufferAvailable(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    // had to wait with some initializations, because the audio format is needed
    if (audioFramePerVideoFrame == 0) {
        QAudioFormat audioFormat = bufferQueue->at(0)->format();

        channelCount = audioFormat.channelCount();

        if (channelCount > 0) {
            audioFramePerVideoFrame = audioFormat.framesForDuration(1000000 / 10);

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
    }

    while (bufferQueue->count() > 0) {
        // diagonostics
        if (sendDiagnostics) {
            sendDiagnosticsData();
        }

        // so that it doesn't have to be called many times
        QAudioBuffer *buffer = bufferQueue->at(0);

        qint64 timestamp           = buffer->startTime();
        int    timestampFrameCount = 0;

        if (dataType != 0) {
            // only one of these will be used, depending on the data type
            qint8   *int8;
            qint16  *int16;
            qint32  *int32;
            quint8  *uint8;
            quint16 *uint16;
            quint32 *uint32;

            double sampleValue = 0;

            // do the math sample by sample
            char  *data      = (char *)buffer->constData();
            int    byteCount = 0;
            while (byteCount < buffer->byteCount()) {
                // not all formats supported, but most common ones are

                // calculation
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

                // have to scale value if not the expected type
                if (dataType != 2) {
                    sampleValue = (((sampleValue - sampleMin) / sampleRange) * int16Range) + int16Min;
                }

                // calculations of temporary values
                if (channelIndex == 0) {
                    frameCount++;
                    timestampFrameCount++;

                    lRmsSum += (sampleValue * sampleValue);
                    if (abs(sampleValue) > lPeak) {
                        lPeak = abs(sampleValue);
                    }

                    channelIndex++;
                }
                else if (channelIndex == 1) {
                    rRmsSum += (sampleValue * sampleValue);
                    if (abs(sampleValue) > rPeak) {
                        rPeak = abs(sampleValue);
                    }

                    channelIndex++;
                }

                // keep track of channels
                if (channelIndex >= channelCount) {
                    channelIndex = 0;
                }

                // send values
                if (frameCount == audioFramePerVideoFrame) {
                    frameCount = 0;
                    timestamp += buffer->format().durationForFrames(timestampFrameCount);

                    RMSTimedData timedData;

                    timedData.timestamp = timestamp;
                    timedData.lrms      = 20. * log10(sqrt(lRmsSum / audioFramePerVideoFrame) / (int16Range / 2));
                    timedData.lpeak     = 20. * log10(lPeak / (int16Range / 2));
                    timedData.rrms      = 20. * log10(sqrt(rRmsSum / audioFramePerVideoFrame) / (int16Range / 2));
                    timedData.rpeak     = 20. * log10(rPeak / (int16Range / 2));

                    lRmsSum      = 0;
                    lPeak        = 0;
                    rRmsSum      = 0;
                    rPeak        = 0;

                    this->timedData.append(timedData);
                }
            }

            // remove from queue
            bufferQueueMutex->lock();
            bufferQueue->remove(0);
            bufferQueueMutex->unlock();

            // this makes the decoder delete it
            emit bufferDone(id, buffer);
        }
    }
}


// signal handler
void RMSMeter::mainOutputPosition(qint64 posMilliseconds)
{
    while ((timedData.count() > 0) && (timedData.at(0).timestamp < posMilliseconds)) {
        timedData.removeFirst();
    }
    if (timedData.count() < 1) {
        return;
    }

    int     *shMemInstanceCount = static_cast<int *>(sharedMemory->data());
    RMSData *shMemRMSData       = reinterpret_cast<RMSData *>(static_cast<char *>(sharedMemory->data()) + sizeof(int));

    if (shMemInstanceCount == nullptr) {
        return;
    }

    sharedMemory->lock();

    int trackCount = *shMemInstanceCount;

    RMSData *seeker = shMemRMSData;
    bool     found  = false;
    int      i      = 0;
    while (!found && (i < trackCount)) {
        if (seeker->instanceId == instanceId) {
            found = true;
        }
        else {
            i++;
            seeker++;
        }
    }

    if (!found && (trackCount < SHAREDMEMORY_MAX_INSTANCES)) {
        seeker = shMemRMSData;
        for (i = 0; i < trackCount; i++) {
            seeker++;
        }

        seeker->instanceId = instanceId;
        *shMemInstanceCount += 1;

        found = true;
    }

    RMSTimedData timedData = this->timedData.at(0);

    if (found) {
        seeker->lpeak = timedData.lpeak;
        seeker->lrms  = timedData.lrms;
        seeker->rpeak = timedData.rpeak;
        seeker->rrms  = timedData.rrms;
    }

    sharedMemory->unlock();
}


// signal handler
void RMSMeter::pause(QUuid uniqueId)
{
    resume(uniqueId);
}


// signal handler
void RMSMeter::resume(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    int     *shMemInstanceCount = static_cast<int *>(sharedMemory->data());
    RMSData *shMemRMSData       = reinterpret_cast<RMSData *>(static_cast<char *>(sharedMemory->data()) + sizeof(int));

    if (shMemInstanceCount == nullptr) {
        return;
    }

    sharedMemory->lock();

    RMSData *seeker = shMemRMSData;
    bool     found  = false;
    int      i      = 0;
    while (!found && (i < *shMemInstanceCount)) {
        if (seeker->instanceId == instanceId) {
            found = true;
        }
        else {
            i++;
            seeker++;
        }
    }

    if (found) {
        seeker->lpeak = -99.9;
        seeker->lrms  = -99.9;
        seeker->rpeak = -99.9;
        seeker->rrms  = -99.9;
    }

    sharedMemory->unlock();

    timedData.clear();

    foreach (QAudioBuffer *buffer, *bufferQueue) {
        emit bufferDone(id, buffer);
    }

    bufferQueueMutex->lock();
    bufferQueue->clear();
    bufferQueueMutex->unlock();

    QThread::currentThread()->msleep(80);
}


// private method
void RMSMeter::sendDiagnosticsData()
{
    DiagnosticData diagnosticData;

    int *shMemInstanceCount = static_cast<int *>(sharedMemory->data());

    if (shMemInstanceCount != nullptr) {
        sharedMemory->lock();
        diagnosticData.append({ "Active tracks", QString("%1").arg(*shMemInstanceCount) });
        sharedMemory->unlock();
    }

    emit diagnostics(id, diagnosticData);
}
