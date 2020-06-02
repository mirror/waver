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


#include "track.h"


// constructor
Track::Track(PluginLibsLoader::LoadedLibs *loadedLibs, TrackInfo trackInfo, QVariantHash additionalInfo, int castPlaytimeMilliseconds, int lovedCastPlaytimeMilliseconds, QUuid sourcePliginId, QObject *parent) : QObject(parent)
{
    // to make debugging easier
    decoderThread.setObjectName("decoder");
    dspPreThread.setObjectName("dsp_pre");
    dspThread.setObjectName("dsp");
    outputThread.setObjectName("output");
    infoThread.setObjectName("info");

    // remember track info
    this->trackInfo      = trackInfo;
    this->additionalInfo = additionalInfo;
    this->sourcePluginId = sourcePliginId;

    // starting out with this long playtime (if cast)
    long castStartingPlaytime = castPlaytimeMilliseconds;
    if (additionalInfo.contains("loved_longplay") && additionalInfo.value("loved_longplay").toInt()) {
        castStartingPlaytime = lovedCastPlaytimeMilliseconds;
    }

    // initializations
    currentStatus                                       = Idle;
    currentCastPlaytimeMilliseconds                     = castStartingPlaytime;
    fadeInRequested                                     = false;
    fadeInRequestedInternal                             = false;
    fadeInRequestedMilliseconds                         = 0;
    fadeInRequestedInternalMilliseconds                 = 0;
    fadeDirection                                       = FADE_DIRECTION_NONE;
    interruptInProgress                                 = false;
    interruptPosition                                   = 0;
    interruptPositionWithFadeOut                        = true;
    interruptAboutToFinishSendPosition                  = 0;
    interruptAboutToFinishSendPositionInternal          = 0;
    nextTrackFadeInRequested                            = false;
    nextTrackFadeInRequestedMilliseconds                = 0;
    previousTrackAboutToFinishSendRequested             = false;
    previousTrackAboutToFinishSendRequestedMilliseconds = 0;
    decodingDone                                        = false;
    finishedSent                                        = false;
    decodedMilliseconds                                 = 0;
    playedMilliseconds                                  = 0;
    dspInitialBufferCount                               = 0;
    replacable                                          = true;

    // priority maps
    QMap<int, QUuid> decoderPriorityMap;
    QMap<int, QUuid> dspPrePriorityMap;
    QMap<int, QUuid> dspPriorityMap;

    // load plugins
    int pluginTypesToLoad = PLUGIN_TYPE_ALL ^ PLUGIN_TYPE_SOURCE;
    foreach (PluginLibsLoader::LoadedLib loadedLib, *loadedLibs) {

        // call library's plugin factory
        PluginFactoryResults plugins;
        loadedLib.pluginFactory(pluginTypesToLoad, &plugins);

        // process each plugin one by one
        foreach (QObject *plugin, plugins) {
            int pluginType;
            if (!plugin->metaObject()->invokeMethod(plugin, "pluginType", Qt::DirectConnection, Q_RETURN_ARG(int, pluginType))) {
                emit error(trackInfo.url, true, "Failed to invoke method on plugin");
            }

            switch (pluginType) {
                case PLUGIN_TYPE_DECODER:
                    setupDecoderPlugin(plugin, loadedLib.fromEasyPluginInstallDir, &decoderPriorityMap);
                    break;
                case PLUGIN_TYPE_DSP_PRE:
                    setupDspPrePlugin(plugin, loadedLib.fromEasyPluginInstallDir, &dspPrePriorityMap);
                    break;
                case PLUGIN_TYPE_DSP:
                    setupDspPlugin(plugin, loadedLib.fromEasyPluginInstallDir, &dspPriorityMap);
                    break;
                case PLUGIN_TYPE_OUTPUT:
                    setupOutputPlugin(plugin);
                    break;
                case PLUGIN_TYPE_INFO:
                    setupInfoPlugin(plugin);
                    break;
            }
        }
    }

    // make sure there's a main output (should been already set during output plugins setup, so this is just a paranoia safeguard)
    if (mainOutputId.isNull() && (outputPlugins.count() > 0)) {
        mainOutputId = outputPlugins.keys().at(0);
    }

    // convert priority maps to vectors (iterating through a map is done in ascending order, and greater number means less priority, so it works out well)
    QMap<int, QUuid>::const_iterator i;
    for (i = decoderPriorityMap.begin(); i != decoderPriorityMap.end(); ++i) {
        decoderPriority.append(i.value());
    }
    for (i = dspPrePriorityMap.begin(); i != dspPrePriorityMap.end(); ++i) {
        dspPrePriority.append(i.value());
    }
    for (i = dspPriorityMap.begin(); i != dspPriorityMap.end(); ++i) {
        dspPriority.append(i.value());
    }

    // most of the plugins are started in setStatus, except info is started immediately
    if (infoPlugins.count() > 0) {
        infoThread.start();
    }
}


// destructor
Track::~Track()
{
    // see messageFromDspPrePlugin
    dspPointers.clear();

    // stop plugins (requestInterruption is called as a courtesy to the plugin, so plugins can check isInterruptionRequested() to end long-running tasks in a clean way)
    dspPreThread.requestInterruption();
    dspPreThread.quit();
    dspPreThread.wait();
    dspThread.requestInterruption();
    dspThread.quit();
    dspThread.wait();
    outputThread.requestInterruption();
    outputThread.quit();
    outputThread.wait();
    infoThread.requestInterruption();
    infoThread.quit();
    infoThread.wait();
    decoderThread.requestInterruption();
    decoderThread.quit();
    decoderThread.wait();

    // housekeeping
    foreach (PluginWithQueue pluginData, dspPrePlugins) {
        delete pluginData.bufferQueue;
        delete pluginData.bufferMutex;
    }
    foreach (PluginWithQueue pluginData, dspPlugins) {
        delete pluginData.bufferQueue;
        delete pluginData.bufferMutex;
    }
    foreach (PluginWithQueue pluginData, outputPlugins) {
        delete pluginData.bufferQueue;
        delete pluginData.bufferMutex;
    }
}


// helper
void Track::setupDecoderPlugin(QObject *plugin, bool fromEasyPluginInstallDir, QMap<int, QUuid> *priorityMap)
{
    // get the ID of the plugin
    QUuid persistentUniqueId;
    if (!plugin->metaObject()->invokeMethod(plugin, "persistentUniqueId", Qt::DirectConnection, Q_RETURN_ARG(QUuid, persistentUniqueId))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }

    // just to be on the safe side
    if (decoderPlugins.contains(persistentUniqueId)) {
        return;
    }

    // get some basic info
    PluginNoQueue pluginData;
    pluginData.pointer = plugin;
    if (!plugin->metaObject()->invokeMethod(plugin, "pluginName", Qt::DirectConnection, Q_RETURN_ARG(QString, pluginData.name))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "pluginVersion", Qt::DirectConnection, Q_RETURN_ARG(int, pluginData.version))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "waverVersionAPICompatibility", Qt::DirectConnection, Q_RETURN_ARG(QString, pluginData.waverVersionAPICompatibility))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "hasUI", Qt::DirectConnection, Q_RETURN_ARG(bool, pluginData.hasUI))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    decoderPlugins[persistentUniqueId] = pluginData;

    // initializations
    if (!plugin->metaObject()->invokeMethod(plugin, "setUrl", Qt::DirectConnection, Q_ARG(QUrl, trackInfo.url))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.4")) {
        if (!plugin->metaObject()->invokeMethod(plugin, "setUserAgent", Qt::DirectConnection, Q_ARG(QString, Globals::userAgent()))) {
            emit error(trackInfo.url, true, "Failed to invoke method on plugin");
        }
    }

    // add to priority map
    int priorty;
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.5")) {
        if (!plugin->metaObject()->invokeMethod(plugin, "priority", Qt::DirectConnection, Q_RETURN_ARG(int, priorty))) {
            emit error(trackInfo.url, true, "Failed to invoke method on plugin");
        }
        priorty += (fromEasyPluginInstallDir ? 25000 : 0);
    }
    else {
        priorty = 50000;
    }
    while (priorityMap->contains(priorty)) {
        priorty++;
    }
    priorityMap->insert(priorty, persistentUniqueId);

    // move to appropriate thread
    plugin->moveToThread(&decoderThread);

    // connect thread signals
    connect(&decoderThread, SIGNAL(started()),  plugin, SLOT(run()));
    connect(&decoderThread, SIGNAL(finished()), plugin, SLOT(deleteLater()));

    // connect plugin signals
    connect(plugin, SIGNAL(saveConfiguration(QUuid, QJsonDocument)),   this,   SLOT(saveConfiguration(QUuid, QJsonDocument)));
    connect(plugin, SIGNAL(loadConfiguration(QUuid)),                  this,   SLOT(loadConfiguration(QUuid)));
    connect(plugin, SIGNAL(uiQml(QUuid, QString)),                     this,   SLOT(ui(QUuid, QString)));
    connect(plugin, SIGNAL(bufferAvailable(QUuid, QAudioBuffer *)),    this,   SLOT(moveBufferInQueue(QUuid, QAudioBuffer *)));
    connect(plugin, SIGNAL(finished(QUuid)),                           this,   SLOT(decoderFinished(QUuid)));
    connect(plugin, SIGNAL(error(QUuid, QString)),                     this,   SLOT(decoderError(QUuid, QString)));
    connect(plugin, SIGNAL(infoMessage(QUuid, QString)),               this,   SLOT(infoMessage(QUuid, QString)));
    connect(this,   SIGNAL(loadedConfiguration(QUuid, QJsonDocument)), plugin, SLOT(loadedConfiguration(QUuid, QJsonDocument)));
    connect(this,   SIGNAL(requestPluginUi(QUuid)),                    plugin, SLOT(getUiQml(QUuid)));
    connect(this,   SIGNAL(pluginUiResults(QUuid, QJsonDocument)),     plugin, SLOT(uiResults(QUuid, QJsonDocument)));
    connect(this,   SIGNAL(startDecode(QUuid)),                        plugin, SLOT(start(QUuid)));
    connect(this,   SIGNAL(bufferDone(QUuid, QAudioBuffer *)),         plugin, SLOT(bufferDone(QUuid, QAudioBuffer *)));
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.3")) {
        connect(plugin, SIGNAL(saveGlobalConfiguration(QUuid, QJsonDocument)),   this,   SLOT(saveGlobalConfiguration(QUuid, QJsonDocument)));
        connect(plugin, SIGNAL(loadGlobalConfiguration(QUuid)),                  this,   SLOT(loadGlobalConfiguration(QUuid)));
        connect(this,   SIGNAL(loadedGlobalConfiguration(QUuid, QJsonDocument)), plugin, SLOT(loadedGlobalConfiguration(QUuid, QJsonDocument)));
    }
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.4")) {
        connect(plugin, SIGNAL(diagnostics(QUuid, DiagnosticData)),                                 this,   SLOT(diagnostics(QUuid, DiagnosticData)));
        connect(plugin, SIGNAL(executeSql(QUuid, bool, QString, int, QString, QVariantList)),       this,   SLOT(executeSql(QUuid, bool, QString, int, QString, QVariantList)));
        connect(plugin, SIGNAL(executeGlobalSql(QUuid, bool, QString, int, QString, QVariantList)), this,   SLOT(executeGlobalSql(QUuid, bool, QString, int, QString, QVariantList)));
        connect(this,   SIGNAL(startDiagnostics(QUuid)),                                            plugin, SLOT(startDiagnostics(QUuid)));
        connect(this,   SIGNAL(stopDiagnostics(QUuid)),                                             plugin, SLOT(stopDiagnostics(QUuid)));
        connect(this,   SIGNAL(executedSqlResults(QUuid, bool, QString, int, SqlResults)),          plugin, SLOT(sqlResults(QUuid, bool, QString, int, SqlResults)));
        connect(this,   SIGNAL(executedGlobalSqlResults(QUuid, bool, QString, int, SqlResults)),    plugin, SLOT(globalSqlResults(QUuid, bool, QString, int, SqlResults)));
        connect(this,   SIGNAL(executedSqlError(QUuid, bool, QString, int, QString)),               plugin, SLOT(sqlError(QUuid, bool, QString, int, QString)));
    }
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.6")) {
        connect(plugin, SIGNAL(messageToPlugin(QUuid, QUuid, int, QVariant)),   this,   SLOT(messageToPlugin(QUuid, QUuid, int, QVariant)));
        connect(plugin, SIGNAL(castTitle(QUuid, qint64, QString)),              this,   SLOT(decoderCastTitle(QUuid, qint64, QString)));
        connect(this,   SIGNAL(messageFromPlugin(QUuid, QUuid, int, QVariant)), plugin, SLOT(messageFromPlugin(QUuid, QUuid, int, QVariant)));
    }
}


