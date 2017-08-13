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


#ifndef WAVERAPPLICATION_H
#define WAVERAPPLICATION_H

#include <QGuiApplication>
#include <QJsonArray>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVariantHash>
#include <QVariantList>
#include <QVector>

#ifdef Q_OS_ANDROID
    #include <QtAndroid>
    #include <QAndroidJniObject>
#endif

#include "clienttcphandler.h"
#include "globals.h"
#include "ipcmessageutils.h"


class WaverApplication : public QGuiApplication {
        Q_OBJECT

    public:

        explicit WaverApplication(int &argc, char **argv);
        ~WaverApplication();

        void setQmlApplicationEngine(QQmlApplicationEngine *qmlApplicationEngine);


    private:

        QQuickWindow *uiMainWindow;

        QThread           tcpThread;
        ClientTcpHandler *tcpHandler;

        QTimer        uiPicturesTimer;
        QVector<QUrl> uiPictures;
        int           uiPictureIndex;

        void active();
        void inactive();
        void suspended();

        void quitWithMessage(QString messageText);
        void updateUICollections(QJsonDocument jsonDocument);
        void updateUITrackInfo(QJsonDocument jsonDocument);
        void updateUIPosition(QJsonDocument jsonDocument);
        void updateUIPlaylist(QJsonDocument jsonDocument);
        void updateUIPluginsList(QJsonDocument jsonDocument);
        void showPluginUI(QJsonDocument jsonDocument);
        void updateOpenTracksList(QJsonDocument jsonDocument);
        void updateSearchList(QJsonDocument jsonDocument);


    signals:

        void ipcConnect();
        void ipcDisconnect();
        void ipcSend(IpcMessageUtils::IpcMessages ipcMessage);
        void ipcSend(IpcMessageUtils::IpcMessages ipcMessage, QJsonDocument ipcJsonData);

        void uiActivated();
        void uiInactivated();
        void uiUserMessage(QVariant messageText);
        void uiPaused();
        void uiResumed();
        void uiCollections(QVariant collections, QVariant currentIndex);
        void uiTrackInfo(QVariant title, QVariant performer, QVariant album, QVariant year, QVariant track);
        void uiPicture(QVariant url);
        void uiPosition(QVariant elapsed, QVariant remaining);
        void uiActions(QVariant linksText);
        void uiClearPlaylist();
        void uiAddToPlaylist(QVariant pictureUrl, QVariant title, QVariant performer, QVariant actions, QVariant showActions);
        void uiClearPluginsList();
        void uiAddToPluginsList(QVariant id, QVariant label);
        void uiDisplayPluginUI(QVariant id, QVariant qml);
        void uiAddToOpenTracksList(QVariant pluginId, QVariant hasChildren, QVariant selectable, QVariant label, QVariant id);
        void uiAddToSearchList(QVariant pluginId, QVariant label, QVariant id);
        void uiAbout(QVariant appName, QVariant appVersion, QVariant appDescription);


    public slots:

        void menuPause();
        void menuResume();
        void menuNext();
        void menuCollection(QVariant collection);
        void menuPlugin(QVariant id);
        void menuAbout();
        void menuQuit();
        void collectionsDialogResults(QVariant collectionsArray);
        void pluginUIResults(QVariant id, QVariant results);
        void getOpenTracks(QVariant pluginId, QVariant parentId);
        void startSearch(QVariant criteria);
        void resolveOpenTracks(QVariant results);
        void trackAction(QVariant index, QVariant action);


    private slots:

        void stateChanged(Qt::ApplicationState state);

        void ipcOpened();
        void ipcClosed();
        void ipcMessage(IpcMessageUtils::IpcMessages message, QJsonDocument jsonDocument);
        void ipcError(bool fatal, QString error);

        void updateUIPicture();

};

#endif // WAVERAPPLICATION_H
