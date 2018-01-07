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


#include "mpg123decoder.h"

// plugin factory
void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal)
{
    if (pluginTypesMask & PLUGIN_TYPE_DECODER) {
        #ifndef Q_OS_ANDROID
        retVal->append((QObject *) new Mpg123Decoder());
        #endif
    }
}


// global method
int Mpg123Decoder::pluginType()
{
    return PLUGIN_TYPE_DECODER;
}


// global method
QString Mpg123Decoder::pluginName()
{
    return "mpg123 Decoder";
}


// global method
int Mpg123Decoder::pluginVersion()
{
    return 3;
}


// overrided virtual function
QString Mpg123Decoder::waverVersionAPICompatibility()
{
    return "0.0.5";
}


// global method
void Mpg123Decoder::setUserAgent(QString userAgent)
{
    this->userAgent = userAgent;
}


// global method
QUuid Mpg123Decoder::persistentUniqueId()
{
    return id;
}


// global method
bool Mpg123Decoder::hasUI()
{
    return false;
}


// overriden virtual function
int Mpg123Decoder::priority()
{
    return 1;
}


// global method
void Mpg123Decoder::setUrl(QUrl url)
{
    // url can be set only once
    if (this->url.isEmpty()) {
        this->url = url;
    }
}


// constructor
Mpg123Decoder::Mpg123Decoder()
{
    id = QUuid("{6D1360A1-AE45-4D40-AFAF-74BD35982B53}");

    memoryUsage         = 0;
    decodedMicroSeconds = 0;
    feed                = NULL;
    mpg123Handle        = NULL;
    sendDiagnostics     = false;
}


// destructor
Mpg123Decoder::~Mpg123Decoder()
{
    feedThread.quit();
    feedThread.wait();

    if (mpg123Handle != NULL) {
        mpg123_delete(mpg123Handle);
        mpg123_exit();
    }

    while (audioBuffers.count() > 0) {
        delete audioBuffers.at(0);
        audioBuffers.remove(0);
    }
}


// thread entry point
void Mpg123Decoder::run()
{
    // checky-checky
    if (url.isEmpty()) {
        emit error(id, "Url is empty");
        return;
    }

    // intialize mpg123 decoder
    int mpg123Result = mpg123_init();
    if (mpg123Result != MPG123_OK) {
        emit error(id, QString(mpg123_plain_strerror(mpg123Result)));
        return;
    }
    mpg123Handle = mpg123_new(NULL, &mpg123Result);
    if (mpg123Handle == NULL) {
        mpg123_exit();
        emit error(id, QString(mpg123_plain_strerror(mpg123Result)));
        return;
    }

    // set up output formats
    mpg123_format_none(mpg123Handle);
    mpg123_format(mpg123Handle, 8000,  MPG123_STEREO | MPG123_MONO, MPG123_ENC_SIGNED_8 | MPG123_ENC_UNSIGNED_8 | MPG123_ENC_SIGNED_16 | MPG123_ENC_UNSIGNED_16 | MPG123_ENC_SIGNED_32 | MPG123_ENC_UNSIGNED_32);
    mpg123_format(mpg123Handle, 11025, MPG123_STEREO | MPG123_MONO, MPG123_ENC_SIGNED_8 | MPG123_ENC_UNSIGNED_8 | MPG123_ENC_SIGNED_16 | MPG123_ENC_UNSIGNED_16 | MPG123_ENC_SIGNED_32 | MPG123_ENC_UNSIGNED_32);
    mpg123_format(mpg123Handle, 22050, MPG123_STEREO | MPG123_MONO, MPG123_ENC_SIGNED_8 | MPG123_ENC_UNSIGNED_8 | MPG123_ENC_SIGNED_16 | MPG123_ENC_UNSIGNED_16 | MPG123_ENC_SIGNED_32 | MPG123_ENC_UNSIGNED_32);
    mpg123_format(mpg123Handle, 44100, MPG123_STEREO | MPG123_MONO, MPG123_ENC_SIGNED_8 | MPG123_ENC_UNSIGNED_8 | MPG123_ENC_SIGNED_16 | MPG123_ENC_UNSIGNED_16 | MPG123_ENC_SIGNED_32 | MPG123_ENC_UNSIGNED_32);
    mpg123_format(mpg123Handle, 48000, MPG123_STEREO | MPG123_MONO, MPG123_ENC_SIGNED_8 | MPG123_ENC_UNSIGNED_8 | MPG123_ENC_SIGNED_16 | MPG123_ENC_UNSIGNED_16 | MPG123_ENC_SIGNED_32 | MPG123_ENC_UNSIGNED_32);
}


