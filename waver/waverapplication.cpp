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


#include "waverapplication.h"


// constructor
WaverApplication::WaverApplication(int &argc, char **argv) : QGuiApplication(argc, argv)
{
    // to make debugging easier
    tcpThread.setObjectName("application_tcp");

    // initializations
    uiMainWindow = NULL;

    // internal signal connections
    connect(&uiPicturesTimer, SIGNAL(timeout()), this, SLOT(updateUIPicture()));

    // so it can be used in inter-thread signals
    qRegisterMetaType<IpcMessageUtils::IpcMessages>("IpcMessageUtils::IpcMessages");

    // instantiate and set up interprocess communication handler

    tcpHandler = new ClientTcpHandler();
    tcpHandler->moveToThread(&tcpThread);

    connect(&tcpThread, SIGNAL(started()),  tcpHandler, SLOT(run()));
    connect(&tcpThread, SIGNAL(finished()), tcpHandler, SLOT(deleteLater()));

    connect(this,       SIGNAL(ipcConnect()),                                         tcpHandler, SLOT(open()));
    connect(this,       SIGNAL(ipcDisconnect()),                                      tcpHandler, SLOT(close()));
    connect(this,       SIGNAL(ipcSend(IpcMessageUtils::IpcMessages)),                tcpHandler, SLOT(send(IpcMessageUtils::IpcMessages)));
    connect(this,       SIGNAL(ipcSend(IpcMessageUtils::IpcMessages, QJsonDocument)), tcpHandler, SLOT(send(IpcMessageUtils::IpcMessages,  QJsonDocument)));
    connect(tcpHandler, SIGNAL(opened()),                                             this,       SLOT(ipcOpened()));
    connect(tcpHandler, SIGNAL(closed()),                                             this,       SLOT(ipcClosed()));
    connect(tcpHandler, SIGNAL(message(IpcMessageUtils::IpcMessages, QJsonDocument)), this,       SLOT(ipcMessage(IpcMessageUtils::IpcMessages, QJsonDocument)));
    connect(tcpHandler, SIGNAL(error(bool, QString)),                                 this,       SLOT(ipcError(bool, QString)));

    // application signal connections
    connect(this, SIGNAL(applicationStateChanged(Qt::ApplicationState)), this, SLOT(stateChanged(Qt::ApplicationState)));

    // applicationStateChanged isn't called on some platforms when the app first starts
    QTimer::singleShot(100, this, &WaverApplication::active);
}


// destructor
WaverApplication::~WaverApplication()
{
    tcpThread.quit();
    tcpThread.wait(THREAD_TIMEOUT);
}


