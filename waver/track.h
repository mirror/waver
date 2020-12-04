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


#ifndef TRACK_H
#define TRACK_H

#include <QtGlobal>

#include <QHash>
#include <QJsonDocument>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVector>

#include "globals.h"
#include "pluginlibsloader.h"
#include "pluginglobals.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif

class Track : public QObject {
        Q_OBJECT

    public:
        static const int CAST_ADD_PLAYTIME      = 5 * 60 * 1000;
        static const int INTERRUPT_FADE_SECONDS = 4;

        typedef QHash<QUuid, QString> PluginList;

        enum Status {
            Idle,
            Decoding,
            Playing,
            Paused
        };

        template <class T> static QString formatPluginName(T pluginInfo, bool version = false)
        {
            if (version) {
                return QString("<b>%1</b> v%2 <i>API %3</i>").arg(pluginInfo.name).arg(pluginInfo.version).arg(pluginInfo.waverVersionAPICompatibility);
            }

            return pluginInfo.name;
        }

        explicit Track(PluginLibsLoader::LoadedLibs *loadedLibs, TrackInfo trackInfo, QVariantHash additionalInfo, int castPlaytimeMilliseconds, int lovedCastPlaytimeMilliseconds, QUuid sourcePluginId,  QObject *parent = 0);
        ~Track();

        Status status();
        void   setStatus(Status status);
        void   interrupt();
        void   startWithFadeIn(qint64 lengthMilliseconds);
        void   startWithoutFadeIn();
        void   setAboutToFinishSend(qint64 beforeEndMillisecods);
        void   resetAboutToFinishSend();
        void   addMoreToCastPlaytime();
        void   addALotToCastPlaytime();
        void   setReplacable(bool replacable);
        bool   isReplacable();

        TrackInfo    getTrackInfo();
        QVariantHash getAdditionalInfo();
        void         addPictures(QVector<QUrl> pictures);
        QUuid        getSourcePluginId();
        bool         getFadeInRequested();
        qint64       getFadeInRequestedMilliseconds();
        bool         getNextTrackFadeInRequested();
        qint64       getNextTrackFadeInRequestedMilliseconds();
        bool         getPreviousTrackAboutToFinishSendRequested();
        qint64       getPreviousTrackAboutToFinishSendRequestedMilliseconds();


    private:

        static const int FADE_DIRECTION_NONE = 0;
        static const int FADE_DIRECTION_IN   = 1;
        static const int FADE_DIRECTION_OUT  = 2;

        struct PluginNoQueue {
            QString  name;
            int      version;
            QString  waverVersionAPICompatibility;
            bool     hasUI;
            QObject *pointer;
        };
        typedef QHash<QUuid, PluginNoQueue> PluginsNoQueue;

        struct PluginWithQueue {
            QString      name;
            int          version;
            QString      waverVersionAPICompatibility;
            bool         hasUI;
            QObject     *pointer;
            BufferQueue *bufferQueue;
            QMutex      *bufferMutex;
        };
        typedef QHash<QUuid, PluginWithQueue> PluginsWithQueue;

        struct SHOUTcastPMCTitlePosition {
            qint64  microSecondsTimestamp;
            QString title;
        };

        TrackInfo    trackInfo;
        QVariantHash additionalInfo;
        QUuid        sourcePluginId;

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

        QVector<QUuid>     decoderPriority;
        QVector<QUuid>     dspPrePriority;
        QVector<QUuid>     dspPriority;
        QVector<QObject *> dspPointers;
        BufferQueue        dspSynchronizerQueue;
        int                dspInitialBufferCount;

        QUuid                      currentDecoderId;
        QUuid                      mainOutputId;
        QHash<QAudioBuffer *, int> bufferOuputDoneCounters;

        Status currentStatus;
        long   currentCastPlaytimeMilliseconds;
        bool   fadeInRequested;
        bool   fadeInRequestedInternal;
        qint64 fadeInRequestedMilliseconds;
        qint64 fadeInRequestedInternalMilliseconds;
        int    fadeDirection;
        qint64 fadePercent;
        int    fadeSeconds;
        double fadeFrameCount;
        bool   interruptInProgress;
        qint64 interruptPosition;
        bool   interruptPositionWithFadeOut;
        qint64 interruptAboutToFinishSendPosition;
        qint64 interruptAboutToFinishSendPositionInternal;
        bool   nextTrackFadeInRequested;
        qint64 nextTrackFadeInRequestedMilliseconds;
        bool   previousTrackAboutToFinishSendRequested;
        qint64 previousTrackAboutToFinishSendRequestedMilliseconds;
        bool   decodingDone;
        bool   finishedSent;
        qint64 decodedMilliseconds;
        qint64 decodedMillisecondsAtUnderrun;
        qint64 playedMilliseconds;
        bool   replacable;

        QVector<SHOUTcastPMCTitlePosition> SHOUTcastPMCTitles;