// helper
void Track::setupDspPrePlugin(QObject *plugin, bool fromEasyPluginInstallDir, QMap<int, QUuid> *priorityMap)
{
    // get the ID of the plugin
    QUuid persistentUniqueId;
    if (!plugin->metaObject()->invokeMethod(plugin, "persistentUniqueId", Qt::DirectConnection, Q_RETURN_ARG(QUuid, persistentUniqueId))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }

    // just to be on the safe side
    if (dspPrePlugins.contains(persistentUniqueId)) {
        return;
    }

    // get some basic info
    PluginWithQueue pluginData;
    pluginData.pointer = plugin;
    if (!plugin->metaObject()->invokeMethod(plugin, "pluginName", Qt::DirectConnection, Q_RETURN_ARG(QString, pluginData.name))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "pluginVersion", Qt::DirectConnection, Q_RETURN_ARG(int, pluginData.version))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "waverVersionAPICompatibility", Qt::DirectConnection, Q_RETURN_ARG(QString, pluginData.waverVersionAPICompatibility))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "hasUI", Qt::DirectConnection, Q_RETURN_ARG(bool, pluginData.hasUI))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.6")) {
        if (!plugin->metaObject()->invokeMethod(plugin, "setCast", Qt::DirectConnection, Q_ARG(bool, trackInfo.cast))) {
            emit error(trackInfo.url, true, "Failed to invoke method on plugin");
        }
    }
    pluginData.bufferQueue = new BufferQueue();
    pluginData.bufferMutex = new QMutex();
    dspPrePlugins[persistentUniqueId] = pluginData;

    // set the queue
    if (!plugin->metaObject()->invokeMethod(plugin, "setBufferQueue", Qt::DirectConnection, Q_ARG(BufferQueue *, pluginData.bufferQueue), Q_ARG(QMutex *, pluginData.bufferMutex))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }

    // add to priority map
    int priorty;
    if (!plugin->metaObject()->invokeMethod(plugin, "priority", Qt::DirectConnection, Q_RETURN_ARG(int, priorty))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    priorty += (fromEasyPluginInstallDir ? 25000 : 0);
    while (priorityMap->contains(priorty)) {
        priorty++;
    }
    priorityMap->insert(priorty, persistentUniqueId);

    // move to appropriate thread
    plugin->moveToThread(&dspPreThread);

    // connect thread signals
    connect(&dspPreThread, SIGNAL(started()),  plugin, SLOT(run()));
    connect(&dspPreThread, SIGNAL(finished()), plugin, SLOT(deleteLater()));

    // connect plugin signals
    connect(plugin, SIGNAL(saveConfiguration(QUuid, QJsonDocument)),         this,   SLOT(saveConfiguration(QUuid, QJsonDocument)));
    connect(plugin, SIGNAL(loadConfiguration(QUuid)),                        this,   SLOT(loadConfiguration(QUuid)));
    connect(plugin, SIGNAL(uiQml(QUuid, QString)),                           this,   SLOT(ui(QUuid, QString)));
    connect(plugin, SIGNAL(infoMessage(QUuid, QString)),                     this,   SLOT(infoMessage(QUuid, QString)));
    connect(plugin, SIGNAL(requestFadeIn(QUuid, qint64)),                    this,   SLOT(dspPreRequestFadeIn(QUuid, qint64)));
    connect(plugin, SIGNAL(requestFadeInForNextTrack(QUuid, qint64)),        this,   SLOT(dspPreRequestFadeInForNextTrack(QUuid, qint64)));
    connect(plugin, SIGNAL(requestInterrupt(QUuid, qint64, bool)),           this,   SLOT(dspPreRequestInterrupt(QUuid, qint64, bool)));
    connect(plugin, SIGNAL(requestAboutToFinishSend(QUuid, qint64)),         this,   SLOT(dspPreRequestAboutToFinishSend(QUuid, qint64)));
    connect(plugin, SIGNAL(bufferDone(QUuid, QAudioBuffer *)),               this,   SLOT(moveBufferInQueue(QUuid, QAudioBuffer *)));
    connect(this,   SIGNAL(loadedConfiguration(QUuid, QJsonDocument)),       plugin, SLOT(loadedConfiguration(QUuid, QJsonDocument)));
    connect(this,   SIGNAL(requestPluginUi(QUuid)),                          plugin, SLOT(getUiQml(QUuid)));
    connect(this,   SIGNAL(pluginUiResults(QUuid, QJsonDocument)),           plugin, SLOT(uiResults(QUuid, QJsonDocument)));
    connect(this,   SIGNAL(bufferAvailable(QUuid)),                          plugin, SLOT(bufferAvailable(QUuid)));
    connect(this,   SIGNAL(decoderDone(QUuid)),                              plugin, SLOT(decoderDone(QUuid)));
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.3")) {
        connect(plugin, SIGNAL(saveGlobalConfiguration(QUuid, QJsonDocument)),   this,   SLOT(saveGlobalConfiguration(QUuid, QJsonDocument)));
        connect(plugin, SIGNAL(loadGlobalConfiguration(QUuid)),                  this,   SLOT(loadGlobalConfiguration(QUuid)));
        connect(this,   SIGNAL(loadedGlobalConfiguration(QUuid, QJsonDocument)), plugin, SLOT(loadedGlobalConfiguration(QUuid, QJsonDocument)));
    }
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.4")) {
        connect(plugin, SIGNAL(diagnostics(QUuid, DiagnosticData)),                                 this,   SLOT(diagnostics(QUuid, DiagnosticData)));
        connect(plugin, SIGNAL(executeSql(QUuid, bool, QString, int, QString, QVariantList)),       this,   SLOT(executeSql(QUuid, bool, QString, int, QString, QVariantList)));
        connect(plugin, SIGNAL(executeGlobalSql(QUuid, bool, QString, int, QString, QVariantList)), this,   SLOT(executeGlobalSql(QUuid, bool, QString, int, QString, QVariantList)));
        connect(plugin, SIGNAL(requestAboutToFinishSendForPreviousTrack(QUuid, qint64)),            this,   SLOT(dspPreRequestAboutToFinishSendForPreviousTrack(QUuid, qint64)));
        connect(this,   SIGNAL(startDiagnostics(QUuid)),                                            plugin, SLOT(startDiagnostics(QUuid)));
        connect(this,   SIGNAL(stopDiagnostics(QUuid)),                                             plugin, SLOT(stopDiagnostics(QUuid)));
        connect(this,   SIGNAL(executedSqlResults(QUuid, bool, QString, int, SqlResults)),          plugin, SLOT(sqlResults(QUuid, bool, QString, int, SqlResults)));
        connect(this,   SIGNAL(executedGlobalSqlResults(QUuid, bool, QString, int, SqlResults)),    plugin, SLOT(globalSqlResults(QUuid, bool, QString, int, SqlResults)));
        connect(this,   SIGNAL(executedSqlError(QUuid, bool, QString, int, QString)),               plugin, SLOT(sqlError(QUuid, bool, QString, int, QString)));
    }
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.6")) {
        connect(plugin, SIGNAL(messageToPlugin(QUuid, QUuid, int, QVariant)),   this,   SLOT(messageToPlugin(QUuid, QUuid, int, QVariant)));
        connect(this,   SIGNAL(messageFromPlugin(QUuid, QUuid, int, QVariant)), plugin, SLOT(messageFromPlugin(QUuid, QUuid, int, QVariant)));
    }
    else {
        connect(plugin, SIGNAL(messageToDspPlugin(QUuid, QUuid, int, QVariant)), this, SLOT(dspPreMessageToDspPlugin(QUuid, QUuid, int, QVariant)));
    }
}