// set the QML engine, this is intended to be called form the main program after the main window is loaded
void WaverApplication::setQmlApplicationEngine(QQmlApplicationEngine *qmlApplicationEngine)
{
    // just in case
    if (uiMainWindow != NULL) {
        disconnect(this,         SIGNAL(uiActivated()),                                                                 uiMainWindow, SLOT(activated()));
        disconnect(this,         SIGNAL(uiInactivated()),                                                               uiMainWindow, SLOT(inactivated()));
        disconnect(this,         SIGNAL(uiUserMessage(QVariant)),                                                       uiMainWindow, SLOT(displayUserMessage(QVariant)));
        disconnect(this,         SIGNAL(uiCollections(QVariant, QVariant)),                                             uiMainWindow, SLOT(fillCollectionsList(QVariant, QVariant)));
        disconnect(this,         SIGNAL(uiTrackInfo(QVariant, QVariant, QVariant, QVariant, QVariant)),                 uiMainWindow, SLOT(updateTrackInfo(QVariant, QVariant, QVariant, QVariant, QVariant)));
        disconnect(this,         SIGNAL(uiPicture(QVariant)),                                                           uiMainWindow, SLOT(updateArt(QVariant)));
        disconnect(this,         SIGNAL(uiPosition(QVariant, QVariant)),                                                uiMainWindow, SLOT(updatePosition(QVariant, QVariant)));
        disconnect(this,         SIGNAL(uiActions(QVariant)),                                                           uiMainWindow, SLOT(updateTrackActions(QVariant)));
        disconnect(this,         SIGNAL(uiClearPlaylist()),                                                             uiMainWindow, SLOT(clearPlaylist()));
        disconnect(this,         SIGNAL(uiAddToPlaylist(QVariant, QVariant, QVariant, QVariant, QVariant)),             uiMainWindow, SLOT(addToPlaylist(QVariant, QVariant, QVariant, QVariant, QVariant)));
        disconnect(this,         SIGNAL(uiSetPlaylistIndex(QVariant)),                                                  uiMainWindow, SLOT(setPlaylistIndex(QVariant)));
        disconnect(this,         SIGNAL(uiPaused()),                                                                    uiMainWindow, SLOT(paused()));
        disconnect(this,         SIGNAL(uiResumed()),                                                                   uiMainWindow, SLOT(resumed()));
        disconnect(this,         SIGNAL(uiClearDiagnosticsSelectorListList()),                                          uiMainWindow, SLOT(clearDiagnosticsSelectorListList()));
        disconnect(this,         SIGNAL(uiAddToDiagnosticsSelectorList(QVariant, QVariant)),                            uiMainWindow, SLOT(addToDiagnosticsSelectorList(QVariant, QVariant)));
        disconnect(this,         SIGNAL(uiClearPluginsList()),                                                          uiMainWindow, SLOT(clearPluginsList()));
        disconnect(this,         SIGNAL(uiAddToPluginsWithUIList(QVariant, QVariant)),                                  uiMainWindow, SLOT(addToPluginsWithUIList(QVariant, QVariant)));
        disconnect(this,         SIGNAL(uiDisplayPluginUI(QVariant, QVariant, QVariant)),                               uiMainWindow, SLOT(displayPluginUI(QVariant, QVariant, QVariant)));
        disconnect(this,         SIGNAL(uiAddToOpenTracksList(QVariant, QVariant, QVariant, QVariant, QVariant)),       uiMainWindow, SLOT(addToOpenTracksList(QVariant, QVariant, QVariant, QVariant, QVariant)));
        disconnect(this,         SIGNAL(uiAddToSearchList(QVariant, QVariant, QVariant)),                               uiMainWindow, SLOT(addToSearchList(QVariant, QVariant, QVariant)));
        disconnect(this,         SIGNAL(uiAbout(QVariant)),                                                             uiMainWindow, SLOT(aboutDialog(QVariant)));
        disconnect(this,         SIGNAL(uiDisplayDiagnosticsMessage(QVariant, QVariant)),                               uiMainWindow, SLOT(displayDiagnosticsMessage(QVariant, QVariant)));
        disconnect(this,         SIGNAL(uiClearSourcePrioritiesList()),                                                 uiMainWindow, SLOT(clearSourcePrioritiesList()));
        disconnect(this,         SIGNAL(uiAddToSourcePrioritiesList(QVariant, QVariant, QVariant, QVariant, QVariant)), uiMainWindow, SLOT(addToSourcePrioritiesList(QVariant, QVariant, QVariant, QVariant, QVariant)));
        disconnect(this,         SIGNAL(uiOptions(QVariant, QVariant, QVariant, QVariant)),                             uiMainWindow, SLOT(optionsData(QVariant, QVariant, QVariant, QVariant)));
        disconnect(uiMainWindow, SIGNAL(menuPause()),                                                                   this,         SLOT(menuPause()));
        disconnect(uiMainWindow, SIGNAL(menuResume()),                                                                  this,         SLOT(menuResume()));
        disconnect(uiMainWindow, SIGNAL(menuNext()),                                                                    this,         SLOT(menuNext()));
        disconnect(uiMainWindow, SIGNAL(menuCollection(QVariant)),                                                      this,         SLOT(menuCollection(QVariant)));
        disconnect(uiMainWindow, SIGNAL(menuAbout()),                                                                   this,         SLOT(menuAbout()));
        disconnect(uiMainWindow, SIGNAL(menuQuit()),                                                                    this,         SLOT(menuQuit()));
        disconnect(uiMainWindow, SIGNAL(menuSourcePriorities()),                                                        this,         SLOT(menuSourcePriorities()));
        disconnect(uiMainWindow, SIGNAL(menuOptions()),                                                                 this,         SLOT(menuOptions()));
        disconnect(uiMainWindow, SIGNAL(collectionsDialogResults(QVariant)),                                            this,         SLOT(collectionsDialogResults(QVariant)));
        disconnect(uiMainWindow, SIGNAL(pluginUIResults(QVariant, QVariant)),                                           this,         SLOT(pluginUIResults(QVariant, QVariant)));
        disconnect(uiMainWindow, SIGNAL(getOpenTracks(QVariant, QVariant)),                                             this,         SLOT(getOpenTracks(QVariant, QVariant)));
        disconnect(uiMainWindow, SIGNAL(resolveOpenTracks(QVariant)),                                                   this,         SLOT(resolveOpenTracks(QVariant)));
        disconnect(uiMainWindow, SIGNAL(trackAction(QVariant, QVariant)),                                               this,         SLOT(trackAction(QVariant, QVariant)));
        disconnect(uiMainWindow, SIGNAL(getDiagnostics(QVariant)),                                                      this,         SLOT(getDiagnostics(QVariant)));
        disconnect(uiMainWindow, SIGNAL(doneDiagnostics()),                                                             this,         SLOT(doneDiagnostics()));
        disconnect(uiMainWindow, SIGNAL(sourcePrioritiesDialogResults(QVariant)),                                       this,         SLOT(sourcePrioritiesDialogResults(QVariant)));
        disconnect(uiMainWindow, SIGNAL(optionsDialogResults(QVariant)),                                                this,         SLOT(optionsDialogResults(QVariant)));
    }

    // what we're really interested in is the main application window
    uiMainWindow = qobject_cast<QQuickWindow *>(qmlApplicationEngine->rootObjects().first());

    // signal connections
    connect(this,         SIGNAL(uiActivated()),                                                                 uiMainWindow, SLOT(activated()));
    connect(this,         SIGNAL(uiInactivated()),                                                               uiMainWindow, SLOT(inactivated()));
    connect(this,         SIGNAL(uiUserMessage(QVariant, QVariant)),                                             uiMainWindow, SLOT(displayUserMessage(QVariant, QVariant)));
    connect(this,         SIGNAL(uiCollections(QVariant, QVariant)),                                             uiMainWindow, SLOT(fillCollectionsList(QVariant, QVariant)));
    connect(this,         SIGNAL(uiTrackInfo(QVariant, QVariant, QVariant, QVariant, QVariant, QVariant)),       uiMainWindow, SLOT(updateTrackInfo(QVariant, QVariant, QVariant, QVariant, QVariant, QVariant)));
    connect(this,         SIGNAL(uiPicture(QVariant)),                                                           uiMainWindow, SLOT(updateArt(QVariant)));
    connect(this,         SIGNAL(uiPosition(QVariant, QVariant)),                                                uiMainWindow, SLOT(updatePosition(QVariant, QVariant)));
    connect(this,         SIGNAL(uiActions(QVariant)),                                                           uiMainWindow, SLOT(updateTrackActions(QVariant)));
    connect(this,         SIGNAL(uiClearPlaylist()),                                                             uiMainWindow, SLOT(clearPlaylist()));
    connect(this,         SIGNAL(uiAddToPlaylist(QVariant, QVariant, QVariant, QVariant, QVariant, QVariant)),   uiMainWindow, SLOT(addToPlaylist(QVariant, QVariant, QVariant, QVariant, QVariant, QVariant)));
    connect(this,         SIGNAL(uiSetPlaylistIndex(QVariant)),                                                  uiMainWindow, SLOT(setPlaylistIndex(QVariant)));
    connect(this,         SIGNAL(uiPaused()),                                                                    uiMainWindow, SLOT(paused()));
    connect(this,         SIGNAL(uiResumed()),                                                                   uiMainWindow, SLOT(resumed()));
    connect(this,         SIGNAL(uiClearDiagnosticsSelectorList()),                                              uiMainWindow, SLOT(clearDiagnosticsSelectorList()));
    connect(this,         SIGNAL(uiAddToDiagnosticsSelectorList(QVariant, QVariant)),                            uiMainWindow, SLOT(addToDiagnosticsSelectorList(QVariant, QVariant)));
    connect(this,         SIGNAL(uiClearPluginsList()),                                                          uiMainWindow, SLOT(clearPluginsList()));
    connect(this,         SIGNAL(uiAddToPluginsWithUIList(QVariant, QVariant)),                                  uiMainWindow, SLOT(addToPluginsWithUIList(QVariant, QVariant)));
    connect(this,         SIGNAL(uiDisplayPluginUI(QVariant, QVariant, QVariant)),                               uiMainWindow, SLOT(displayPluginUI(QVariant, QVariant, QVariant)));
    connect(this,         SIGNAL(uiAddToOpenTracksList(QVariant, QVariant, QVariant, QVariant, QVariant)),       uiMainWindow, SLOT(addToOpenTracksList(QVariant, QVariant, QVariant, QVariant, QVariant)));
    connect(this,         SIGNAL(uiAddToSearchList(QVariant, QVariant, QVariant)),                               uiMainWindow, SLOT(addToSearchList(QVariant, QVariant, QVariant)));
    connect(this,         SIGNAL(uiAbout(QVariant)),                                                             uiMainWindow, SLOT(aboutDialog(QVariant)));
    connect(this,         SIGNAL(uiDisplayDiagnosticsMessage(QVariant, QVariant)),                               uiMainWindow, SLOT(displayDiagnosticsMessage(QVariant, QVariant)));
    connect(this,         SIGNAL(uiClearSourcePrioritiesList()),                                                 uiMainWindow, SLOT(clearSourcePrioritiesList()));
    connect(this,         SIGNAL(uiAddToSourcePrioritiesList(QVariant, QVariant, QVariant, QVariant, QVariant)), uiMainWindow, SLOT(addToSourcePrioritiesList(QVariant, QVariant, QVariant, QVariant, QVariant)));
    connect(this,         SIGNAL(uiOptions(QVariant, QVariant, QVariant, QVariant)),                             uiMainWindow, SLOT(optionsData(QVariant, QVariant, QVariant, QVariant)));
    connect(uiMainWindow, SIGNAL(menuPause()),                                                                   this,         SLOT(menuPause()));
    connect(uiMainWindow, SIGNAL(menuResume()),                                                                  this,         SLOT(menuResume()));
    connect(uiMainWindow, SIGNAL(menuNext()),                                                                    this,         SLOT(menuNext()));
    connect(uiMainWindow, SIGNAL(menuCollection(QVariant)),                                                      this,         SLOT(menuCollection(QVariant)));
    connect(uiMainWindow, SIGNAL(menuPlugin(QVariant)),                                                          this,         SLOT(menuPlugin(QVariant)));
    connect(uiMainWindow, SIGNAL(menuAbout()),                                                                   this,         SLOT(menuAbout()));
    connect(uiMainWindow, SIGNAL(menuQuit()),                                                                    this,         SLOT(menuQuit()));
    connect(uiMainWindow, SIGNAL(menuSourcePriorities()),                                                        this,         SLOT(menuSourcePriorities()));
    connect(uiMainWindow, SIGNAL(menuOptions()),                                                                 this,         SLOT(menuOptions()));
    connect(uiMainWindow, SIGNAL(collectionsDialogResults(QVariant)),                                            this,         SLOT(collectionsDialogResults(QVariant)));
    connect(uiMainWindow, SIGNAL(pluginUIResults(QVariant, QVariant)),                                           this,         SLOT(pluginUIResults(QVariant, QVariant)));
    connect(uiMainWindow, SIGNAL(getOpenTracks(QVariant, QVariant)),                                             this,         SLOT(getOpenTracks(QVariant, QVariant)));
    connect(uiMainWindow, SIGNAL(startSearch(QVariant)),                                                         this,         SLOT(startSearch(QVariant)));
    connect(uiMainWindow, SIGNAL(resolveOpenTracks(QVariant)),                                                   this,         SLOT(resolveOpenTracks(QVariant)));
    connect(uiMainWindow, SIGNAL(trackAction(QVariant, QVariant)),                                               this,         SLOT(trackAction(QVariant, QVariant)));
    connect(uiMainWindow, SIGNAL(getDiagnostics(QVariant)),                                                      this,         SLOT(getDiagnostics(QVariant)));
    connect(uiMainWindow, SIGNAL(doneDiagnostics()),                                                             this,         SLOT(doneDiagnostics()));
    connect(uiMainWindow, SIGNAL(sourcePrioritiesDialogResults(QVariant)),                                       this,         SLOT(sourcePrioritiesDialogResults(QVariant)));
    connect(uiMainWindow, SIGNAL(optionsDialogResults(QVariant)),                                                this,         SLOT(optionsDialogResults(QVariant)));
}


