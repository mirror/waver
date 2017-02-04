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

    connect(this, SIGNAL(ipcConnect()),                                        tcpHandler, SLOT(open()));
    connect(this, SIGNAL(ipcDisconnect()),                                     tcpHandler, SLOT(close()));
    connect(this, SIGNAL(ipcSend(IpcMessageUtils::IpcMessages)),               tcpHandler, SLOT(send(IpcMessageUtils::IpcMessages)));
    connect(this, SIGNAL(ipcSend(IpcMessageUtils::IpcMessages,QJsonDocument)), tcpHandler, SLOT(send(IpcMessageUtils::IpcMessages,QJsonDocument)));

    connect(tcpHandler, SIGNAL(opened()),                                             this, SLOT(ipcOpened()));
    connect(tcpHandler, SIGNAL(closed()),                                             this, SLOT(ipcClosed()));
    connect(tcpHandler, SIGNAL(message(IpcMessageUtils::IpcMessages, QJsonDocument)), this, SLOT(ipcMessage(IpcMessageUtils::IpcMessages, QJsonDocument)));
    connect(tcpHandler, SIGNAL(error(bool,QString)),                                  this, SLOT(ipcError(bool,QString)));

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
        disconnect(this,         SIGNAL(uiUserMessage(QVariant)),                                             uiMainWindow, SLOT(displayUserMessage(QVariant)));
        disconnect(this,         SIGNAL(uiCollections(QVariant,QVariant)),                                    uiMainWindow, SLOT(fillCollectionsList(QVariant,QVariant)));
        disconnect(this,         SIGNAL(uiTrackInfo(QVariant,QVariant,QVariant,QVariant,QVariant)),           uiMainWindow, SLOT(updateTrackInfo(QVariant,QVariant,QVariant,QVariant,QVariant)));
        disconnect(this,         SIGNAL(uiPicture(QVariant)),                                                 uiMainWindow, SLOT(updateArt(QVariant)));
        disconnect(this,         SIGNAL(uiPosition(QVariant,QVariant)),                                       uiMainWindow, SLOT(updatePosition(QVariant, QVariant)));
        disconnect(this,         SIGNAL(uiAddToPlaylist(QVariant,QVariant,QVariant)),                         uiMainWindow, SLOT(addToPlaylist(QVariant,QVariant,QVariant)));
        disconnect(this,         SIGNAL(uiPaused()),                                                          uiMainWindow, SLOT(paused()));
        disconnect(this,         SIGNAL(uiResumed()),                                                         uiMainWindow, SLOT(resumed()));
        disconnect(this,         SIGNAL(uiClearPluginsList()),                                                uiMainWindow, SLOT(clearPluginsList()));
        disconnect(this,         SIGNAL(uiAddToPluginsList(QVariant,QVariant)),                               uiMainWindow, SLOT(addToPluginsList(QVariant,QVariant)));
        disconnect(this,         SIGNAL(uiDisplayPluginUI(QVariant,QVariant)),                                uiMainWindow, SLOT(displayPluginUI(QVariant id, QVariant qml)));
        disconnect(this,         SIGNAL(uiAddToOpenTracksList(QVariant,QVariant,QVariant,QVariant,QVariant)), uiMainWindow, SLOT(addToOpenTracksList(QVariant,QVariant,QVariant,QVariant,QVariant)));
        disconnect(uiMainWindow, SIGNAL(menuPause()),                                                         this,         SLOT(menuPause()));
        disconnect(uiMainWindow, SIGNAL(menuResume()),                                                        this,         SLOT(menuResume()));
        disconnect(uiMainWindow, SIGNAL(menuNext()),                                                          this,         SLOT(menuNext()));
        disconnect(uiMainWindow, SIGNAL(menuCollection(QVariant)),                                            this,         SLOT(menuCollection(QVariant)));
        disconnect(uiMainWindow, SIGNAL(menuQuit()),                                                          this,         SLOT(menuQuit()));
        disconnect(uiMainWindow, SIGNAL(collectionsDialogResults(QVariant)),                                  this,         SLOT(collectionsDialogResults(QVariant)));
        disconnect(uiMainWindow, SIGNAL(pluginUIResults(QVariant,QVariant)),                                  this,         SLOT(pluginUIResults(QVariant,QVariant)));
        disconnect(uiMainWindow, SIGNAL(getOpenTracks(QVariant,QVariant)),                                    this,         SLOT(getOpenTracks(QVariant,QVariant)));
        disconnect(uiMainWindow, SIGNAL(resolveOpenTracks(QVariant)),                                         this,         SLOT(resolveOpenTracks(QVariant)));
    }

    // what we're really interested in is the main application window
    uiMainWindow = qobject_cast<QQuickWindow*>(qmlApplicationEngine->rootObjects().first());

    // signal connections
    connect(this,         SIGNAL(uiUserMessage(QVariant)),                                             uiMainWindow, SLOT(displayUserMessage(QVariant)));
    connect(this,         SIGNAL(uiCollections(QVariant,QVariant)),                                    uiMainWindow, SLOT(fillCollectionsList(QVariant,QVariant)));
    connect(this,         SIGNAL(uiTrackInfo(QVariant,QVariant,QVariant,QVariant,QVariant)),           uiMainWindow, SLOT(updateTrackInfo(QVariant,QVariant,QVariant,QVariant,QVariant)));
    connect(this,         SIGNAL(uiPicture(QVariant)),                                                 uiMainWindow, SLOT(updateArt(QVariant)));
    connect(this,         SIGNAL(uiPosition(QVariant,QVariant)),                                       uiMainWindow, SLOT(updatePosition(QVariant, QVariant)));
    connect(this,         SIGNAL(uiClearPlaylist()),                                                   uiMainWindow, SLOT(clearPlaylist()));
    connect(this,         SIGNAL(uiAddToPlaylist(QVariant,QVariant,QVariant)),                         uiMainWindow, SLOT(addToPlaylist(QVariant,QVariant,QVariant)));
    connect(this,         SIGNAL(uiPaused()),                                                          uiMainWindow, SLOT(paused()));
    connect(this,         SIGNAL(uiResumed()),                                                         uiMainWindow, SLOT(resumed()));
    connect(this,         SIGNAL(uiClearPluginsList()),                                                uiMainWindow, SLOT(clearPluginsList()));
    connect(this,         SIGNAL(uiAddToPluginsList(QVariant,QVariant)),                               uiMainWindow, SLOT(addToPluginsList(QVariant,QVariant)));
    connect(this,         SIGNAL(uiDisplayPluginUI(QVariant,QVariant)),                                uiMainWindow, SLOT(displayPluginUI(QVariant,QVariant)));
    connect(this,         SIGNAL(uiAddToOpenTracksList(QVariant,QVariant,QVariant,QVariant,QVariant)), uiMainWindow, SLOT(addToOpenTracksList(QVariant,QVariant,QVariant,QVariant,QVariant)));
    connect(this,         SIGNAL(uiAddToSearchList(QVariant,QVariant,QVariant)),                       uiMainWindow, SLOT(addToSearchList(QVariant,QVariant,QVariant)));
    connect(uiMainWindow, SIGNAL(menuPause()),                                                         this,         SLOT(menuPause()));
    connect(uiMainWindow, SIGNAL(menuResume()),                                                        this,         SLOT(menuResume()));
    connect(uiMainWindow, SIGNAL(menuNext()),                                                          this,         SLOT(menuNext()));
    connect(uiMainWindow, SIGNAL(menuCollection(QVariant)),                                            this,         SLOT(menuCollection(QVariant)));
    connect(uiMainWindow, SIGNAL(menuPlugin(QVariant)),                                                this,         SLOT(menuPlugin(QVariant)));
    connect(uiMainWindow, SIGNAL(menuQuit()),                                                          this,         SLOT(menuQuit()));
    connect(uiMainWindow, SIGNAL(collectionsDialogResults(QVariant)),                                  this,         SLOT(collectionsDialogResults(QVariant)));
    connect(uiMainWindow, SIGNAL(pluginUIResults(QVariant,QVariant)),                                  this,         SLOT(pluginUIResults(QVariant,QVariant)));
    connect(uiMainWindow, SIGNAL(getOpenTracks(QVariant,QVariant)),                                    this,         SLOT(getOpenTracks(QVariant,QVariant)));
    connect(uiMainWindow, SIGNAL(startSearch(QVariant)),                                               this,         SLOT(startSearch(QVariant)));
    connect(uiMainWindow, SIGNAL(resolveOpenTracks(QVariant)),                                         this,         SLOT(resolveOpenTracks(QVariant)));
}