// helper
void Track::setupDspPlugin(QObject *plugin, bool fromEasyPluginInstallDir, QMap<int, QUuid> *priorityMap)
{
    // get the ID of the plugin
    QUuid persistentUniqueId;
    if (!plugin->metaObject()->invokeMethod(plugin, "persistentUniqueId", Qt::DirectConnection, Q_RETURN_ARG(QUuid, persistentUniqueId))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }

    // just to be on the safe side
    if (dspPlugins.contains(persistentUniqueId)) {
        return;
    }

    // need to keep these pointers, see messageFromDspPrePlugin
    dspPointers.append(plugin);

    // get some basic info
    PluginWithQueue pluginData;
    pluginData.pointer = plugin;
    if (!plugin->metaObject()->invokeMethod(plugin, "pluginName", Qt::DirectConnection, Q_RETURN_ARG(QString, pluginData.name))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "pluginVersion", Qt::DirectConnection, Q_RETURN_ARG(int, pluginData.version))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "waverVersionAPICompatibility", Qt::DirectConnection, Q_RETURN_ARG(QString, pluginData.waverVersionAPICompatibility))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "hasUI", Qt::DirectConnection, Q_RETURN_ARG(bool, pluginData.hasUI))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.6")) {
        if (!plugin->metaObject()->invokeMethod(plugin, "setCast", Qt::DirectConnection, Q_ARG(bool, trackInfo.cast))) {
            emit error(trackInfo.url, true, "Failed to invoke method on plugin");
        }
    }
    pluginData.bufferQueue = new BufferQueue();
    pluginData.bufferMutex = new QMutex();
    dspPlugins[persistentUniqueId] = pluginData;

    // set the queue
    if (!plugin->metaObject()->invokeMethod(plugin, "setBufferQueue", Qt::DirectConnection, Q_ARG(BufferQueue *, pluginData.bufferQueue), Q_ARG(QMutex *, pluginData.bufferMutex))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }

    // add to priority map
    int priorty;
    if (!plugin->metaObject()->invokeMethod(plugin, "priority", Qt::DirectConnection, Q_RETURN_ARG(int, priorty))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    priorty += (fromEasyPluginInstallDir ? 25000 : 0);
    while (priorityMap->contains(priorty)) {
        priorty++;
    }
    priorityMap->insert(priorty, persistentUniqueId);

    // move to appropriate thread
    plugin->moveToThread(&dspThread);

    // connect thread signals
    connect(&dspThread, SIGNAL(started()),  plugin, SLOT(run()));
    connect(&dspThread, SIGNAL(finished()), plugin, SLOT(deleteLater()));

    // connect plugin signals
    connect(plugin, SIGNAL(saveConfiguration(QUuid, QJsonDocument)),              this,   SLOT(saveConfiguration(QUuid, QJsonDocument)));
    connect(plugin, SIGNAL(loadConfiguration(QUuid)),                             this,   SLOT(loadConfiguration(QUuid)));
    connect(plugin, SIGNAL(uiQml(QUuid, QString)),                                this,   SLOT(ui(QUuid, QString)));
    connect(plugin, SIGNAL(infoMessage(QUuid, QString)),                          this,   SLOT(infoMessage(QUuid, QString)));
    connect(plugin, SIGNAL(bufferDone(QUuid, QAudioBuffer *)),                    this,   SLOT(moveBufferInQueue(QUuid, QAudioBuffer *)));
    connect(this,   SIGNAL(loadedConfiguration(QUuid, QJsonDocument)),            plugin, SLOT(loadedConfiguration(QUuid, QJsonDocument)));
    connect(this,   SIGNAL(requestPluginUi(QUuid)),                               plugin, SLOT(getUiQml(QUuid)));
    connect(this,   SIGNAL(pluginUiResults(QUuid, QJsonDocument)),                plugin, SLOT(uiResults(QUuid, QJsonDocument)));
    connect(this,   SIGNAL(bufferAvailable(QUuid)),                               plugin, SLOT(bufferAvailable(QUuid)));
    connect(this,   SIGNAL(playBegin(QUuid)),                                     plugin, SLOT(playBegin(QUuid)));
    connect(this,   SIGNAL(messageFromDspPrePlugin(QUuid, QUuid, int, QVariant)), plugin, SLOT(messageFromDspPrePlugin(QUuid, QUuid, int, QVariant)));
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.3")) {
        connect(plugin, SIGNAL(saveGlobalConfiguration(QUuid, QJsonDocument)),   this,   SLOT(saveGlobalConfiguration(QUuid, QJsonDocument)));
        connect(plugin, SIGNAL(loadGlobalConfiguration(QUuid)),                  this,   SLOT(loadGlobalConfiguration(QUuid)));
        connect(this,   SIGNAL(loadedGlobalConfiguration(QUuid, QJsonDocument)), plugin, SLOT(loadedGlobalConfiguration(QUuid, QJsonDocument)));
    }
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.4")) {
        connect(plugin, SIGNAL(diagnostics(QUuid, DiagnosticData)),                                 this,   SLOT(diagnostics(QUuid, DiagnosticData)));
        connect(plugin, SIGNAL(executeSql(QUuid, bool, QString, int, QString, QVariantList)),       this,   SLOT(executeSql(QUuid, bool, QString, int, QString, QVariantList)));
        connect(plugin, SIGNAL(executeGlobalSql(QUuid, bool, QString, int, QString, QVariantList)), this,   SLOT(executeGlobalSql(QUuid, bool, QString, int, QString, QVariantList)));
        connect(this,   SIGNAL(startDiagnostics(QUuid)),                                            plugin, SLOT(startDiagnostics(QUuid)));
        connect(this,   SIGNAL(stopDiagnostics(QUuid)),                                             plugin, SLOT(stopDiagnostics(QUuid)));
        connect(this,   SIGNAL(executedSqlResults(QUuid, bool, QString, int, SqlResults)),          plugin, SLOT(sqlResults(QUuid, bool, QString, int, SqlResults)));
        connect(this,   SIGNAL(executedGlobalSqlResults(QUuid, bool, QString, int, SqlResults)),    plugin, SLOT(globalSqlResults(QUuid, bool, QString, int, SqlResults)));
        connect(this,   SIGNAL(executedSqlError(QUuid, bool, QString, int, QString)),               plugin, SLOT(sqlError(QUuid, bool, QString, int, QString)));
    }
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.6")) {
        connect(plugin, SIGNAL(messageToPlugin(QUuid, QUuid, int, QVariant)),   this,   SLOT(messageToPlugin(QUuid, QUuid, int, QVariant)));
        connect(this,   SIGNAL(messageFromPlugin(QUuid, QUuid, int, QVariant)), plugin, SLOT(messageFromPlugin(QUuid, QUuid, int, QVariant)));
    }
}