// application signal handler
void WaverApplication::stateChanged(Qt::ApplicationState state)
{
    // call the appropriate method
    switch (state) {
        case Qt::ApplicationActive:
            active();
            break;

        case Qt::ApplicationInactive:
            inactive();
            break;

        case Qt::ApplicationSuspended:
            suspended();
            break;

        default:
            break;
    }
}


// menu signal handler
void WaverApplication::menuPause()
{
    emit ipcSend(IpcMessageUtils::Pause);
}


// menu signal handler
void WaverApplication::menuResume()
{
    emit ipcSend(IpcMessageUtils::Resume);
}


// menu signal handler
void WaverApplication::menuNext()
{
    emit ipcSend(IpcMessageUtils::Next);
}


// menu signal handler
void WaverApplication::menuCollection(QVariant collection)
{
    emit ipcSend(IpcMessageUtils::CollectionMenuChange, QJsonDocument(QJsonObject::fromVariantHash(QVariantHash({
        {
            "collection", collection.toString()
        }
    }))));
}


// menu signal handler
void WaverApplication::menuPlugin(QVariant id)
{
    emit ipcSend(IpcMessageUtils::PluginUI, QJsonDocument(QJsonObject::fromVariantHash(QVariantHash({{"plugin_id", id.toString()}}))));
}