// start decoding
void Mpg123Decoder::start(QUuid uniqueId)
{
    // parameter check
    if (uniqueId != id) {
        return;
    }

    // checky-checky
    if (url.isEmpty()) {
        emit error(id, "Url is empty");
        return;
    }

    // start feed (a.k.a. file reader/downloader)

    feed = new Feed(url, userAgent);
    feed->moveToThread(&feedThread);

    connect(&feedThread, SIGNAL(started()),  feed, SLOT(run()));
    connect(&feedThread, SIGNAL(finished()), feed, SLOT(deleteLater()));

    connect(feed, SIGNAL(ready()),        this, SLOT(feedReady()));
    connect(feed, SIGNAL(error(QString)), this, SLOT(feedError(QString)));

    feedThread.start();

    // open decoder
    int mpg123Result = mpg123_open_feed(mpg123Handle);
    if (mpg123Result != MPG123_OK) {
        emit error(id, QString(mpg123_plain_strerror(mpg123Result)));
        return;
    }
}


// this plugin has no configuration
void Mpg123Decoder::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// this plugin has no configuration
void Mpg123Decoder::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// configuration
void Mpg123Decoder::sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void Mpg123Decoder::globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void Mpg123Decoder::sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(error);
}


// this plugin has no configuration
void Mpg123Decoder::getUiQml(QUuid uniqueId)
{
    Q_UNUSED(uniqueId);
}


// this plugin has no configuration
void Mpg123Decoder::uiResults(QUuid uniqueId, QJsonDocument results)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(results);
}


// client wants to receive updates of this plugin's diagnostic information
void Mpg123Decoder::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = true;
    sendDiagnosticsData();
}


// client doesnt want to receive updates of this plugin's diagnostic information anymore
void Mpg123Decoder::stopDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = false;
}


// network download signal handler
void Mpg123Decoder::feedReady()
{
    // set up buffers for mpg123
    char input[INPUT_SIZE];
    char output[OUTPUT_SIZE];
    size_t input_size;
    size_t output_size;

    // loop until it's done
    bool done     = false;
    bool wasError = false;
    while (!done && !wasError && !QThread::currentThread()->isInterruptionRequested()) {
        // try to read from file/internet
        input_size = feed->read(&input[0], INPUT_SIZE);
        while ((input_size == 0) && !feed->isFinished() && !QThread::currentThread()->isInterruptionRequested()) {
            // this can happen when waiting for the network, so let's wait a bit
            QThread::currentThread()->msleep(250);
            input_size = feed->read(&input[0], INPUT_SIZE);
        }

        // make sure user wants to keep going
        if (QThread::currentThread()->isInterruptionRequested()) {
            break;
        }

        // if there's nothing more to read, then decoding is done
        if ((input_size == 0) && (feed->isFinished())) {
            done = true;
            continue;
        }

        // feed to the decoder
        int mpg123Result = mpg123_decode(mpg123Handle, (unsigned char *)&input, input_size, (unsigned char *)&output, OUTPUT_SIZE, &output_size);
        if (mpg123Result == MPG123_ERR) {
            emit error(id, QString(mpg123_plain_strerror(mpg123Result)));
            wasError = true;
            continue;
        }

        // there will be more than one decoded chucks for each input
        while ((output_size > 0) && !QThread::currentThread()->isInterruptionRequested()) {
            // get the current format
            long rate;
            int  channels;
            int  encoding;
            mpg123Result = mpg123_getformat(mpg123Handle, &rate, &channels, &encoding);
            if (mpg123Result != MPG123_OK) {
                emit error(id, QString(mpg123_plain_strerror(mpg123Result)));
                wasError    = true;
                output_size = 0;
                continue;
            }

            // create format object
            QAudioFormat audioFormat;
            audioFormat.setByteOrder(QSysInfo::ByteOrder == QSysInfo::BigEndian ? QAudioFormat::BigEndian : QAudioFormat::LittleEndian);
            audioFormat.setChannelCount(channels);
            audioFormat.setCodec("audio/pcm");
            audioFormat.setSampleRate(rate);
            switch (encoding) {
                case MPG123_ENC_SIGNED_8:
                    audioFormat.setSampleSize(8);
                    audioFormat.setSampleType(QAudioFormat::SignedInt);
                    break;
                case MPG123_ENC_UNSIGNED_8:
                    audioFormat.setSampleSize(8);
                    audioFormat.setSampleType(QAudioFormat::UnSignedInt);
                    break;
                case MPG123_ENC_SIGNED_16:
                    audioFormat.setSampleSize(16);
                    audioFormat.setSampleType(QAudioFormat::SignedInt);
                    break;
                case MPG123_ENC_UNSIGNED_16:
                    audioFormat.setSampleSize(16);
                    audioFormat.setSampleType(QAudioFormat::UnSignedInt);
                    break;
                case MPG123_ENC_SIGNED_32:
                    audioFormat.setSampleSize(32);
                    audioFormat.setSampleType(QAudioFormat::SignedInt);
                    break;
                case MPG123_ENC_UNSIGNED_32:
                    audioFormat.setSampleSize(32);
                    audioFormat.setSampleType(QAudioFormat::UnSignedInt);
            }

            // create audio buffer
            QAudioBuffer *audioBuffer = new QAudioBuffer(QByteArray(&output[0], output_size), audioFormat, decodedMicroSeconds);

            // update counters
            memoryUsage         += audioBuffer->byteCount();
            decodedMicroSeconds += audioBuffer->format().durationForBytes(audioBuffer->byteCount());

            // remember buffer so it can be deleted later
            audioBuffers.append(audioBuffer);

            // diagnostics
            if (sendDiagnostics) {
                sendDiagnosticsData();
            }

            // emit signal
            emit bufferAvailable(id, audioBuffer);

            // delay to limit memory usage and also to relieve the CPU (the exponential curve allows fast decoding in the beginning, then slows it down sharply as memory gets closer to limit)
            double        factor = qPow(memoryUsage, 12) / qPow(MAX_MEMORY, 12);
            unsigned long delay  = qMin((unsigned long)2500 * qRound((factor * USEC_PER_SEC) / 2500), USEC_PER_SEC - 1);
            if (delay > 0) {
                QThread::currentThread()->usleep(delay);
            }

            // also must give a chance for bufferDone events to process
            QCoreApplication::processEvents();

            // get more decoded pcm data
            int mpg123Result = mpg123_decode(mpg123Handle, NULL, 0, (unsigned char *)&output, OUTPUT_SIZE, &output_size);
            if (mpg123Result == MPG123_ERR) {
                emit error(id, QString(mpg123_plain_strerror(mpg123Result)));
                wasError    = true;
                output_size = 0;
            }
        }
    }

    // no reason to keep mpg123 working
    mpg123_delete(mpg123Handle);
    mpg123_exit();
    mpg123Handle = NULL;

    if (done) {
        emit finished(id);
    }
}


