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


#ifndef TCPMESSAGEUTILS_H
#define TCPMESSAGEUTILS_H

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QVariantHash>
#include <QVariantList>

#include "pluginglobals.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


const int  TCP_TIMEOUT       = 2500;
const char MESSAGE_SEPARATOR = 30;

class IpcMessageUtils : public QObject {
        Q_OBJECT

    public:

        enum IpcMessages {
            Unknown = 0,
            AreYouAlive,
            CollectionList,
            CollectionMenuChange,
            CollectionsDialogResults,
            Diagnostics,
            ImAlive,
            InfoMessage,
            Next,
            OpenTracks,
            OpenTracksSelected,
            OpenUrl,
            Options,
            OptionsResults,
            Pause,
            Playlist,
            PlayPauseState,
            Plugins,
            PluginsWithUI,
            PluginUI,
            PluginUIResults,
            Position,
            Quit,
            QuitClients,
            Resume,
            Search,
            SourcePriorities,
            SourcePriorityResults,
            TrackAction,
            TrackInfos,
            Window,
        };


        static int tcpPort();

        explicit IpcMessageUtils(QObject *parent = 0);

        QString constructIpcString(IpcMessages ipcMessage);
        QString constructIpcString(IpcMessages ipcMessage, QJsonDocument ipcJsonData);

        void          processIpcString(QString ipcString);
        int           processedCount();
        IpcMessages   processedIpcMessage(int index);
        QJsonDocument processedIpcData(int index);
        QString       processedRaw(int index);

        QJsonDocument trackInfoToJSONDocument(TrackInfo trackInfo, QVariantHash additionalInfo);
        TrackInfo     jsonDocumentToTrackInfo(QJsonDocument jsonDocument);
        QVariantHash  jsonDocumentToAdditionalInfo(QJsonDocument jsonDocument);


    private:

        QHash<IpcMessages, QString> ipcMessagesToStrings;

        QString     lastPartialIpcString;
        QStringList lastCompleteIpcStrings;

};

Q_DECLARE_METATYPE(IpcMessageUtils::IpcMessages)

#endif // TCPMESSAGEUTILS_H