// menu signal handler
void WaverApplication::menuAbout()
{
    QVariantHash data;
    data.insert("name", Globals::appName());
    data.insert("version", Globals::appVersion());
    data.insert("description", Globals::appDesc());
    data.insert("email", Globals::email(true));
    data.insert("website", Globals::website(true));
    data.insert("copyright", Globals::copyright());
    data.insert("license", Globals::license());
    data.insert("credits", Globals::credits());
    data.insert("privacy", Globals::privacy());

    emit uiAbout(QString(QJsonDocument(QJsonObject::fromVariantHash(data)).toJson()));
}


// menu signal handler
void WaverApplication::menuQuit()
{
    emit ipcSend(IpcMessageUtils::Quit);

    #ifdef Q_OS_ANDROID
    // Android tries to keep the service alive unless it's explicitly stopped
    QAndroidJniObject activity = QtAndroid::androidActivity();
    activity.callMethod<void>("stopService");
    #endif

    //quit();
}


// UI signal handler
void WaverApplication::collectionsDialogResults(QVariant collectionsArray)
{
    emit ipcSend(IpcMessageUtils::CollectionsDialogResults, QJsonDocument(QJsonArray::fromStringList(collectionsArray.toStringList())));
}


// UI signal handler
void WaverApplication::pluginUIResults(QVariant id, QVariant results)
{
    QJsonDocument resultsDocument = QJsonDocument::fromJson(results.toString().toUtf8());
    if (resultsDocument.isNull() || resultsDocument.isEmpty()) {
        return;
    }

    emit ipcSend(IpcMessageUtils::PluginUIResults, QJsonDocument(QJsonObject::fromVariantHash(QVariantHash({
        {
            "plugin_id", id.toString()
        },
        {
            "ui_results", (resultsDocument.isArray() ? QVariant(resultsDocument.array()) : QVariant(resultsDocument.object()))
        }
    }))));
}