// network download signal handler
void Mpg123Decoder::feedError(QString errorString)
{
    // let the world know
    emit error(id, errorString);
}


// signal handler from owner
void Mpg123Decoder::bufferDone(QUuid uniqueId, QAudioBuffer *buffer)
{
    // parameter check
    if (uniqueId != id) {
        return;
    }

    // release buffer
    if (audioBuffers.contains(buffer)) {
        audioBuffers.remove(audioBuffers.indexOf(buffer));
        memoryUsage -= buffer->byteCount();
        delete buffer;
    }

    // diagnostics
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// private method
void Mpg123Decoder::sendDiagnosticsData()
{
    DiagnosticData diagnosticData;

    diagnosticData.append({ "PCM buffer size", formatBytes((double) memoryUsage) });
    if (feed != NULL) {
        diagnosticData.append({ "Raw buffer size", formatBytes((double) feed->getTotalBufferBytes()) });
    }

    if (audioBuffers.count() > 0) {
        QString type;
        switch (audioBuffers.last()->format().sampleType()) {
            case QAudioFormat::SignedInt:
                type = "signed";
                break;
            case QAudioFormat::UnSignedInt:
                type = "unsigned";
                break;
            case QAudioFormat::Float:
                type = "floating point";
                break;
            default:
                type = "unknown";
        }
        diagnosticData.append({ "PCM format", QString("%1 Hz, %2 bit, %3").arg(audioBuffers.last()->format().sampleRate()).arg(audioBuffers.last()->format().sampleSize()).arg(type) });
    }
    emit diagnostics(id, diagnosticData);
}


// helper
QString Mpg123Decoder::formatBytes(double bytes)
{
    if (bytes > (1024 * 1024)) {
        return QString("%1 MB").arg(bytes / (1024 * 1024), 0, 'f', 2);
    }
    else if (bytes > 1024) {
        return QString("%1 KB").arg(bytes / 1024, 0, 'f', 2);
    }

    return QString("%1 B").arg(bytes, 0, 'f', 0);
}