// helper
void Track::setupOutputPlugin(QObject *plugin)
{
    // get the ID of the plugin
    QUuid persistentUniqueId;
    if (!plugin->metaObject()->invokeMethod(plugin, "persistentUniqueId", Qt::DirectConnection, Q_RETURN_ARG(QUuid, persistentUniqueId))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }

    // just to be on the safe side
    if (outputPlugins.contains(persistentUniqueId)) {
        return;
    }

    // get some basic info
    PluginWithQueue pluginData;
    pluginData.pointer = plugin;
    if (!plugin->metaObject()->invokeMethod(plugin, "pluginName", Qt::DirectConnection, Q_RETURN_ARG(QString, pluginData.name))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "pluginVersion", Qt::DirectConnection, Q_RETURN_ARG(int, pluginData.version))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "waverVersionAPICompatibility", Qt::DirectConnection, Q_RETURN_ARG(QString, pluginData.waverVersionAPICompatibility))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "hasUI", Qt::DirectConnection, Q_RETURN_ARG(bool, pluginData.hasUI))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    pluginData.bufferQueue = new BufferQueue();
    pluginData.bufferMutex = new QMutex();
    outputPlugins[persistentUniqueId] = pluginData;

    // set the queue
    if (!plugin->metaObject()->invokeMethod(plugin, "setBufferQueue", Qt::DirectConnection, Q_ARG(BufferQueue *, pluginData.bufferQueue), Q_ARG(QMutex *, pluginData.bufferMutex))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }

    // set main output
    bool isMainOutput;
    if (!plugin->metaObject()->invokeMethod(plugin, "isMainOutput", Qt::DirectConnection, Q_RETURN_ARG(bool, isMainOutput))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (isMainOutput && mainOutputId.isNull()) {
        mainOutputId = persistentUniqueId;
    }

    // move to appropriate thread
    plugin->moveToThread(&outputThread);

    // connect thread signals
    connect(&outputThread, SIGNAL(started()),  plugin, SLOT(run()));
    connect(&outputThread, SIGNAL(finished()), plugin, SLOT(deleteLater()));

    // connect plugin signals
    connect(plugin, SIGNAL(saveConfiguration(QUuid, QJsonDocument)),   this,   SLOT(saveConfiguration(QUuid, QJsonDocument)));
    connect(plugin, SIGNAL(loadConfiguration(QUuid)),                  this,   SLOT(loadConfiguration(QUuid)));
    connect(plugin, SIGNAL(uiQml(QUuid, QString)),                     this,   SLOT(ui(QUuid, QString)));
    connect(plugin, SIGNAL(infoMessage(QUuid, QString)),               this,   SLOT(infoMessage(QUuid, QString)));
    connect(plugin, SIGNAL(bufferDone(QUuid, QAudioBuffer *)),         this,   SLOT(moveBufferInQueue(QUuid, QAudioBuffer *)));
    connect(plugin, SIGNAL(positionChanged(QUuid, qint64)),            this,   SLOT(outputPositionChanged(QUuid, qint64)));
    connect(plugin, SIGNAL(bufferUnderrun(QUuid)),                     this,   SLOT(outputBufferUnderrun(QUuid)));
    connect(plugin, SIGNAL(error(QUuid, QString)),                     this,   SLOT(outputError(QUuid, QString)));
    connect(this,   SIGNAL(loadedConfiguration(QUuid, QJsonDocument)), plugin, SLOT(loadedConfiguration(QUuid, QJsonDocument)));
    connect(this,   SIGNAL(requestPluginUi(QUuid)),                    plugin, SLOT(getUiQml(QUuid)));
    connect(this,   SIGNAL(pluginUiResults(QUuid, QJsonDocument)),     plugin, SLOT(uiResults(QUuid, QJsonDocument)));
    connect(this,   SIGNAL(bufferAvailable(QUuid)),                    plugin, SLOT(bufferAvailable(QUuid)));
    connect(this,   SIGNAL(pause(QUuid)),                              plugin, SLOT(pause(QUuid)));
    connect(this,   SIGNAL(resume(QUuid)),                             plugin, SLOT(resume(QUuid)));
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.3")) {
        connect(plugin, SIGNAL(saveGlobalConfiguration(QUuid, QJsonDocument)),   this,   SLOT(saveGlobalConfiguration(QUuid, QJsonDocument)));
        connect(plugin, SIGNAL(loadGlobalConfiguration(QUuid)),                  this,   SLOT(loadGlobalConfiguration(QUuid)));
        connect(this,   SIGNAL(loadedGlobalConfiguration(QUuid, QJsonDocument)), plugin, SLOT(loadedGlobalConfiguration(QUuid, QJsonDocument)));
    }
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.4")) {
        connect(plugin, SIGNAL(diagnostics(QUuid, DiagnosticData)),                                 this,   SLOT(diagnostics(QUuid, DiagnosticData)));
        connect(plugin, SIGNAL(executeSql(QUuid, bool, QString, int, QString, QVariantList)),       this,   SLOT(executeSql(QUuid, bool, QString, int, QString, QVariantList)));
        connect(plugin, SIGNAL(executeGlobalSql(QUuid, bool, QString, int, QString, QVariantList)), this,   SLOT(executeGlobalSql(QUuid, bool, QString, int, QString, QVariantList)));
        connect(this,   SIGNAL(startDiagnostics(QUuid)),                                            plugin, SLOT(startDiagnostics(QUuid)));
        connect(this,   SIGNAL(stopDiagnostics(QUuid)),                                             plugin, SLOT(stopDiagnostics(QUuid)));
        connect(this,   SIGNAL(executedSqlResults(QUuid, bool, QString, int, SqlResults)),          plugin, SLOT(sqlResults(QUuid, bool, QString, int, SqlResults)));
        connect(this,   SIGNAL(executedGlobalSqlResults(QUuid, bool, QString, int, SqlResults)),    plugin, SLOT(globalSqlResults(QUuid, bool, QString, int, SqlResults)));
        connect(this,   SIGNAL(executedSqlError(QUuid, bool, QString, int, QString)),               plugin, SLOT(sqlError(QUuid, bool, QString, int, QString)));
    }
    if (!PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.5")) {
        connect(plugin, SIGNAL(fadeInComplete(QUuid)),  this,   SLOT(outputFadeInComplete(QUuid)));
        connect(plugin, SIGNAL(fadeOutComplete(QUuid)), this,   SLOT(outputFadeOutComplete(QUuid)));
        connect(this,   SIGNAL(fadeIn(QUuid, int)),     plugin, SLOT(fadeIn(QUuid, int)));
        connect(this,   SIGNAL(fadeOut(QUuid, int)),    plugin, SLOT(fadeOut(QUuid, int)));
    }
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.6")) {
        connect(plugin, SIGNAL(messageToPlugin(QUuid, QUuid, int, QVariant)),   this,   SLOT(messageToPlugin(QUuid, QUuid, int, QVariant)));
        connect(plugin, SIGNAL(window(QString)),                                this,   SLOT(window(QString)));
        connect(this,   SIGNAL(messageFromPlugin(QUuid, QUuid, int, QVariant)), plugin, SLOT(messageFromPlugin(QUuid, QUuid, int, QVariant)));
        connect(this,   SIGNAL(mainOutputPosition(qint64)),                     plugin, SLOT(mainOutputPosition(qint64)));
    }
}


// helper
void Track::setupInfoPlugin(QObject *plugin)
{
    // get the ID of the plugin
    QUuid persistentUniqueId;
    if (!plugin->metaObject()->invokeMethod(plugin, "persistentUniqueId", Qt::DirectConnection, Q_RETURN_ARG(QUuid, persistentUniqueId))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }

    // just to be on the safe side
    if (infoPlugins.contains(persistentUniqueId)) {
        return;
    }

    // get some basic info
    PluginNoQueue pluginData;
    pluginData.pointer = plugin;
    if (!plugin->metaObject()->invokeMethod(plugin, "pluginName", Qt::DirectConnection, Q_RETURN_ARG(QString, pluginData.name))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "pluginVersion", Qt::DirectConnection, Q_RETURN_ARG(int, pluginData.version))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "waverVersionAPICompatibility", Qt::DirectConnection, Q_RETURN_ARG(QString, pluginData.waverVersionAPICompatibility))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (!plugin->metaObject()->invokeMethod(plugin, "hasUI", Qt::DirectConnection, Q_RETURN_ARG(bool, pluginData.hasUI))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    infoPlugins[persistentUniqueId] = pluginData;

    // initializations
    if (!plugin->metaObject()->invokeMethod(plugin, "setUrl", Qt::DirectConnection, Q_ARG(QUrl, trackInfo.url))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.4")) {
        if (!plugin->metaObject()->invokeMethod(plugin, "setUserAgent", Qt::DirectConnection, Q_ARG(QString, Globals::userAgent()))) {
            emit error(trackInfo.url, true, "Failed to invoke method on plugin");
        }
    }

    // move to appropriate thread
    plugin->moveToThread(&infoThread);

    // connect thread signals
    connect(&infoThread, SIGNAL(started()),  plugin, SLOT(run()));
    connect(&infoThread, SIGNAL(finished()), plugin, SLOT(deleteLater()));

    // connect plugin signals
    connect(plugin, SIGNAL(saveConfiguration(QUuid, QJsonDocument)),   this,   SLOT(saveConfiguration(QUuid, QJsonDocument)));
    connect(plugin, SIGNAL(loadConfiguration(QUuid)),                  this,   SLOT(loadConfiguration(QUuid)));
    connect(plugin, SIGNAL(uiQml(QUuid, QString)),                     this,   SLOT(ui(QUuid, QString)));
    connect(plugin, SIGNAL(infoMessage(QUuid, QString)),               this,   SLOT(infoMessage(QUuid, QString)));
    connect(plugin, SIGNAL(updateTrackInfo(QUuid, TrackInfo)),         this,   SLOT(infoUpdateTrackInfo(QUuid, TrackInfo)));
    connect(this,   SIGNAL(loadedConfiguration(QUuid, QJsonDocument)), plugin, SLOT(loadedConfiguration(QUuid, QJsonDocument)));
    connect(this,   SIGNAL(requestPluginUi(QUuid)),                    plugin, SLOT(getUiQml(QUuid)));
    connect(this,   SIGNAL(pluginUiResults(QUuid, QJsonDocument)),     plugin, SLOT(uiResults(QUuid, QJsonDocument)));
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.3")) {
        connect(plugin, SIGNAL(saveGlobalConfiguration(QUuid, QJsonDocument)),   this,   SLOT(saveGlobalConfiguration(QUuid, QJsonDocument)));
        connect(plugin, SIGNAL(loadGlobalConfiguration(QUuid)),                  this,   SLOT(loadGlobalConfiguration(QUuid)));
        connect(this,   SIGNAL(loadedGlobalConfiguration(QUuid, QJsonDocument)), plugin, SLOT(loadedGlobalConfiguration(QUuid, QJsonDocument)));
    }
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.4")) {
        connect(plugin, SIGNAL(diagnostics(QUuid, DiagnosticData)),                                 this,   SLOT(diagnostics(QUuid, DiagnosticData)));
        connect(plugin, SIGNAL(executeSql(QUuid, bool, QString, int, QString, QVariantList)),       this,   SLOT(executeSql(QUuid, bool, QString, int, QString, QVariantList)));
        connect(plugin, SIGNAL(executeGlobalSql(QUuid, bool, QString, int, QString, QVariantList)), this,   SLOT(executeGlobalSql(QUuid, bool, QString, int, QString, QVariantList)));
        connect(this,   SIGNAL(startDiagnostics(QUuid)),                                            plugin, SLOT(startDiagnostics(QUuid)));
        connect(this,   SIGNAL(stopDiagnostics(QUuid)),                                             plugin, SLOT(stopDiagnostics(QUuid)));
        connect(this,   SIGNAL(executedSqlResults(QUuid, bool, QString, int, SqlResults)),          plugin, SLOT(sqlResults(QUuid, bool, QString, int, SqlResults)));
        connect(this,   SIGNAL(executedGlobalSqlResults(QUuid, bool, QString, int, SqlResults)),    plugin, SLOT(globalSqlResults(QUuid, bool, QString, int, SqlResults)));
        connect(this,   SIGNAL(executedSqlError(QUuid, bool, QString, int, QString)),               plugin, SLOT(sqlError(QUuid, bool, QString, int, QString)));
    }
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.6")) {
        connect(plugin, SIGNAL(messageToPlugin(QUuid, QUuid, int, QVariant)),   this,   SLOT(messageToPlugin(QUuid, QUuid, int, QVariant)));
        connect(this,   SIGNAL(messageFromPlugin(QUuid, QUuid, int, QVariant)), plugin, SLOT(messageFromPlugin(QUuid, QUuid, int, QVariant)));
        connect(this,   SIGNAL(getInfo(QUuid, TrackInfo, QVariantHash)),        plugin, SLOT(getInfo(QUuid, TrackInfo, QVariantHash)));
        connect(this,   SIGNAL(trackAction(QUuid, int, TrackInfo)),     plugin, SLOT(action(QUuid, int, TrackInfo)));
    }
    else {
        connect(this,   SIGNAL(getInfo(QUuid, TrackInfo)),   plugin, SLOT(getInfo(QUuid, TrackInfo)));
        connect(plugin, SIGNAL(addInfoHtml(QUuid, QString)), this,   SLOT(infoAddInfoHtml(QUuid, QString)));
    }
}