// UI signal handler
void WaverApplication::getOpenTracks(QVariant pluginId, QVariant parentId)
{
    emit ipcSend(IpcMessageUtils::OpenTracks, QJsonDocument(QJsonObject::fromVariantHash(QVariantHash({
        {
            "plugin_id", pluginId.toString()
        },
        {
            "parent_id", parentId.toString()
        },
    }))));
}


// UI signal handler
void WaverApplication::startSearch(QVariant criteria)
{
    emit ipcSend(IpcMessageUtils::Search, QJsonDocument(QJsonObject::fromVariantHash(QVariantHash({
        {
            "criteria", criteria.toString()
        },
    }))));
}


// UI signal handler
void WaverApplication::resolveOpenTracks(QVariant results)
{
    emit ipcSend(IpcMessageUtils::OpenTracksSelected, QJsonDocument::fromJson(results.toString().toUtf8()));
}


// UI signal handler
void WaverApplication::trackAction(QVariant index, QVariant action)
{
    emit ipcSend(IpcMessageUtils::TrackAction, QJsonDocument(QJsonObject::fromVariantHash(QVariantHash({
        {
            "index", index.toString()
        },
        {
            "action", action.toString()
        }
    }))));
}


// UI signal handler
void WaverApplication::getDiagnostics(QVariant id)
{
    emit ipcSend(IpcMessageUtils::Diagnostics, QJsonDocument(QJsonObject::fromVariantHash(QVariantHash({
        {
            "mode", "get"
        },
        {
            "id", id
        }
    }))));
}


// UI signal handler
void WaverApplication::doneDiagnostics()
{
    emit ipcSend(IpcMessageUtils::Diagnostics, QJsonDocument(QJsonObject::fromVariantHash(QVariantHash({
        {
            "mode", "done"
        }
    }))));
}


// UI signal handler
void WaverApplication::menuSourcePriorities()
{
    emit ipcSend(IpcMessageUtils::SourcePriorities, QJsonDocument());
}


// UI signal handler
void WaverApplication::menuOptions()
{
    emit ipcSend(IpcMessageUtils::Options, QJsonDocument());
}


// UI signal handler
void WaverApplication::sourcePrioritiesDialogResults(QVariant results)
{
    emit ipcSend(IpcMessageUtils::SourcePriorityResults, QJsonDocument::fromJson(results.toString().toUtf8()));
}


// UI signal handler
void WaverApplication::optionsDialogResults(QVariant results)
{
    emit ipcSend(IpcMessageUtils::OptionsResults, QJsonDocument::fromJson(results.toString().toUtf8()));
}


// application activated
void WaverApplication::active()
{
    // start interprocess communication handler if not yet running
    if (!tcpThread.isRunning()) {
        tcpThread.start();
    }

    // attempt to connect to the server if not yet connected
    emit ipcConnect();

    // ui stuff
    emit uiActivated();
}


// application inactivated
void WaverApplication::inactive()
{
    emit uiInactivated();
}


// application suspended
void WaverApplication::suspended()
{
    inactive();

    emit ipcDisconnect();
}


// display a notification and quit
void WaverApplication::quitWithMessage(QString messageText)
{
    // quit right away if main window is not known
    if (uiMainWindow == NULL) {
        quit();
        return;
    }

    // send message to main window
    emit uiUserMessage(messageText, "error");

    // wait a wile for user to have a chance to read the message, then quit
    QTimer::singleShot(uiMainWindow->property("duration_visible_before_fadeout").toInt() + uiMainWindow->property("duration_fadeout").toInt(), this, SLOT(quit()));
}


