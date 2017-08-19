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


#include "ipcmessageutils.h"

// static
int IpcMessageUtils::tcpPort()
{
    return 17400;
}


// constructor
IpcMessageUtils::IpcMessageUtils(QObject *parent) : QObject(parent)
{
    lastPartialIpcString = "";

    ipcMessagesToStrings[Unknown]                  = "unknown";
    ipcMessagesToStrings[AreYouAlive]              = "are_you_alive";
    ipcMessagesToStrings[CollectionList]           = "collection_list";
    ipcMessagesToStrings[CollectionMenuChange]     = "collection_menu_change";
    ipcMessagesToStrings[CollectionsDialogResults] = "collections_dialog_results";
    ipcMessagesToStrings[ImAlive]                  = "im_alive";
    ipcMessagesToStrings[InfoMessage]              = "info_message";
    ipcMessagesToStrings[Next]                     = "next";
    ipcMessagesToStrings[OpenTracks]               = "open_tracks";
    ipcMessagesToStrings[OpenTracksSelected]       = "open_tracks_selected";
    ipcMessagesToStrings[Pause]                    = "pause";
    ipcMessagesToStrings[Playlist]                 = "playlist";
    ipcMessagesToStrings[PlayPauseState]           = "play_pause_state";
    ipcMessagesToStrings[PluginsWithUI]            = "plugins_with_ui";
    ipcMessagesToStrings[PluginUI]                 = "plugin_ui";
    ipcMessagesToStrings[PluginUIResults]          = "plugin_ui_results";
    ipcMessagesToStrings[Position]                 = "position";
    ipcMessagesToStrings[Quit]                     = "quit";
    ipcMessagesToStrings[QuitClients]              = "quit_clients";
    ipcMessagesToStrings[Resume]                   = "resume";
    ipcMessagesToStrings[Search]                   = "search";
    ipcMessagesToStrings[TrackAction]              = "track_action";
    ipcMessagesToStrings[TrackInfos]               = "track_info";
}


// static, create a string that is ready to be sent over interprocess communication TCP/IP connection
QString IpcMessageUtils::constructIpcString(IpcMessages ipcMessage)
{
    return QString("%1%2").arg(ipcMessagesToStrings.value(ipcMessage)).arg(MESSAGE_SEPARATOR);
}


// static, create a string that is ready to be sent over interprocess communication TCP/IP connection
QString IpcMessageUtils::constructIpcString(IpcMessages ipcMessage, QJsonDocument ipcJsonData)
{
    if (ipcJsonData.isEmpty()) {
        return QString("%1%2").arg(ipcMessagesToStrings.value(ipcMessage)).arg(MESSAGE_SEPARATOR);
    }
    return QString("%1:%2%3").arg(ipcMessagesToStrings.value(ipcMessage)).arg(QString(ipcJsonData.toJson(
                    QJsonDocument::Compact))).arg(MESSAGE_SEPARATOR);
}


// process a string that is received through interprocess communication TCP/IP connection
void IpcMessageUtils::processIpcString(QString ipcString)
{
    ipcString            = lastPartialIpcString + ipcString;
    lastPartialIpcString = "";

    lastCompleteIpcStrings.clear();
    lastCompleteIpcStrings.append(ipcString.split(MESSAGE_SEPARATOR));

    if (!ipcString.endsWith(MESSAGE_SEPARATOR)) {
        lastPartialIpcString = lastCompleteIpcStrings.last();
        lastCompleteIpcStrings.removeLast();
    }
}


// returns the number of complete strings last processed
int IpcMessageUtils::processedCount()
{
    return lastCompleteIpcStrings.count();
}


// returns a message last processed
IpcMessageUtils::IpcMessages IpcMessageUtils::processedIpcMessage(int index)
{
    if ((index < 0) || (index >= lastCompleteIpcStrings.count())) {
        return Unknown;
    }

    QString temp = lastCompleteIpcStrings.at(index);

    if (temp.indexOf(':') >= 0) {
        temp = temp.left(temp.indexOf(':'));
    }

    return ipcMessagesToStrings.key(temp);
}


// returns a data last processed
QJsonDocument IpcMessageUtils::processedIpcData(int index)
{
    if ((index < 0) || (index >= lastCompleteIpcStrings.count())) {
        return QJsonDocument();
    }

    QString temp = lastCompleteIpcStrings.at(index);

    if (temp.indexOf(':') < 0) {
        return QJsonDocument();
    }

    temp = temp.mid(temp.indexOf(':') + 1);
    return QJsonDocument::fromJson(temp.toUtf8());
}


// returns raw ipc string last processed
QString IpcMessageUtils::processedRaw(int index)
{
    if ((index < 0) || (index >= lastCompleteIpcStrings.count())) {
        return "";
    }

    return lastCompleteIpcStrings.at(index);
}


// convenience method
QJsonDocument IpcMessageUtils::trackInfoToJSONDocument(TrackInfo trackInfo)
{
    QStringList pictures;
    foreach (QUrl url, trackInfo.pictures) {
        pictures.append(url.toString());
    }

    QVariantHash actions;
    foreach (int key, trackInfo.actions.keys()) {
        actions.insert(QString("%1").arg(key), trackInfo.actions.value(key));
    }

    QJsonObject info;
    info.insert("url",       QJsonValue(trackInfo.url.toString()));
    info.insert("cast",      QJsonValue(trackInfo.cast));
    info.insert("pictures",  QJsonArray::fromStringList(pictures));
    info.insert("title",     QJsonValue(trackInfo.title));
    info.insert("performer", QJsonValue(trackInfo.performer));
    info.insert("album",     QJsonValue(trackInfo.album));
    info.insert("year",      QJsonValue(trackInfo.year));
    info.insert("track",     QJsonValue(trackInfo.track));
    info.insert("actions",   QJsonObject::fromVariantHash(actions));

    return QJsonDocument(info);
}


// convenience method
TrackInfo IpcMessageUtils::jsonDocumentToTrackInfo(QJsonDocument jsonDocument)
{
    QJsonObject info = jsonDocument.object();

    TrackInfo trackInfo;

    trackInfo.url       = QUrl::fromUserInput(info.value("url").toString());
    trackInfo.cast      = info.value("cast").toBool();
    trackInfo.title     = info.value("title").toString();
    trackInfo.performer = info.value("performer").toString();
    trackInfo.album     = info.value("album").toString();
    trackInfo.year      = info.value("year").toInt();
    trackInfo.track     = info.value("track").toInt();

    if (info.value("pictures").isArray()) {
        QVariantList pictures = info.value("pictures").toArray().toVariantList();
        foreach (QVariant url, pictures) {
            trackInfo.pictures.append(QUrl::fromUserInput(url.toString()));
        }
    }

    if (info.value("actions").isObject()) {
        QVariantHash actions = info.value("actions").toObject().toVariantHash();
        foreach (QString key, actions.keys()) {
            trackInfo.actions.insert(key.toInt(), actions.value(key).toString());
        }
    }

    return trackInfo;
}