// public method
Track::Status Track::status()
{
    return currentStatus;
}


// public method
void Track::setStatus(Status status)
{
    // there are some rules

    if (status == Idle) {
        return;
    }

    if ((status == Decoding) && (currentStatus == Idle)) {
        decoderThread.start();
        dspPreThread.start();

        if (currentDecoderId.isNull() && (decoderPriority.count() > 0)) {
            currentDecoderId    = decoderPriority.at(0);
            decodedMilliseconds = 0;
            emit startDecode(currentDecoderId);
        }

        currentStatus = Decoding;
        return;
    }

    if ((status == Playing) && (currentStatus == Idle)) {
        decoderThread.start();
        dspPreThread.start();
        dspThread.start();
        outputThread.start();

        if (currentDecoderId.isNull() && (decoderPriority.count() > 0)) {
            currentDecoderId    = decoderPriority.at(0);
            decodedMilliseconds = 0;
            emit startDecode(currentDecoderId);
        }

        foreach (QUuid dspPluginId, dspPlugins.keys()) {
            emit playBegin(dspPluginId);
        }

        if (trackInfo.cast || (fadeInRequested && !isFadeForbidden())) {
            fadeDirection  = FADE_DIRECTION_IN;
            fadePercent    = 0;
            fadeSeconds    = (fadeInRequestedMilliseconds == 0 ? INTERRUPT_FADE_SECONDS : fadeInRequestedMilliseconds / 1000);
            fadeFrameCount = 0;
        }

        currentStatus = Playing;

        sendLoadedPlugins();
        sendLoadedPluginsWithUI();

        foreach (QUuid uniqueId, infoPlugins.keys()) {
            emit getInfo(uniqueId, this->trackInfo);
            emit getInfo(uniqueId, this->trackInfo, this->additionalInfo);
        }

        return;
    }

    if ((status == Playing) && (currentStatus == Decoding)) {
        dspThread.start();
        outputThread.start();

        foreach (QUuid dspPluginId, dspPlugins.keys()) {
            emit playBegin(dspPluginId);
        }

        if (trackInfo.cast || (fadeInRequested && !isFadeForbidden())) {
            fadeDirection  = FADE_DIRECTION_IN;
            fadePercent    = 0;
            fadeSeconds    = (fadeInRequestedMilliseconds == 0 ? INTERRUPT_FADE_SECONDS : fadeInRequestedMilliseconds / 1000);
            fadeFrameCount = 0;
        }

        if (dspPlugins.count() > 0) {
            QUuid firstDspPluginId = dspPriority.at(0);
            if (dspPlugins.value(firstDspPluginId).bufferQueue->count() > 0) {
                emit bufferAvailable(firstDspPluginId);
            }
        }

        foreach (QUuid outputPluginId, outputPlugins.keys()) {
            if ((outputPlugins.value(outputPluginId).bufferQueue->count() > 0)) {
                emit bufferAvailable(outputPluginId);
            }
        }

        currentStatus = Playing;

        sendLoadedPlugins();
        sendLoadedPluginsWithUI();

        foreach (QUuid uniqueId, infoPlugins.keys()) {
            emit getInfo(uniqueId, this->trackInfo);
            emit getInfo(uniqueId, this->trackInfo, this->additionalInfo);
        }

        return;
    }

    if ((status == Paused) && (currentStatus == Playing)) {
        foreach (QUuid pluginId, outputPlugins.keys()) {
            emit pause(pluginId);
        }

        currentStatus = Paused;
        return;
    }

    if ((status == Playing) && (currentStatus == Paused)) {
        fadeDirection  = FADE_DIRECTION_IN;
        fadePercent    = 0;
        fadeSeconds    = 2;
        fadeFrameCount = 0;
        foreach (QUuid pluginId, outputPlugins.keys()) {
            emit resume(pluginId);
        }

        currentStatus = Playing;
        return;
    }
}


// public method
TrackInfo Track::getTrackInfo()
{
    return trackInfo;
}


// public method
QVariantHash Track::getAdditionalInfo()
{
    return additionalInfo;
}


// public method
void Track::addPictures(QVector<QUrl> pictures)
{
    foreach (QUrl url, pictures) {
        if (!trackInfo.pictures.contains(url)) {
            trackInfo.pictures.append(url);
        }
    }
}


// public method
QUuid Track::getSourcePluginId()
{
    return sourcePluginId;
}


// public method
bool Track::getFadeInRequested()
{
    return fadeInRequested;
}


// public method
qint64 Track::getFadeInRequestedMilliseconds()
{
    return fadeInRequestedMilliseconds;
}


// public method
bool Track::getNextTrackFadeInRequested()
{
    return nextTrackFadeInRequested;
}


// public method
qint64 Track::getNextTrackFadeInRequestedMilliseconds()
{
    return nextTrackFadeInRequestedMilliseconds;
}


// public method
bool Track::getPreviousTrackAboutToFinishSendRequested()
{
    return previousTrackAboutToFinishSendRequested;
}


// public method
qint64 Track::getPreviousTrackAboutToFinishSendRequestedMilliseconds()
{
    return previousTrackAboutToFinishSendRequestedMilliseconds;
}


// public method
void Track::startWithFadeIn(qint64 lengthMilliseconds)
{
    if (isFadeForbidden()) {
        startWithoutFadeIn();
        return;
    }

    // this has effect only when the track starts to play
    fadeInRequested = true;
    if (fadeInRequestedMilliseconds < lengthMilliseconds) {
        fadeInRequestedMilliseconds = lengthMilliseconds;
    }
}


// public method
void Track::startWithoutFadeIn()
{
    if (fadeInRequestedInternal && !isFadeForbidden()) {
        fadeInRequestedMilliseconds = fadeInRequestedInternalMilliseconds;
        return;
    }

    fadeInRequested             = false;
    fadeInRequestedMilliseconds = 0;
}


// public method
void Track::setAboutToFinishSend(qint64 beforeEndMillisecods)
{
    if (trackInfo.cast || !decodingDone || isFadeForbidden()) {
        return;
    }

    if ((interruptAboutToFinishSendPosition == 0) || (interruptAboutToFinishSendPosition > (decodedMilliseconds - beforeEndMillisecods))) {
        interruptAboutToFinishSendPosition = decodedMilliseconds - beforeEndMillisecods;
    }
}


// public method
void Track::resetAboutToFinishSend()
{
    if (isFadeForbidden()) {
        interruptAboutToFinishSendPosition = 0;
    }

    interruptAboutToFinishSendPosition = interruptAboutToFinishSendPositionInternal;
}


// public method
void Track::addMoreToCastPlaytime()
{
    if (currentCastPlaytimeMilliseconds < ((24 * 60 * 60 * 1000) - CAST_ADD_PLAYTIME)) {
        currentCastPlaytimeMilliseconds += CAST_ADD_PLAYTIME;
    }
}


// public method
void Track::addALotToCastPlaytime()
{
    currentCastPlaytimeMilliseconds = 24 * 60 * 60 * 1000;
}


// public method
void Track::setReplacable(bool replacable)
{
    this->replacable = replacable;
}


// public method
bool Track::isReplacable()
{
    return replacable;
}


// public method
void Track::interrupt()
{
    if ((currentStatus == Playing) && (!interruptInProgress)) {
        interruptInProgress = true;

        fadeDirection  = FADE_DIRECTION_OUT;
        fadePercent    = 100;
        fadeSeconds    = INTERRUPT_FADE_SECONDS;
        fadeFrameCount = 0;

        return;
    }

    if (!interruptInProgress) {
        sendFinished();
    }
}


// private method
void Track::sendLoadedPlugins()
{
    PluginList plugins;

    foreach (QUuid id, decoderPlugins.keys()) {
        plugins.insert(id, formatPluginName(decoderPlugins.value(id)));
    }
    foreach (QUuid id, dspPrePlugins.keys()) {
        plugins.insert(id, formatPluginName(dspPrePlugins.value(id)));
    }
    foreach (QUuid id, dspPlugins.keys()) {
        plugins.insert(id, formatPluginName(dspPlugins.value(id)));
    }
    foreach (QUuid id, outputPlugins.keys()) {
        plugins.insert(id, formatPluginName(outputPlugins.value(id)));
    }
    foreach (QUuid id, infoPlugins.keys()) {
        plugins.insert(id, formatPluginName(infoPlugins.value(id)));
    }

    emit loadedPlugins(plugins);
}


// private method
void Track::sendLoadedPluginsWithUI()
{
    PluginList pluginsWithUI;

    foreach (QUuid id, decoderPlugins.keys()) {
        if (decoderPlugins.value(id).hasUI) {
            pluginsWithUI.insert(id, formatPluginName(decoderPlugins.value(id)));
        }
    }
    foreach (QUuid id, dspPrePlugins.keys()) {
        if (dspPrePlugins.value(id).hasUI) {
            pluginsWithUI.insert(id, formatPluginName(dspPrePlugins.value(id)));
        }
    }
    foreach (QUuid id, dspPlugins.keys()) {
        if (dspPlugins.value(id).hasUI) {
            pluginsWithUI.insert(id, formatPluginName(dspPlugins.value(id)));
        }
    }
    foreach (QUuid id, outputPlugins.keys()) {
        if (outputPlugins.value(id).hasUI) {
            pluginsWithUI.insert(id, formatPluginName(outputPlugins.value(id)));
        }
    }
    foreach (QUuid id, infoPlugins.keys()) {
        if (infoPlugins.value(id).hasUI) {
            pluginsWithUI.insert(id, formatPluginName(infoPlugins.value(id)));
        }
    }

    emit loadedPluginsWithUI(pluginsWithUI);
}