// collections
void WaverApplication::updateUICollections(QJsonDocument jsonDocument)
{
    QVariantHash data         = jsonDocument.object().toVariantHash();
    QStringList  collections  = data.value("collections").toStringList();
    int          currentIndex = collections.indexOf(data.value("current_collection").toString());

    emit uiCollections(collections, currentIndex);
}


// track info
void WaverApplication::updateUITrackInfo(QJsonDocument jsonDocument)
{
    uiPicturesTimer.stop();
    uiPictures.clear();

    IpcMessageUtils ipcMessageUtils;
    TrackInfo trackInfo         = ipcMessageUtils.jsonDocumentToTrackInfo(jsonDocument);
    QVariantHash additionalInfo = ipcMessageUtils.jsonDocumentToAdditionalInfo(jsonDocument);

    int loved = PLAYLIST_MODE_NORMAL;
    if (additionalInfo.contains("loved")) {
        loved = additionalInfo.value("loved").toInt();
    }

    emit uiTrackInfo(trackInfo.title, trackInfo.performer, trackInfo.album, trackInfo.year, trackInfo.track, loved);

    if (trackInfo.pictures.count() < 1) {
        emit uiPicture("/images/waver.png");
    }
    else {
        emit uiPicture(trackInfo.pictures.at(0).toString());

        if (trackInfo.pictures.count() > 1) {
            foreach (QUrl url, trackInfo.pictures) {
                uiPictures.append(url);
            }
            uiPictureIndex = 0;
            uiPicturesTimer.start(2500);
        }
    }

    QStringList actions;
    if (trackInfo.cast) {
        actions.append("<a href=\"s~play_more\">More</a>");
        actions.append("<a href=\"s~play_forever\">Infinite</a>");
    }
    qSort(trackInfo.actions.begin(), trackInfo.actions.end(), [](TrackAction a, TrackAction b) {
        return ((a.pluginId < b.pluginId) || (a.id < b.id));
    });
    foreach (TrackAction trackAction, trackInfo.actions) {
        actions.append(QString("<a href=\"%1~%2\">%3</a>").arg(trackAction.pluginId.toString()).arg(trackAction.id).arg(trackAction.label));
    }
    emit uiActions(actions.join(" "));
}


// timer signal handler
void WaverApplication::updateUIPicture()
{
    uiPictureIndex++;
    if (uiPictureIndex >= uiPictures.count()) {
        uiPictureIndex = 0;
        if (uiPicturesTimer.interval() < 20000) {
            uiPicturesTimer.setInterval(uiPicturesTimer.interval() * 2);
        }
    }

    emit uiPicture(uiPictures.at(uiPictureIndex).toString());
}


// playing position
void WaverApplication::updateUIPosition(QJsonDocument jsonDocument)
{
    QVariantHash position = jsonDocument.object().toVariantHash();

    emit uiPosition(position.value("elapsed", "00:00"), position.value("remaining", "00:00"));
}


// playlist
void WaverApplication::updateUIPlaylist(QJsonDocument jsonDocument)
{
    emit uiClearPlaylist();

    IpcMessageUtils ipcMessageUtils;

    QVariantHash data                  = jsonDocument.object().toVariantHash();
    int          contextShowTrackIndex = data.value("contextShowTrackIndex").toInt();
    QJsonArray   playlist              = QJsonArray::fromVariantList(data.value("playlist").toList());

    for (int i = 0; i < playlist.count(); i++) {
        TrackInfo trackInfo         = ipcMessageUtils.jsonDocumentToTrackInfo(QJsonDocument(playlist.at(i).toObject()));
        QVariantHash additionalInfo = ipcMessageUtils.jsonDocumentToAdditionalInfo(QJsonDocument(playlist.at(i).toObject()));

        QStringList actions;

        if (i > 0) {
            actions.append("<a href=\"s~up\">Up</a>");
        }
        if (i < (playlist.count() - 1)) {
            actions.append("<a href=\"s~down\">Down</a>");
        }
        actions.append("<a href=\"s~remove\">Remove</a>");

        qSort(trackInfo.actions.begin(), trackInfo.actions.end(), [](TrackAction a, TrackAction b) {
            return ((a.pluginId < b.pluginId) || (a.id < b.id));
        });
        foreach (TrackAction trackAction, trackInfo.actions) {
            actions.append(QString("<a href=\"%1~%2\">%3</a>").arg(trackAction.pluginId.toString()).arg(trackAction.id).arg(trackAction.label));
        }

        int loved = PLAYLIST_MODE_NORMAL;
        if (additionalInfo.contains("loved")) {
            loved = additionalInfo.value("loved").toInt();
        }

        emit uiAddToPlaylist((trackInfo.pictures.count() > 0 ? trackInfo.pictures.at(0).toString() : "/images/waver.png"), trackInfo.title.simplified(), trackInfo.performer.simplified(), actions.join(" "), (i == contextShowTrackIndex), loved);
    }

    if (contextShowTrackIndex >= 0) {
        emit uiSetPlaylistIndex(contextShowTrackIndex);
    }
}