        void setupDecoderPlugin(QObject *plugin, bool fromEasyPluginInstallDir, QMap<int, QUuid> *priorityMap);
        void setupDspPrePlugin(QObject *plugin, bool fromEasyPluginInstallDir, QMap<int, QUuid> *priorityMap);
        void setupDspPlugin(QObject *plugin, bool fromEasyPluginInstallDir, QMap<int, QUuid> *priorityMap);
        void setupOutputPlugin(QObject *plugin);
        void setupInfoPlugin(QObject *plugin);
        void sendLoadedPlugins();
        void sendLoadedPluginsWithUI();
        bool isFadeForbidden();
        void applyFade(QAudioBuffer *buffer);


    signals:

        // for external receivers

        void error(QUrl url, bool fatal, QString errorString);

        void savePluginSettings(QUuid uniqueId, QJsonDocument settings);
        void savePluginGlobalSettings(QUuid uniqueId, QJsonDocument settings);
        void loadPluginSettings(QUuid uniqueId);
        void loadPluginGlobalSettings(QUuid uniqueId);
        void executeSettingsSql(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString sql, QVariantList values);
        void executeGlobalSettingsSql(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString sql, QVariantList values);
        void loadedPlugins(Track::PluginList pluginsWithUI);
        void loadedPluginsWithUI(Track::PluginList pluginsWithUI);
        void pluginUi(QUuid id, QString qml, QString header);
        void pluginDiagnostics(QUuid id, QUrl url, DiagnosticData diagnosticData);
        void pluginWindow(QString qmlString);

        void playPosition(QUrl url, bool decoderFinished, long knownDurationMilliseconds, long positionMilliseconds);
        void aboutToFinish(QUrl url);
        void finished(QUrl url);
        void requestFadeInForNextTrack(QUrl url, qint64 lengthMilliseconds);
        void requestAboutToFinishSendForPreviousTrack(QUrl url, qint64 posBeforeEndMilliseconds);
        void trackInfoUpdated(QUrl url);

        // for internal receivers

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration);
        void loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration);
        void executedSqlResults(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results);
        void executedGlobalSqlResults(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results);
        void executedSqlError(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error);
        void messageFromPlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value);

        void requestPluginUi(QUuid id);
        void pluginUiResults(QUuid uniqueId, QJsonDocument results);
        void startDiagnostics(QUuid uniqueId);
        void stopDiagnostics(QUuid uniqueId);

        void startDecode(QUuid uniqueId);
        void bufferDone(QUuid uniqueId, QAudioBuffer *buffer);

        void bufferAvailable(QUuid uniqueId);

        void pause(QUuid uniqueId);
        void resume(QUuid uniqueId);
        void fadeIn(QUuid uniqueId, int seconds);
        void fadeOut(QUuid uniqueId, int seconds);
        void mainOutputPosition(qint64 posMilliseconds);

        void decoderDone(QUuid uniqueId);

        void playBegin(QUuid uniqueId);
        void messageFromDspPrePlugin(QUuid uniqueId, QUuid sourceUniqueId, int messageId, QVariant value);

        void getInfo(QUuid uniqueId, TrackInfo trackInfo);
        void getInfo(QUuid uniqueId, TrackInfo trackInfo, QVariantHash additionalInfo);
        void trackAction(QUuid uniqueId, int actionKey, TrackInfo trackInfo);


    public slots:

        void loadedPluginSettings(QUuid id, QJsonDocument settings);
        void loadedPluginGlobalSettings(QUuid id, QJsonDocument settings);
        void executedPluginSqlResults(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results);
        void executedPluginGlobalSqlResults(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results);
        void executedPluginSqlError(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error);
        void requestedPluginUi(QUuid id);
        void receivedPluginUiResults(QUuid uniqueId, QJsonDocument results);
        void startPluginDiagnostics(QUuid uniquedId);
        void stopPluginDiagnostics(QUuid uniquedId);
        void trackActionRequest(QUuid uniquedId, int actionKey, QUrl url);

        void infoUpdateTrackInfo(QUuid uniqueId, TrackInfo trackInfo);

    private slots:

        void loadConfiguration(QUuid uniqueId);
        void loadGlobalConfiguration(QUuid uniqueId);
        void saveConfiguration(QUuid uniqueId, QJsonDocument configuration);
        void saveGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration);
        void executeSql(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString sql, QVariantList values);
        void executeGlobalSql(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString sql, QVariantList values);
        void messageToPlugin(QUuid uniqueId, QUuid destinationUniqueId, int messageId, QVariant value);
        void ui(QUuid uniqueId, QString qml);
        void infoMessage(QUuid uniqueId, QString message);
        void diagnostics(QUuid id, DiagnosticData diagnosticData);
        void window(QString qmlString);

        void moveBufferInQueue(QUuid pluginId, QAudioBuffer *buffer);
        void sendFinished();

        void decoderCastTitle(QUuid uniqueId, qint64 microSecondsTimestamp, QString title);
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
        void dspPreRequestAboutToFinishSendForPreviousTrack(QUuid uniqueId, qint64 posBeforeEndMilliseconds);
        void dspPreMessageToDspPlugin(QUuid uniqueId, QUuid destinationUniqueId, int messageId, QVariant value);

        void infoAddInfoHtml(QUuid uniqueId, QString info);
};

#endif // TRACK_H
