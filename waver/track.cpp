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


#include "track.h"


// constructor
Track::Track(PluginLibsLoader::LoadedLibs *loadedLibs, TrackInfo trackInfo, QUuid sourcePliginId, QObject *parent) : QObject(parent)
{
    // to make debugging easier
    decoderThread.setObjectName("decoder");
    dspPreThread.setObjectName("dsp_pre");
    dspThread.setObjectName("dsp");
    outputThread.setObjectName("output");
    infoThread.setObjectName("info");

    // remember track info
    this->trackInfo      = trackInfo;
    this->sourcePliginId = sourcePliginId;

    // initializations
    currentStatus                        = Idle;
    fadeInRequested                      = false;
    fadeInRequestedInternal              = false;
    fadeInRequestedMilliseconds          = 0;
    fadeInRequestedInternalMilliseconds  = 0;
    interruptInProgress                  = false;
    interruptPosition                    = 0;
    interruptPositionWithFadeOut         = true;
    interruptAboutToFinishSendPosition   = 0;
    nextTrackFadeInRequested             = false;
    nextTrackFadeInRequestedMilliseconds = 0;
    decodingDone                         = false;
    finishedSent                         = false;
    decodedMilliseconds                  = 0;
    playedMilliseconds                   = 0;
    dspInitialBufferCount                = 0;

    // priority maps
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
                    setupDecoderPlugin(plugin);
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
void Track::setupDecoderPlugin(QObject *plugin)
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

    // add to decoders' list, this makes it easier to look for decoders
    decoders.append(persistentUniqueId);

    // initializations
    if (!plugin->metaObject()->invokeMethod(plugin, "setUrl", Qt::DirectConnection, Q_ARG(QUrl, trackInfo.url))) {
        emit error(trackInfo.url, true, "Failed to invoke method on plugin");
    }

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
    connect(plugin, SIGNAL(messageToDspPlugin(QUuid, QUuid, int, QVariant)), this,   SLOT(dspPreMessageToDspPlugin(QUuid, QUuid, int, QVariant)));
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
    connect(plugin, SIGNAL(fadeInComplete(QUuid)),                     this,   SLOT(outputFadeInComplete(QUuid)));
    connect(plugin, SIGNAL(fadeOutComplete(QUuid)),                    this,   SLOT(outputFadeOutComplete(QUuid)));
    connect(plugin, SIGNAL(error(QUuid, QString)),                     this,   SLOT(outputError(QUuid, QString)));
    connect(this,   SIGNAL(loadedConfiguration(QUuid, QJsonDocument)), plugin, SLOT(loadedConfiguration(QUuid, QJsonDocument)));
    connect(this,   SIGNAL(requestPluginUi(QUuid)),                    plugin, SLOT(getUiQml(QUuid)));
    connect(this,   SIGNAL(pluginUiResults(QUuid, QJsonDocument)),     plugin, SLOT(uiResults(QUuid, QJsonDocument)));
    connect(this,   SIGNAL(bufferAvailable(QUuid)),                    plugin, SLOT(bufferAvailable(QUuid)));
    connect(this,   SIGNAL(pause(QUuid)),                              plugin, SLOT(pause(QUuid)));
    connect(this,   SIGNAL(resume(QUuid)),                             plugin, SLOT(resume(QUuid)));
    connect(this,   SIGNAL(fadeIn(QUuid, int)),                        plugin, SLOT(fadeIn(QUuid, int)));
    connect(this,   SIGNAL(fadeOut(QUuid, int)),                       plugin, SLOT(fadeOut(QUuid, int)));
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.3")) {
        connect(plugin, SIGNAL(saveGlobalConfiguration(QUuid, QJsonDocument)),   this,   SLOT(saveGlobalConfiguration(QUuid, QJsonDocument)));
        connect(plugin, SIGNAL(loadGlobalConfiguration(QUuid)),                  this,   SLOT(loadGlobalConfiguration(QUuid)));
        connect(this,   SIGNAL(loadedGlobalConfiguration(QUuid, QJsonDocument)), plugin, SLOT(loadedGlobalConfiguration(QUuid, QJsonDocument)));
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
    connect(plugin, SIGNAL(addInfoHtml(QUuid, QString)),               this,   SLOT(infoAddInfoHtml(QUuid, QString)));
    connect(this,   SIGNAL(loadedConfiguration(QUuid, QJsonDocument)), plugin, SLOT(loadedConfiguration(QUuid, QJsonDocument)));
    connect(this,   SIGNAL(requestPluginUi(QUuid)),                    plugin, SLOT(getUiQml(QUuid)));
    connect(this,   SIGNAL(pluginUiResults(QUuid, QJsonDocument)),     plugin, SLOT(uiResults(QUuid, QJsonDocument)));
    if (PluginLibsLoader::isPluginCompatible(pluginData.waverVersionAPICompatibility, "0.0.3")) {
        connect(plugin, SIGNAL(saveGlobalConfiguration(QUuid, QJsonDocument)),   this,   SLOT(saveGlobalConfiguration(QUuid, QJsonDocument)));
        connect(plugin, SIGNAL(loadGlobalConfiguration(QUuid)),                  this,   SLOT(loadGlobalConfiguration(QUuid)));
        connect(this,   SIGNAL(loadedGlobalConfiguration(QUuid, QJsonDocument)), plugin, SLOT(loadedGlobalConfiguration(QUuid, QJsonDocument)));
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

        if (currentDecoderId.isNull() && (decoders.count() > 0)) {
            currentDecoderId    = decoders.at(0);
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

        if (currentDecoderId.isNull() && (decoders.count() > 0)) {
            currentDecoderId    = decoders.at(0);
            decodedMilliseconds = 0;
            emit startDecode(currentDecoderId);
        }

        foreach (QUuid dspPluginId, dspPlugins.keys()) {
            emit playBegin(dspPluginId);
        }

        if (trackInfo.cast || fadeInRequested) {
            foreach (QUuid outputPluginId, outputPlugins.keys()) {
                emit fadeIn(outputPluginId, (fadeInRequestedMilliseconds == 0 ? INTERRUPT_FADE_SECONDS : fadeInRequestedMilliseconds / 1000));
            }
        }

        currentStatus = Playing;

        sendLoadedPluginsWithUI();

        return;
    }

    if ((status == Playing) && (currentStatus == Decoding)) {
        dspThread.start();
        outputThread.start();

        foreach (QUuid dspPluginId, dspPlugins.keys()) {
            emit playBegin(dspPluginId);
        }

        if (trackInfo.cast || fadeInRequested) {
            foreach (QUuid outputPluginId, outputPlugins.keys()) {
                emit fadeIn(outputPluginId, (fadeInRequestedMilliseconds == 0 ? INTERRUPT_FADE_SECONDS : fadeInRequestedMilliseconds / 1000));
            }
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

        sendLoadedPluginsWithUI();

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
QUuid Track::getSourcePluginId()
{
    return sourcePliginId;
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
void Track::startWithFadeIn(qint64 lengthMilliseconds)
{
    // this has effect only when the track starts to play
    fadeInRequested = true;
    if (fadeInRequestedMilliseconds < lengthMilliseconds) {
        fadeInRequestedMilliseconds = lengthMilliseconds;
    }
}


// public method
void Track::startWithoutFadeIn()
{
    if (fadeInRequestedInternal) {
        fadeInRequestedMilliseconds = fadeInRequestedInternalMilliseconds;
        return;
    }

    fadeInRequested             = false;
    fadeInRequestedMilliseconds = 0;
}


// public method
bool Track::isDecodingDone()
{
    return decodingDone;
}


// public method
qint64 Track::getDecodedMilliseconds()
{
    return decodedMilliseconds;
}


// public method
qint64 Track::getPlayedMilliseconds()
{
    return playedMilliseconds;
}


// public method
void Track::interrupt()
{
    if ((currentStatus == Playing) && (!interruptInProgress)) {
        interruptInProgress = true;

        foreach (QUuid pluginId, outputPlugins.keys()) {
            emit fadeOut(pluginId, INTERRUPT_FADE_SECONDS);
        }

        return;
    }

    if (!interruptInProgress) {
        sendFinished();
    }
}


// private method
void Track::sendLoadedPluginsWithUI()
{
    PluginsWithUI pluginsWithUI;

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


// plugin signal handler
void Track::loadConfiguration(QUuid uniqueId)
{
    // re-emit for server
    emit loadPluginSettings(uniqueId);
}


// plugin signal handler
void Track::saveConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    // re-emit for server
    emit savePluginSettings(uniqueId, configuration);
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
void Track::decoderFinished(QUuid uniqueId)
{
    if (uniqueId != currentDecoderId) {
        return;
    }

    decodingDone = true;

    foreach (QUuid pluginId, dspPrePlugins.keys()) {
        emit decoderDone(pluginId);
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
        int nextDecoderIndex = decoders.indexOf(currentDecoderId) + 1;
        if (nextDecoderIndex < decoders.count()) {
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

            currentDecoderId    = decoders.at(nextDecoderIndex);
            decodedMilliseconds = 0;
            emit startDecode(uniqueId);

            return;
        }

        // there were no more decoders, so this is fatal at this point
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
        interruptAboutToFinishSendPosition = 0;
        emit aboutToFinish(trackInfo.url);
    }

    playedMilliseconds = posMilliseconds;

    emit playPosition(trackInfo.url, trackInfo.cast, decodingDone, decodedMilliseconds, posMilliseconds);
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
        sendFinished();
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

    // new request overwrites pervious one
    interruptPosition            = posMilliseconds;
    interruptPositionWithFadeOut = withFadeOut;
}


// pre-dsp signal handler
void Track::dspPreRequestAboutToFinishSend(QUuid uniqueId, qint64 posMilliseconds)
{
    Q_UNUSED(uniqueId);

    // new request overwrites pervious one
    interruptAboutToFinishSendPosition = posMilliseconds;
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

    emit trackInfoUpdated(trackInfo.url);
}


// info plugin signal handler
void Track::infoAddInfoHtml(QUuid uniqueId, QString info)
{

}