// diagnostics selector list
void WaverApplication::updateUIDiagnosticsSelector(QJsonDocument jsonDocument)
{
    emit uiClearDiagnosticsSelectorList();

    // sort them by plugin name
    QVariantHash plugins = jsonDocument.object().toVariantHash();
    QVariantList values  = plugins.values();
    qSort(values);

    // send to UI
    foreach (QVariant value, values) {
        foreach (QString id, plugins.keys(value)) {
            emit uiAddToDiagnosticsSelectorList(id, plugins.value(id));
        }
    }
}


// plugins list
void WaverApplication::updateUIPluginsWithUIList(QJsonDocument jsonDocument)
{
    emit uiClearPluginsList();

    // sort them by plugin name
    QVariantHash plugins = jsonDocument.object().toVariantHash();
    QVariantList values  = plugins.values();
    qSort(values);

    // send to UI
    foreach (QVariant value, values) {
        foreach (QString id, plugins.keys(value)) {
            emit uiAddToPluginsWithUIList(id, plugins.value(id));
        }
    }
}


// plugin UI
void WaverApplication::showPluginUI(QJsonDocument jsonDocument)
{
    QVariantHash data     = jsonDocument.object().toVariantHash();
    QString      pluginId = data.value("plugin_id").toString();
    QString      uiQml    = data.value("ui_qml").toString();
    QString      header   = data.value("header").toString();

    emit uiDisplayPluginUI(pluginId, uiQml, header);
}


// open url in browser
void WaverApplication::openUrl(QJsonDocument jsonDocument)
{
    QVariantHash data = jsonDocument.object().toVariantHash();
    QString      url  = data.value("url").toString();

    QDesktopServices::openUrl(QUrl(url));
}


// playlist add
void WaverApplication::updateUIOpenTracksList(QJsonDocument jsonDocument)
{
    QVariantHash data       = jsonDocument.object().toVariantHash();
    QString      pluginId   = data.value("plugin_id").toString();
    QJsonArray   tracksJson = QJsonArray::fromVariantList(data.value("tracks").toList());

    foreach (QVariant track, tracksJson) {
        QVariantHash trackHash = track.toHash();
        emit uiAddToOpenTracksList(pluginId, trackHash.value("hasChildren"), trackHash.value("selectable"), trackHash.value("label"),
            trackHash.value("id"));
    }
}


// playlist add
void WaverApplication::updateUISearchList(QJsonDocument jsonDocument)
{
    QVariantHash data       = jsonDocument.object().toVariantHash();
    QString      pluginId   = data.value("plugin_id").toString();
    QJsonArray   tracksJson = QJsonArray::fromVariantList(data.value("tracks").toList());

    foreach (QVariant track, tracksJson) {
        QVariantHash trackHash = track.toHash();
        emit uiAddToSearchList(pluginId, trackHash.value("label"), trackHash.value("id"));
    }
}


// diagnostic message
void WaverApplication::updateUIDiagnosticsMessage(QJsonDocument jsonDocument)
{
    QVariantHash data      = jsonDocument.object().toVariantHash();
    QString      id        = data.value("id").toString();

    QStringList messages;

    // error log is special case
    if (id.compare("error_log") == 0) {
        // get the error log items
        QJsonArray dataItems = QJsonArray::fromVariantList(data.value("data").toList());

        foreach (QVariant dataItem, dataItems) {
            // get the error log item
            QVariantHash dataItemHash = dataItem.toHash();
            bool         fatal        = dataItemHash.value("fatal").toBool();

            // format it
            QString message = QString("<pre>%1 <b>%2</b><br>  %3%4%5</pre>")
                .arg(dataItemHash.value("timestamp").toString(),
                    dataItemHash.value("title").toString(),
                    fatal ? "<font color=\"#880000\">" : "",
                    dataItemHash.value("message").toString(),
                    fatal ? "</font>" : "");

            // descending order
            messages.prepend(message);
        }

        // send to UI
        emit uiDisplayDiagnosticsMessage(id, QString(messages.join("<br>")));

        return;
    }

    // diagnostics are groupped by track title (for some reason Qt turns QVariantHash to QVariantMap but not on the top level which is crazy)
    QVariantMap dataTitles = data.value("data").toMap();
    foreach (QString title, dataTitles.keys()) {
        // some doesn't have a title (source plugins) indicated by tidle
        bool hasTitle = !(title.compare("~") == 0);

        if (hasTitle) {
            messages.append(QString("<pre><b>%1</b></pre>").arg(title));
        }

        // get items for this title
        QVariantList dataItems = dataTitles.value(title).toList();

        // find longest label
        int labelLength = 0;
        foreach (QVariant dataItem, dataItems) {
            int length = dataItem.toMap().value("label").toString().length();
            if (length > labelLength) {
                labelLength = length;
            }
        }

        // format data
        foreach (QVariant dataItem, dataItems) {
            QVariantMap dataItemHash = dataItem.toMap();

            QString message = QString("<pre>%1%2: %3</pre>")
                .arg(hasTitle ? "  " : "")
                .arg(dataItemHash.value("label").toString(), labelLength * -1)
                .arg(dataItemHash.value("message").toString());

            messages.append(message);
        }

        if (hasTitle) {
            messages.append("<br>");
        }
    }

    // send to UI
    emit uiDisplayDiagnosticsMessage(id, QString(messages.join("")));
}