// private method
void Track::sendFinished()
{
    if (finishedSent) {
        return;
    }

    emit finished(trackInfo.url);
    finishedSent = true;
}


// server signal handler
void Track::loadedPluginSettings(QUuid id, QJsonDocument settings)
{
    // re-emit for plugins
    emit loadedConfiguration(id, settings);
}


// server signal handler
void Track::loadedPluginGlobalSettings(QUuid id, QJsonDocument settings)
{
    // re-emit for plugins
    emit loadedGlobalConfiguration(id, settings);
}


// server signal handler
void Track::executedPluginSqlResults(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    // re-emit for plugins
    emit executedSqlResults(uniqueId, temporary, clientIdentifier, clientSqlIdentifier, results);
}


// server signal handler
void Track::executedPluginGlobalSqlResults(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    // re-emit for plugins
    emit executedGlobalSqlResults(uniqueId, temporary, clientIdentifier, clientSqlIdentifier, results);
}


// server signal handler
void Track::executedPluginSqlError(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
{
    // re-emit for plugins
    emit executedSqlError(uniqueId, temporary, clientIdentifier, clientSqlIdentifier, error);
}


// server signal handler
void Track::requestedPluginUi(QUuid id)
{
    // just re-emit, only the plugin with the same id will respond anyways
    emit requestPluginUi(id);
}


// server signal handler
void Track::receivedPluginUiResults(QUuid uniqueId, QJsonDocument results)
{
    // just re-emit, only the plugin with the same id will respond anyways
    emit pluginUiResults(uniqueId, results);
}


// server signal handler
void Track::startPluginDiagnostics(QUuid uniquedId)
{
    emit startDiagnostics(uniquedId);
}


// server signal handler
void Track::stopPluginDiagnostics(QUuid uniquedId)
{
    emit stopDiagnostics(uniquedId);
}


// server signal handler
void Track::trackActionRequest(QUuid uniquedId, int actionKey, QUrl url)
{
    if (url != trackInfo.url) {
        return;
    }

    emit trackAction(uniquedId, actionKey, trackInfo);
}


// plugin signal handler
void Track::loadConfiguration(QUuid uniqueId)
{
    // re-emit for server
    emit loadPluginSettings(uniqueId);
}


// plugin signal handler
void Track::loadGlobalConfiguration(QUuid uniqueId)
{
    // re-emit for server
    emit loadPluginGlobalSettings(uniqueId);
}


// plugin signal handler
void Track::saveConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    // re-emit for server
    emit savePluginSettings(uniqueId, configuration);
}


// plugin signal handler
void Track::saveGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    // re-emit for server
    emit savePluginGlobalSettings(uniqueId, configuration);
}


// plugin signal handler
void Track::executeSql(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString sql, QVariantList values)
{
    // re-emit for server
    emit executeSettingsSql(uniqueId, temporary, clientIdentifier, clientSqlIdentifier, sql, values);
}


// plugin signal handler
void Track::executeGlobalSql(QUuid uniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString sql, QVariantList values)
{
    // re-emit for server
    emit executeGlobalSettingsSql(uniqueId, temporary, clientIdentifier, clientSqlIdentifier, sql, values);
}


// message from one plugin to another
void Track::messageToPlugin(QUuid uniqueId, QUuid destinationUniqueId, int messageId, QVariant value)
{
    if (decoderPlugins.contains(destinationUniqueId)) {
        if (decoderThread.isRunning()) {
            emit messageFromPlugin(destinationUniqueId, uniqueId, messageId, value);
            return;
        }
        decoderPlugins.value(destinationUniqueId).pointer->metaObject()->invokeMethod(decoderPlugins.value(destinationUniqueId).pointer, "messageFromPlugin", Qt::DirectConnection, Q_ARG(QUuid, destinationUniqueId), Q_ARG(QUuid, uniqueId), Q_ARG(int, messageId), Q_ARG(QVariant, value));
        return;
    }

    if (dspPrePlugins.contains(destinationUniqueId)) {
        if (dspPreThread.isRunning()) {
            emit messageFromPlugin(destinationUniqueId, uniqueId, messageId, value);
            return;
        }
        dspPrePlugins.value(destinationUniqueId).pointer->metaObject()->invokeMethod(dspPrePlugins.value(destinationUniqueId).pointer, "messageFromPlugin", Qt::DirectConnection, Q_ARG(QUuid, destinationUniqueId), Q_ARG(QUuid, uniqueId), Q_ARG(int, messageId), Q_ARG(QVariant, value));
        return;
    }

    if (dspPlugins.contains(destinationUniqueId)) {
        if (dspThread.isRunning()) {
            emit messageFromPlugin(destinationUniqueId, uniqueId, messageId, value);
            return;
        }
        dspPlugins.value(destinationUniqueId).pointer->metaObject()->invokeMethod(dspPlugins.value(destinationUniqueId).pointer, "messageFromPlugin", Qt::DirectConnection, Q_ARG(QUuid, destinationUniqueId), Q_ARG(QUuid, uniqueId), Q_ARG(int, messageId), Q_ARG(QVariant, value));
        return;
    }

    if (outputPlugins.contains(destinationUniqueId)) {
        if (outputThread.isRunning()) {
            emit messageFromPlugin(destinationUniqueId, uniqueId, messageId, value);
            return;
        }
        outputPlugins.value(destinationUniqueId).pointer->metaObject()->invokeMethod(outputPlugins.value(destinationUniqueId).pointer, "messageFromPlugin", Qt::DirectConnection, Q_ARG(QUuid, destinationUniqueId), Q_ARG(QUuid, uniqueId), Q_ARG(int, messageId), Q_ARG(QVariant, value));
        return;
    }

    if (infoPlugins.contains(destinationUniqueId)) {
        if (infoThread.isRunning()) {
            emit messageFromPlugin(destinationUniqueId, uniqueId, messageId, value);
            return;
        }
        infoPlugins.value(destinationUniqueId).pointer->metaObject()->invokeMethod(infoPlugins.value(destinationUniqueId).pointer, "messageFromPlugin", Qt::DirectConnection, Q_ARG(QUuid, destinationUniqueId), Q_ARG(QUuid, uniqueId), Q_ARG(int, messageId), Q_ARG(QVariant, value));
        return;
    }
}


// plugin signal handler
void Track::ui(QUuid uniqueId, QString qml)
{
    QString header;
    if (decoderPlugins.contains(uniqueId)) {
        header = formatPluginName(decoderPlugins.value(uniqueId), true);
    }
    else if (dspPrePlugins.contains(uniqueId)) {
        header = formatPluginName(dspPrePlugins.value(uniqueId), true);
    }
    else if (dspPlugins.contains(uniqueId)) {
        header = formatPluginName(dspPlugins.value(uniqueId), true);
    }
    else if (outputPlugins.contains(uniqueId)) {
        header = formatPluginName(outputPlugins.value(uniqueId), true);
    }
    else if (infoPlugins.contains(uniqueId)) {
        header = formatPluginName(infoPlugins.value(uniqueId), true);
    }

    // re-emit for server
    emit pluginUi(uniqueId, qml, header);
}


// plugin signal handler
void Track::infoMessage(QUuid uniqueId, QString message)
{
    Q_UNUSED(uniqueId);

    // this is not an error, but that's OK
    emit error(trackInfo.url, false, message);
}


// plugin signal handler
void Track::diagnostics(QUuid id, DiagnosticData diagnosticData)
{
    emit pluginDiagnostics(id, trackInfo.url, diagnosticData);
}


// plugin signal handler
void Track::window(QString qmlString)
{
    emit pluginWindow(qmlString);
}