// application signal handler
void WaverApplication::stateChanged(Qt::ApplicationState state)
{
    // call the appropriate method
    switch (state) {
    case Qt::ApplicationActive:
        active();
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
void WaverApplication::menuQuit()
{
    emit ipcSend(IpcMessageUtils::Quit);

    #ifdef Q_OS_ANDROID
        // Android tries to keep the service alive unless it's explicitly stopped
        QAndroidJniObject activity = QtAndroid::androidActivity();
        activity.callMethod<void>("stopService");
    #endif

    quit();
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


// application activated
void WaverApplication::active()
{
    // start interprocess communication handler if not yet running
    if (!tcpThread.isRunning()) {
        tcpThread.start();
    }

    // attempt to connect to the server if not yet connected
    emit ipcConnect();
}


// application suspended
void WaverApplication::suspended()
{
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
    emit uiUserMessage(messageText);

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
    PluginSource::TrackInfo trackInfo = ipcMessageUtils.jsonDocumentToTrackInfo(jsonDocument);

    emit uiTrackInfo(trackInfo.title, trackInfo.performer, trackInfo.album, trackInfo.year, trackInfo.track);

    if (trackInfo.pictures.count() < 1) {
        emit uiPicture("images/waver.png");
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

    QJsonArray playlist = jsonDocument.array();

    foreach (QJsonValue item, playlist) {
        PluginSource::TrackInfo trackInfo = ipcMessageUtils.jsonDocumentToTrackInfo(QJsonDocument(item.toObject()));
        emit uiAddToPlaylist((trackInfo.pictures.count() > 0 ? trackInfo.pictures.at(0).toString() : "images/waver.png"), trackInfo.title, trackInfo.performer);
    }
}


// plugins list
void WaverApplication::updateUIPluginsList(QJsonDocument jsonDocument)
{
    emit uiClearPluginsList();

    // sort them by plugin name
    QVariantHash plugins = jsonDocument.object().toVariantHash();
    QVariantList values  = plugins.values();
    qSort(values);
    foreach (QVariant value, values) {
        foreach(QString id, plugins.keys(value)) {
            emit uiAddToPluginsList(id, plugins.value(id));
        }
    }
}


// plugin UI
void WaverApplication::showPluginUI(QJsonDocument jsonDocument)
{
    QVariantHash data     = jsonDocument.object().toVariantHash();
    QString      pluginId = data.value("plugin_id").toString();
    QString      uiQml    = data.value("ui_qml").toString();

    emit uiDisplayPluginUI(pluginId, uiQml);
}


// playlist add
void WaverApplication::updateOpenTracksList(QJsonDocument jsonDocument)
{
    QVariantHash data       = jsonDocument.object().toVariantHash();
    QString      pluginId   = data.value("plugin_id").toString();
    QJsonArray   tracksJson = QJsonArray::fromVariantList(data.value("tracks").toList());

    foreach (QVariant track, tracksJson) {
        QVariantHash trackHash = track.toHash();
        emit uiAddToOpenTracksList(pluginId, trackHash.value("hasChildren"), trackHash.value("selectable"), trackHash.value("label"), trackHash.value("id"));
    }
}


// playlist add
void WaverApplication::updateSearchList(QJsonDocument jsonDocument)
{
    QVariantHash data       = jsonDocument.object().toVariantHash();
    QString      pluginId   = data.value("plugin_id").toString();
    QJsonArray   tracksJson = QJsonArray::fromVariantList(data.value("tracks").toList());

    foreach (QVariant track, tracksJson) {
        QVariantHash trackHash = track.toHash();
        emit uiAddToSearchList(pluginId, trackHash.value("label"), trackHash.value("id"));
    }
}


// interprocess communication signal handler
void WaverApplication::ipcOpened()
{
    emit ipcSend(IpcMessageUtils::CollectionList);
    emit ipcSend(IpcMessageUtils::TrackInfo);
    emit ipcSend(IpcMessageUtils::Playlist);
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
        emit updateUICollections(jsonDocument);
        break;

    case IpcMessageUtils::InfoMessage:
        emit uiUserMessage(jsonDocument.object().toVariantHash().value("message"));
        break;

    case IpcMessageUtils::OpenTracks:
        updateOpenTracksList(jsonDocument);
        break;

    case IpcMessageUtils::Pause:
        emit uiPaused();
        break;

    case IpcMessageUtils::Playlist:
        updateUIPlaylist(jsonDocument);
        break;

    case IpcMessageUtils::PluginsWithUI:
        updateUIPluginsList(jsonDocument);
        break;

    case IpcMessageUtils::PluginUI:
        showPluginUI(jsonDocument);
        break;

    case IpcMessageUtils::Position:
        updateUIPosition(jsonDocument);
        break;

    case IpcMessageUtils::Resume:
        emit uiResumed();
        break;

    case IpcMessageUtils::Search:
        updateSearchList(jsonDocument);
        break;

    case IpcMessageUtils::TrackInfo:
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
    emit uiUserMessage(error);
}
