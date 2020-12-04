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
    ipcMessagesToStrings[Diagnostics]              = "diagnostics";
    ipcMessagesToStrings[ImAlive]                  = "im_alive";
    ipcMessagesToStrings[InfoMessage]              = "info_message";
    ipcMessagesToStrings[Next]                     = "next";
    ipcMessagesToStrings[OpenTracks]               = "open_tracks";
    ipcMessagesToStrings[OpenTracksSelected]       = "open_tracks_selected";
    ipcMessagesToStrings[OpenUrl]                  = "open_url";
    ipcMessagesToStrings[Options]                  = "options";
    ipcMessagesToStrings[OptionsResults]           = "options_results";
    ipcMessagesToStrings[Pause]                    = "pause";
    ipcMessagesToStrings[Playlist]                 = "playlist";
    ipcMessagesToStrings[PlayPauseState]           = "play_pause_state";
    ipcMessagesToStrings[Plugins]                  = "plugins";
    ipcMessagesToStrings[PluginsWithUI]            = "plugins_with_ui";
    ipcMessagesToStrings[PluginUI]                 = "plugin_ui";
    ipcMessagesToStrings[PluginUIResults]          = "plugin_ui_results";
    ipcMessagesToStrings[Position]                 = "position";
    ipcMessagesToStrings[Quit]                     = "quit";
    ipcMessagesToStrings[QuitClients]              = "quit_clients";
    ipcMessagesToStrings[Resume]                   = "resume";
    ipcMessagesToStrings[Search]                   = "search";
    ipcMessagesToStrings[SourcePriorities]         = "source_priorities";
    ipcMessagesToStrings[SourcePriorityResults]    = "source_priority_results";
    ipcMessagesToStrings[TrackAction]              = "track_action";
    ipcMessagesToStrings[TrackInfos]               = "track_info";
    ipcMessagesToStrings[Window]                   = "window";
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

    QString temp = QString("%1:%2%3").arg(ipcMessagesToStrings.value(ipcMessage), QString::fromUtf8(ipcJsonData.toJson(QJsonDocument::Compact)), QString(MESSAGE_SEPARATOR));
    return temp;
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
QJsonDocument IpcMessageUtils::trackInfoToJSONDocument(TrackInfo trackInfo, QVariantHash additionalInfo)
{
    QStringList pictures;
    foreach (QUrl url, trackInfo.pictures) {
        pictures.append(url.toString());
    }

    QVariantHash actions;
    foreach (PLUGINGLOBALS_H::TrackAction trackAction, trackInfo.actions) {
        QStringList ids;
        ids.append(trackAction.pluginId.toString());
        ids.append(QString("%1").arg(trackAction.id));
        actions.insert(ids.join('~'), trackAction.label);
    }

    QJsonObject info;
    info.insert("url",            QJsonValue(trackInfo.url.toString()));
    info.insert("cast",           QJsonValue(trackInfo.cast));
    info.insert("pictures",       QJsonArray::fromStringList(pictures));
    info.insert("title",          QJsonValue(trackInfo.title));
    info.insert("performer",      QJsonValue(trackInfo.performer));
    info.insert("album",          QJsonValue(trackInfo.album));
    info.insert("year",           QJsonValue(trackInfo.year));
    info.insert("track",          QJsonValue(trackInfo.track));
    info.insert("actions",        QJsonObject::fromVariantHash(actions));
    info.insert("additionalInfo", QJsonObject::fromVariantHash(additionalInfo));

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
            QStringList ids = key.split('~');

            PLUGINGLOBALS_H::TrackAction trackAction;
            trackAction.pluginId = QUuid(ids.at(0));
            trackAction.id       = ids.at(1).toInt();
            trackAction.label    = actions.value(key).toString();

            trackInfo.actions.append(trackAction);
        }
    }

    return trackInfo;
}


// convenience method
QVariantHash IpcMessageUtils::jsonDocumentToAdditionalInfo(QJsonDocument jsonDocument)
{
    QVariantHash additionalInfo;

    QJsonObject info = jsonDocument.object();
    if (info.value("additionalInfo").isObject()) {
        additionalInfo = info.value("additionalInfo").toObject().toVariantHash();
    }

    return additionalInfo;
}