// plugin signal handler
void Track::moveBufferInQueue(QUuid pluginId, QAudioBuffer *buffer)
{
    // buffer just got available from decoder
    if (decoderPlugins.contains(pluginId)) {

        // protection against slow signals in case a decoder errored out
        if (pluginId != currentDecoderId) {
            emit bufferDone(pluginId, buffer);
            return;
        }

        // update decoded duration
        decodedMilliseconds = (buffer->startTime() + buffer->duration()) / 1000;

        // are there any pre-dsp plugins?
        if (dspPrePlugins.count() > 0) {
            // queue to first pre-dsp plugin
            QUuid firstDspPrePluginId = dspPrePriority.at(0);
            dspPrePlugins.value(firstDspPrePluginId).bufferMutex->lock();
            dspPrePlugins.value(firstDspPrePluginId).bufferQueue->append(buffer);
            dspPrePlugins.value(firstDspPrePluginId).bufferMutex->unlock();
            emit bufferAvailable(firstDspPrePluginId);
        }

        // are there any dsp plugins?
        else if (dspPlugins.count() > 0) {

            // is this track just starting?
            if (dspInitialBufferCount < CACHE_BUFFER_COUNT) {
                // queue to first dsp plugin
                QUuid firstDspPluginId = dspPriority.at(0);
                dspPlugins.value(firstDspPluginId).bufferMutex->lock();
                dspPlugins.value(firstDspPluginId).bufferQueue->append(buffer);
                dspPlugins.value(firstDspPluginId).bufferMutex->unlock();

                // send message only if thread is fired up
                if (dspThread.isRunning()) {
                    emit bufferAvailable(firstDspPluginId);
                }

                // update counter
                dspInitialBufferCount++;
            }
            else {
                // append to temporary queue so it can be synchronized with output
                dspSynchronizerQueue.append(buffer);
            }
        }

        // are there any output plugins?
        else if (outputPlugins.count() > 0) {

            // fade
            if (fadeDirection != FADE_DIRECTION_NONE) {
                applyFade(buffer);
            }

            // initialize done counter for this buffer (this will determine if all outputs are done with this buffer)
            bufferOuputDoneCounters[buffer] = 0;

            // queue buffer for every output
            foreach (QUuid outputPluginId, outputPlugins.keys()) {
                // update done counter for this buffer
                bufferOuputDoneCounters[buffer]++;

                // queue buffer for this output
                outputPlugins.value(outputPluginId).bufferMutex->lock();
                outputPlugins.value(outputPluginId).bufferQueue->append(buffer);
                outputPlugins.value(outputPluginId).bufferMutex->unlock();

                // send message if thread is fired up
                if (outputThread.isRunning()) {
                    emit bufferAvailable(outputPluginId);
                }
            }
        }

        // there was no plugin to queue this buffer to
        else {
            // this lets the decoder free up the memory
            emit bufferDone(pluginId, buffer);
        }
        return;
    }

    // a pre-dsp plugin is done with the buffer
    if (dspPrePlugins.contains(pluginId)) {

        // is there a next pre-dsp plugin?
        int nextDspPrePluginIndex = dspPrePriority.indexOf(pluginId) + 1;
        if (nextDspPrePluginIndex < dspPrePriority.count()) {
            // queue to next pre-dsp plugin
            QUuid nextDspPrePluginId = dspPrePriority.at(nextDspPrePluginIndex);
            dspPrePlugins.value(nextDspPrePluginId).bufferMutex->lock();
            dspPrePlugins.value(nextDspPrePluginId).bufferQueue->append(buffer);
            dspPrePlugins.value(nextDspPrePluginId).bufferMutex->unlock();
            emit bufferAvailable(nextDspPrePluginId);
        }

        // are there any dsp plugins?
        else if (dspPlugins.count() > 0) {

            // is this track just starting?
            if (dspInitialBufferCount < CACHE_BUFFER_COUNT) {
                // queue to first dsp plugin
                QUuid firstDspPluginId = dspPriority.at(0);
                dspPlugins.value(firstDspPluginId).bufferMutex->lock();
                dspPlugins.value(firstDspPluginId).bufferQueue->append(buffer);
                dspPlugins.value(firstDspPluginId).bufferMutex->unlock();

                // send message only if thread is fired up
                if (dspThread.isRunning()) {
                    emit bufferAvailable(firstDspPluginId);
                }

                // update counter
                dspInitialBufferCount++;
            }
            else {
                // append to temporary queue so it can be synchronized with output
                dspSynchronizerQueue.append(buffer);
            }
        }

        // are there any output plugins?
        else if (outputPlugins.count() > 0) {

            // fade
            if (fadeDirection != FADE_DIRECTION_NONE) {
                applyFade(buffer);
            }

            // initialize done counter for this buffer (this will determine if all outputs are done with this buffer)
            bufferOuputDoneCounters[buffer] = 0;

            // queue buffer for every output
            foreach (QUuid outputPluginId, outputPlugins.keys()) {
                // update done counter for this buffer
                bufferOuputDoneCounters[buffer]++;

                // queue buffer for this output
                outputPlugins.value(outputPluginId).bufferMutex->lock();
                outputPlugins.value(outputPluginId).bufferQueue->append(buffer);
                outputPlugins.value(outputPluginId).bufferMutex->unlock();

                // send message if thread is fired up
                if (outputThread.isRunning()) {
                    emit bufferAvailable(outputPluginId);
                }
            }
        }

        // there was no plugin to queue this buffer to
        else {
            // this lets the decoder free up the memory
            emit bufferDone(currentDecoderId, buffer);
        }
        return;
    }

    // a dsp plugin is done with the buffer
    if (dspPlugins.contains(pluginId)) {

        // is there a next dsp plugin?
        int nextDspPluginIndex = dspPriority.indexOf(pluginId) + 1;
        if (nextDspPluginIndex < dspPriority.count()) {
            // queue to next dsp plugin
            QUuid nextDspPluginId = dspPriority.at(nextDspPluginIndex);
            dspPlugins.value(nextDspPluginId).bufferMutex->lock();
            dspPlugins.value(nextDspPluginId).bufferQueue->append(buffer);
            dspPlugins.value(nextDspPluginId).bufferMutex->unlock();

            // send message if thread is fired up
            if (dspThread.isRunning()) {
                emit bufferAvailable(nextDspPluginId);
            }
        }

        // are there any output plugins?
        else if (outputPlugins.count() > 0) {

            // fade
            if (fadeDirection != FADE_DIRECTION_NONE) {
                applyFade(buffer);
            }

            // initialize done counter for this buffer (this will determine if all outputs are done with this buffer)
            bufferOuputDoneCounters[buffer] = 0;

            // queue buffer for every output
            foreach (QUuid outputPluginId, outputPlugins.keys()) {
                // update done counter for this buffer
                bufferOuputDoneCounters[buffer]++;

                // queue buffer for this output
                outputPlugins.value(outputPluginId).bufferMutex->lock();
                outputPlugins.value(outputPluginId).bufferQueue->append(buffer);
                outputPlugins.value(outputPluginId).bufferMutex->unlock();

                // send message if thread is fired up
                if (outputThread.isRunning()) {
                    emit bufferAvailable(outputPluginId);
                }
            }
        }

        // there was no plugin to queue this buffer to
        else {
            // this lets the decoder free up the memory
            emit bufferDone(currentDecoderId, buffer);
        }
        return;
    }

    // an output is done with the buffer
    if (outputPlugins.contains(pluginId)) {
        // update done counter for this buffer
        bufferOuputDoneCounters[buffer]--;

        // are all outputs done with this buffer?
        if (bufferOuputDoneCounters[buffer] < 1) {
            // this lets the decoder free up the memory
            emit bufferDone(currentDecoderId, buffer);

            // housekeeping
            bufferOuputDoneCounters.remove(buffer);
        }

        // if this is the main output, and this track isn't just starting anymore, then queue the next buffer to the first dsp plugin
        if ((pluginId == mainOutputId) && (dspInitialBufferCount >= CACHE_BUFFER_COUNT)) {
            // make sure there's at least one buffer in the synchonizer queue
            if (dspSynchronizerQueue.count() > 0) {
                // this should never be null, because dspSynchronizerQueue will not contain anything unless there's at least one ready dsp plugin
                QUuid firstDspPluginId = dspPriority.at(0);

                // queue to first dsp plugin (by this time dsp thread is runnning for sure)
                dspPlugins.value(firstDspPluginId).bufferMutex->lock();
                dspPlugins.value(firstDspPluginId).bufferQueue->append(dspSynchronizerQueue.at(0));
                dspPlugins.value(firstDspPluginId).bufferMutex->unlock();
                emit bufferAvailable(firstDspPluginId);

                // don't queue the same buffer again, housekeeping
                dspSynchronizerQueue.remove(0);
            }
            else {
                // some plugin might be slow, give it another chance
                dspInitialBufferCount = 0;
            }
        }
    }
}


// decoder plugin signal handler
void Track::decoderCastTitle(QUuid uniqueId, qint64 microSecondsTimestamp, QString title)
{
    if (uniqueId != currentDecoderId) {
        return;
    }

    SHOUTcastPMCTitles.append({ microSecondsTimestamp, title });
}

// decoder plugin signal handler
void Track::decoderFinished(QUuid uniqueId)
{
    if (uniqueId != currentDecoderId) {
        return;
    }

    decodingDone = true;

    foreach (QUuid pluginId, dspPrePlugins.keys()) {
        emit decoderDone(pluginId);
    }

    if (currentStatus == Paused) {
        emit playPosition(trackInfo.url, true, trackInfo.cast ? currentCastPlaytimeMilliseconds : decodedMilliseconds, playedMilliseconds);
    }
}


// decoder plugin signal handler
void Track::decoderError(QUuid uniqueId, QString errorMessage)
{
    if (uniqueId != currentDecoderId) {
        return;
    }

    // is this pre-buffering meaning playing did not start yet
    if (currentStatus != Playing) {
        // is there a next decoder?
        int nextDecoderIndex = decoderPriority.indexOf(currentDecoderId) + 1;
        if (nextDecoderIndex < decoderPriority.count()) {
            // as far as the outside world goes, this is not an error, so let's just try the next decoder

            dspSynchronizerQueue.clear();
            foreach (PluginWithQueue pluginData, dspPrePlugins) {
                pluginData.bufferMutex->lock();
                pluginData.bufferQueue->clear();
                pluginData.bufferMutex->unlock();
            }
            foreach (PluginWithQueue pluginData, dspPlugins) {
                pluginData.bufferMutex->lock();
                pluginData.bufferQueue->clear();
                pluginData.bufferMutex->unlock();
            }
            foreach (PluginWithQueue pluginData, outputPlugins) {
                pluginData.bufferMutex->lock();
                pluginData.bufferQueue->clear();
                pluginData.bufferMutex->unlock();
            }

            currentDecoderId      = decoderPriority.at(nextDecoderIndex);
            decodedMilliseconds   = 0;
            dspInitialBufferCount = 0;
            emit startDecode(currentDecoderId);

            return;
        }

        // there are no more decoders, so this is fatal at this point
        emit error(trackInfo.url, true, errorMessage);
        return;
    }

    // already playing, are there any pcm data left to play?
    if ((decodedMilliseconds - 1000) > playedMilliseconds) {
        // this is non-fatal, let the world know anyways
        emit error(trackInfo.url, false, errorMessage);

        // treat as if decoder finished normally
        decoderFinished(currentDecoderId);

        return;
    }

    // there's nothing more to play, let this be fatal
    emit error(trackInfo.url, true, errorMessage);
}


// output signal handler
void Track::outputPositionChanged(QUuid uniqueId, qint64 posMilliseconds)
{
    if (uniqueId != mainOutputId) {
        return;
    }

    emit mainOutputPosition(posMilliseconds * 1000);

    if ((interruptPosition > 0) && (posMilliseconds >= interruptPosition)) {
        interruptPosition = 0;
        if (interruptPositionWithFadeOut) {
            interrupt();
        }
        else {
            sendFinished();
        }
    }

    if ((interruptAboutToFinishSendPosition > 0) && (posMilliseconds >= interruptAboutToFinishSendPosition)) {
        interruptAboutToFinishSendPosition         = 0;
        interruptAboutToFinishSendPositionInternal = 0;
        emit aboutToFinish(trackInfo.url);
    }

    playedMilliseconds = posMilliseconds;

    emit playPosition(trackInfo.url, trackInfo.cast ? true : decodingDone, trackInfo.cast ? currentCastPlaytimeMilliseconds : decodedMilliseconds, posMilliseconds);

    if ((SHOUTcastPMCTitles.count() > 0) && (SHOUTcastPMCTitles.first().microSecondsTimestamp <= (posMilliseconds * 1000))) {
        trackInfo.performer = SHOUTcastPMCTitles.first().title;
        SHOUTcastPMCTitles.removeFirst();
        emit trackInfoUpdated(trackInfo.url);
    }

}