// soure plugin priorities
void WaverApplication::updateUISourcePriorities(QJsonDocument jsonDocument)
{
    emit uiClearSourcePrioritiesList();

    QVariantList data = jsonDocument.array().toVariantList();
    foreach (QVariant dataItem, data) {
        QVariantMap sourcePriority = dataItem.toMap();
        emit uiAddToSourcePrioritiesList(sourcePriority.value("id"), sourcePriority.value("name"), sourcePriority.value("priority"), sourcePriority.value("recurring"), sourcePriority.value("lovedMode"));
    }
}


// options
void WaverApplication::updateUIOptions(QJsonDocument jsonDocument)
{
    QVariantHash data = jsonDocument.object().toVariantHash();

    int     streamPlayTime      = data.value("streamPlayTime").toInt();
    int     lovedStreamPlayTime = data.value("lovedStreamPlayTime").toInt();
    int     playlistAddMode     = data.value("playlistAddMode").toInt();
    QString startupCollection   = data.value("startupCollection").toString();

    emit uiOptions(streamPlayTime, lovedStreamPlayTime, playlistAddMode, startupCollection);
}


// interprocess communication signal handler
void WaverApplication::ipcOpened()
{
    emit ipcSend(IpcMessageUtils::CollectionList);
    emit ipcSend(IpcMessageUtils::TrackInfos);
    emit ipcSend(IpcMessageUtils::PlayPauseState);
    emit ipcSend(IpcMessageUtils::Playlist);
    emit ipcSend(IpcMessageUtils::Plugins);
    emit ipcSend(IpcMessageUtils::PluginsWithUI);
}


// interprocess communication signal handler
void WaverApplication::ipcClosed()
{
    // TODO quit if quitting isn't in process
}


// interprocess communication signal handler
void WaverApplication::ipcMessage(IpcMessageUtils::IpcMessages message, QJsonDocument jsonDocument)
{
    switch (message) {

        case IpcMessageUtils::CollectionList:
            updateUICollections(jsonDocument);
            break;

        case IpcMessageUtils::Diagnostics:
            updateUIDiagnosticsMessage(jsonDocument);
            break;

        case IpcMessageUtils::InfoMessage:
            emit uiUserMessage(jsonDocument.object().toVariantHash().value("message"), jsonDocument.object().toVariantHash().value("type"));
            break;

        case IpcMessageUtils::OpenTracks:
            updateUIOpenTracksList(jsonDocument);
            break;

        case IpcMessageUtils::OpenUrl:
            openUrl(jsonDocument);
            break;

        case IpcMessageUtils::Options:
            updateUIOptions(jsonDocument);
            break;

        case IpcMessageUtils::Pause:
            emit uiPaused();
            break;

        case IpcMessageUtils::Playlist:
            updateUIPlaylist(jsonDocument);
            break;

        case IpcMessageUtils::Plugins:
            updateUIDiagnosticsSelector(jsonDocument);
            break;

        case IpcMessageUtils::PluginsWithUI:
            updateUIPluginsWithUIList(jsonDocument);
            break;

        case IpcMessageUtils::PluginUI:
            showPluginUI(jsonDocument);
            break;

        case IpcMessageUtils::Position:
            updateUIPosition(jsonDocument);
            break;

        case IpcMessageUtils::QuitClients:
            quit();
            break;

        case IpcMessageUtils::Resume:
            emit uiResumed();
            break;

        case IpcMessageUtils::Search:
            updateUISearchList(jsonDocument);
            break;

        case IpcMessageUtils::SourcePriorities:
            updateUISourcePriorities(jsonDocument);
            break;

        case IpcMessageUtils::TrackInfos:
            updateUITrackInfo(jsonDocument);
            break;

        default:
            break;
    }
}


// interprocess communication signal handler
void WaverApplication::ipcError(bool fatal, QString error)
{
    // TODO better error handling
    emit uiUserMessage(error, "error");
}
