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


#ifndef TRACK_H
#define TRACK_H

#include <QtGlobal>

#include <QHash>
#include <QMap>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include "globals.h"
#include "pluginbase.h"
#include "plugindecoder.h"
#include "plugindsp.h"
#include "plugindsppre.h"
#include "plugininfo.h"
#include "pluginlibsloader.h"
#include "pluginoutput.h"
#include "pluginsource.h"


#include <QDebug>


class Track : public QObject
{
    Q_OBJECT

public:

    static const int INTERRUPT_FADE_SECONDS = 4;

    typedef QHash<QUuid, QString> PluginsWithUI;

    enum Status {
        Idle,
        Decoding,
        Playing,
        Paused
    };

    template <class T> static QString formatPluginName(T pluginInfo)
    {
        return QString("%1 %2.%3.%4").arg(pluginInfo.name).arg(pluginInfo.baseVersion).arg(pluginInfo.pluginTypeVersion).arg(pluginInfo.version);
    }

    explicit Track(PluginLibsLoader::LoadedLibs *loadedLibs, PluginSource::TrackInfo trackInfo, QUuid sourcePliginId, QObject *parent = 0);
    ~Track();

    Status status();
    void   setStatus(Status status);
    void   interrupt();
    void   startWithFadeIn(qint64 lengthMilliseconds);

    PluginSource::TrackInfo getTrackInfo();
    QUuid                   getSourcePluginId();


private:

    struct PluginNoQueue {
        QString name;
        int     version;
        int     baseVersion;
        int     pluginTypeVersion;
        bool    hasUI;
    };
    typedef QHash<QUuid, PluginNoQueue> PluginsNoQueue;

    struct PluginWithQueue {
        QString                  name;
        int                      version;
        int                      baseVersion;
        int                      pluginTypeVersion;
        bool                     hasUI;
        PluginBase::BufferQueue *bufferQueue;
        QMutex                  *bufferMutex;
    };
    typedef QHash<QUuid, PluginWithQueue> PluginsWithQueue;

    PluginSource::TrackInfo trackInfo;
    QUuid                   sourcePliginId;

    QThread decoderThread;
    QThread dspPreThread;
    QThread dspThread;
    QThread outputThread;
    QThread infoThread;

    PluginsNoQueue   decoderPlugins;
    PluginsWithQueue dspPrePlugins;
    PluginsWithQueue dspPlugins;
    PluginsWithQueue outputPlugins;
    PluginsNoQueue   infoPlugins;

    QVector<QUuid>          decoders;
    QVector<QUuid>          dspPrePriority;
    QVector<QUuid>          dspPriority;
    QVector<PluginDsp*>     dspPointers;
    PluginBase::BufferQueue dspSynchronizerQueue;
    int                     dspInitialBufferCount;

    QUuid                     currentDecoderId;
    QUuid                     mainOutputId;
    QHash<QAudioBuffer*, int> bufferOuputDoneCounters;

    Status currentStatus;
    bool   fadeInRequested;
    qint64 fadeInRequestedMilliseconds;
    bool   interruptInProgress;
    qint64 interruptPosition;
    bool   interruptPositionWithFadeOut;
    qint64 interruptAboutToFinishSendPosition;
    bool   decodingDone;
    bool   finishedSent;
    qint64 decodedMilliseconds;
    qint64 decodedMillisecondsAtUnderrun;
    qint64 playedMilliseconds;

    void setupDecoderPlugin(PluginBase *plugin);
    void setupDspPrePlugin(PluginBase *plugin, bool fromEasyPluginInstallDir, QMap<int, QUuid> *priorityMap);
    void setupDspPlugin(PluginBase *plugin, bool fromEasyPluginInstallDir, QMap<int, QUuid> *priorityMap);
    void setupOutputPlugin(PluginBase *plugin);
    void setupInfoPlugin(PluginBase *plugin);
    void sendLoadedPluginsWithUI();
    void sendFinished();


signals:

    // for external receivers

    void error(QUrl url, bool fatal, QString errorString);

    void savePluginSettings(QUuid uniqueId, QJsonDocument settings);
    void loadPluginSettings(QUuid uniqueId);
    void loadedPluginsWithUI(Track::PluginsWithUI pluginsWithUI);
    void pluginUi(QUuid id, QString qml);

    void playPosition(QUrl url, bool cast, bool decoderFinished, long knownDurationMilliseconds, long positionMilliseconds);
    void aboutToFinish(QUrl url);
    void finished(QUrl url);
    void requestFadeInForNextTrack(QUrl url, qint64 lengthMilliseconds);
    void trackInfoUpdated(QUrl url);

    // for internal receivers

    void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration);
    void requestPluginUi(QUuid id);
    void pluginUiResults(QUuid uniqueId, QJsonDocument results);

    void startDecode(QUuid uniqueId);
    void bufferDone(QUuid uniqueId, QAudioBuffer *buffer);

    void bufferAvailable(QUuid uniqueId);

    void pause(QUuid uniqueId);
    void resume(QUuid uniqueId);
    void fadeIn(QUuid uniqueId, int seconds);
    void fadeOut(QUuid uniqueId, int seconds);

    void decoderDone(QUuid uniqueId);

    void playBegin(QUuid uniqueId);
    void messageFromDspPrePlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value);


public slots:

    void loadedPluginSettings(QUuid id, QJsonDocument settings);
    void requestedPluginUi(QUuid id);
    void receivedPluginUiResults(QUuid uniqueId, QJsonDocument results);


private slots:

    void loadConfiguration(QUuid uniqueId);
    void saveConfiguration(QUuid uniqueId, QJsonDocument configuration);
    void ui(QUuid uniqueId, QString qml);
    void infoMessage(QUuid uniqueId, QString message);

    void moveBufferInQueue(QUuid pluginId, QAudioBuffer *buffer);

    void decoderFinished(QUuid uniqueId);
    void decoderError(QUuid uniqueId, QString errorMessage);
    void underrunTimeout();

    void outputPositionChanged(QUuid uniqueId, qint64 posMilliseconds);
    void outputBufferUnderrun(QUuid uniqueId);
    void outputFadeInComplete(QUuid uniqueId);
    void outputFadeOutComplete(QUuid uniqueId);
    void outputError(QUuid uniqueId, QString errorMessage);

    void dspPreRequestFadeIn(QUuid uniqueId, qint64 lengthMilliseconds);
    void dspPreRequestFadeInForNextTrack(QUuid uniqueId, qint64 lengthMilliseconds);
    void dspPreRequestInterrupt(QUuid uniqueId, qint64 posMilliseconds, bool withFadeOut);
    void dspPreRequestAboutToFinishSend(QUuid uniqueId, qint64 posMilliseconds);
    void dspPreMessageToDspPlugin(QUuid uniqueId, QUuid destinationUniqueId, int messageId, QVariant value);

    void infoUpdateTrackInfo(QUuid uniqueId, PluginSource::TrackInfo trackInfo);
    void infoAddInfoHtml(QUuid uniqueId, QString info);

};

#endif // TRACK_H