// output signal handler
void Track::outputBufferUnderrun(QUuid uniqueId)
{
    if (uniqueId != mainOutputId) {
        return;
    }

    if (decodingDone && (playedMilliseconds >= (decodedMilliseconds - 1000))) {
        sendFinished();
        return;
    }

    decodedMillisecondsAtUnderrun = decodedMilliseconds;
    QTimer::singleShot(5000, this, SLOT(underrunTimeout()));
}


// timer signal handler
void Track::underrunTimeout()
{
    if (!decodingDone && (decodedMillisecondsAtUnderrun >= decodedMilliseconds)) {
        emit error(trackInfo.url, true, "Buffer underrun. Possible download interruption due to a network error.");
    }
}


// output signal handler
void Track::outputFadeInComplete(QUuid uniqueId)
{
    // noting to do yet
    Q_UNUSED(uniqueId);
}


// output signal handler
void Track::outputFadeOutComplete(QUuid uniqueId)
{
    // let the world know
    if (uniqueId != mainOutputId) {
        return;
    }

    sendFinished();
}


// output signal handler
void Track::outputError(QUuid uniqueId, QString errorMessage)
{
    errorMessage = outputPlugins.value(uniqueId).name + ": " + errorMessage;

    if (uniqueId != mainOutputId) {
        emit error(trackInfo.url, false, errorMessage);
        return;
    }

    emit error(trackInfo.url, true, errorMessage);
}


// pre-dsp signal handler
void Track::dspPreRequestFadeIn(QUuid uniqueId, qint64 lengthMilliseconds)
{
    Q_UNUSED(uniqueId);

    if (isFadeForbidden()) {
        return;
    }

    // this has effect only when the track starts to play
    fadeInRequested = true;
    if (fadeInRequestedMilliseconds < lengthMilliseconds) {
        fadeInRequestedMilliseconds = lengthMilliseconds;
    }

    fadeInRequestedInternal             = true;
    fadeInRequestedInternalMilliseconds = lengthMilliseconds;
}


// pre-dsp signal handler
void Track::dspPreRequestFadeInForNextTrack(QUuid uniqueId, qint64 lengthMilliseconds)
{
    Q_UNUSED(uniqueId);

    nextTrackFadeInRequested             = true;
    nextTrackFadeInRequestedMilliseconds = lengthMilliseconds;

    emit requestFadeInForNextTrack(trackInfo.url, lengthMilliseconds);
}


// pre-dsp signal handler
void Track::dspPreRequestInterrupt(QUuid uniqueId, qint64 posMilliseconds, bool withFadeOut)
{
    Q_UNUSED(uniqueId);

    if (isFadeForbidden()) {
        return;
    }

    // new request overwrites pervious one
    interruptPosition            = posMilliseconds;
    interruptPositionWithFadeOut = withFadeOut;
}


// pre-dsp signal handler
void Track::dspPreRequestAboutToFinishSend(QUuid uniqueId, qint64 posMilliseconds)
{
    Q_UNUSED(uniqueId);

    if (isFadeForbidden()) {
        return;
    }

    interruptAboutToFinishSendPosition         = posMilliseconds;
    interruptAboutToFinishSendPositionInternal = posMilliseconds;
}


// pre-dsp signal handler
void Track::dspPreRequestAboutToFinishSendForPreviousTrack(QUuid uniqueId, qint64 posBeforeEndMilliseconds)
{
    Q_UNUSED(uniqueId);

    previousTrackAboutToFinishSendRequested             = true;
    previousTrackAboutToFinishSendRequestedMilliseconds = posBeforeEndMilliseconds;

    emit requestAboutToFinishSendForPreviousTrack(trackInfo.url, posBeforeEndMilliseconds);
}


// pre-dsp signal handler
void Track::dspPreMessageToDspPlugin(QUuid uniqueId, QUuid destinationUniqueId, int messageId, QVariant value)
{
    if (dspThread.isRunning()) {
        emit messageFromDspPrePlugin(destinationUniqueId, uniqueId, messageId, value);
        return;
    }

    // these can come long before dsp thread is fired up
    foreach (QObject *plugin, dspPointers) {
        plugin->metaObject()->invokeMethod(plugin, "messageFromDspPrePlugin", Qt::DirectConnection, Q_ARG(QUuid, destinationUniqueId), Q_ARG(QUuid, uniqueId), Q_ARG(int, messageId), Q_ARG(QVariant, value));
    }
}


// info plugin signal handler
void Track::infoUpdateTrackInfo(QUuid uniqueId, TrackInfo trackInfo)
{
    Q_UNUSED(uniqueId);

    if (trackInfo.title.length() > 0) {
        this->trackInfo.title = trackInfo.title;
    }
    if (trackInfo.performer.length() > 0) {
        this->trackInfo.performer = trackInfo.performer;
    }
    if (trackInfo.album.length() > 0) {
        this->trackInfo.album = trackInfo.album;
    }
    if (trackInfo.year > 0) {
        this->trackInfo.year = trackInfo.year;
    }
    if (trackInfo.track > 0) {
        this->trackInfo.track = trackInfo.track;
    }

    foreach (QUrl url, trackInfo.pictures) {
        if (!this->trackInfo.pictures.contains(url)) {
            this->trackInfo.pictures.append(url);
        }
    }

    if (!trackInfo.actions.isEmpty()) {
        QVector<TrackAction> temp;
        foreach (TrackAction trackAction, this->trackInfo.actions) {
            if (trackAction.pluginId == uniqueId) {
                continue;
            }
            temp.append(trackAction);
        }
        this->trackInfo.actions.clear();
        foreach (TrackAction trackAction, temp) {
            this->trackInfo.actions.append(trackAction);
        }
        foreach (TrackAction trackAction, trackInfo.actions) {
            this->trackInfo.actions.append(trackAction);
        }
    }

    emit trackInfoUpdated(trackInfo.url);
}


// info plugin signal handler (deprecated, don't use)
void Track::infoAddInfoHtml(QUuid uniqueId, QString info)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(info);
}


// private method
bool Track::isFadeForbidden()
{
    return additionalInfo.contains("fade_forbidden") && additionalInfo.value("fade_forbidden").toInt();
}


// private method
void Track::applyFade(QAudioBuffer *buffer)
{
    // some variables that are needed
    double framesPerPercent = buffer->format().framesForDuration(fadeSeconds * 1000000) / 100;
    double framesPerSample  = 1.0 / buffer->format().channelCount();

    // this is only to speed up things inside the loop
    int dataType = 0;
    if (buffer->format().sampleType() == QAudioFormat::SignedInt) {
        if (buffer->format().sampleSize() == 8) {
            dataType = 1;
        }
        else if (buffer->format().sampleSize() == 16) {
            dataType = 2;
        }
        else if (buffer->format().sampleSize() == 32) {
            dataType = 3;
        }
    }
    else if (buffer->format().sampleType() == QAudioFormat::UnSignedInt) {
        if (buffer->format().sampleSize() == 8) {
            dataType = 4;
        }
        else if (buffer->format().sampleSize() == 16) {
            dataType = 5;
        }
        else if (buffer->format().sampleSize() == 32) {
            dataType = 6;
        }
    }

    // only one of these will be used, depending on the data type
    qint8   *int8;
    qint16  *int16;
    qint32  *int32;
    quint8  *uint8;
    quint16 *uint16;
    quint32 *uint32;

    // do the math sample by sample (simple linear fade)
    char  *data      = (char *)buffer->data();
    int    byteCount = 0;
    while (byteCount < buffer->byteCount()) {
        // not all formats supported, but most common ones are
        if (dataType != 0) {
            // calculation
            switch (dataType) {
                case 1:
                    int8  = (qint8 *)data;
                    *int8 = (fadePercent * *int8) / 100;
                    data      += 1;
                    byteCount += 1;
                    break;
                case 2:
                    int16  = (qint16 *)data;
                    *int16 = (fadePercent * *int16) / 100;
                    data      += 2;
                    byteCount += 2;
                    break;
                case 3:
                    int32  = (qint32 *)data;
                    *int32 = (fadePercent * *int32) / 100;
                    data      += 4;
                    byteCount += 4;
                    break;
                case 4:
                    uint8  = (quint8 *)data;
                    *uint8 = (fadePercent * *uint8) / 100;
                    data      += 1;
                    byteCount += 1;
                    break;
                case 5:
                    uint16  = (quint16 *)data;
                    *uint16 = (fadePercent * *uint16) / 100;
                    data      += 2;
                    byteCount += 2;
                    break;
                case 6:
                    uint32  = (quint32 *)data;
                    *uint32 = (fadePercent * *uint32) / 100;
                    data      += 4;
                    byteCount += 4;
            }
        }
        else {
            byteCount += buffer->format().sampleSize() / 8;
        }

        fadeFrameCount += framesPerSample;

        // change percentage if it's time to do that
        if (fadeFrameCount >= framesPerPercent) {
            // reset counter for next percent
            fadeFrameCount = 0;

            // fade in
            if ((fadeDirection == FADE_DIRECTION_IN) && (fadePercent < 100)) {
                fadePercent++;

                // after fade in, it is excepted that the track will play along, so stop fading when 100% is reached
                if (fadePercent == 100) {
                    fadeDirection = FADE_DIRECTION_NONE;
                }
            }

            // fade out
            if ((fadeDirection == FADE_DIRECTION_OUT) && (fadePercent > 0)) {
                fadePercent--;

                // after fade out, the track will stop, but must be through a timer to make sure this loop exits clearly
                if (fadePercent == 0) {
                    QTimer::singleShot(50, this, SLOT(sendFinished()));
                    break;
                }
            }
        }
    }
}
