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



#include "taglibinfo.h"


// plugin factory
void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal)
{
    if (pluginTypesMask & PLUGIN_TYPE_INFO) {
        retVal->append((QObject *) new TagLibInfo());
    }
}


// global method
int TagLibInfo::pluginType()
{
    return PLUGIN_TYPE_INFO;
}


// global method
QString TagLibInfo::pluginName()
{
    return "Tag Reader";
}


// global method
int TagLibInfo::pluginVersion()
{
    return 2;
}


// overrided virtual function
QString TagLibInfo::waverVersionAPICompatibility()
{
    return "0.0.4";
}


// global method
bool TagLibInfo::hasUI()
{
    return false;
}


// global method
QUuid TagLibInfo::persistentUniqueId()
{
    return id;
}


// global method
void TagLibInfo::setUrl(QUrl url)
{
    // url can be set only once
    if (this->url.isEmpty()) {
        this->url = url;
    }
}


// global method
void TagLibInfo::setUserAgent(QString userAgent)
{
    Q_UNUSED(userAgent);
}


// constructor
TagLibInfo::TagLibInfo()
{
    id = QUuid("{51C91776-E385-4444-AF8D-A3A47E3FEFB8}");

    sendDiagnostics = false;
}


// thread entry point
void TagLibInfo::run()
{
    state = NotChecked;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }

    TrackInfo trackInfo;
    trackInfo.track = 0;
    trackInfo.url   = url;
    trackInfo.year  = 0;

    // checky-checky
    if (url.isEmpty()) {
        return;
    }

    // operates on local files only
    if (!url.isLocalFile()) {
        // still return an empty trackinfo just to let them know we're done
        emit updateTrackInfo(id, trackInfo);
        return;
    }


    TagLib::FileRef fileRef(QFile::encodeName(url.toLocalFile()).constData());

    if (!fileRef.isNull() && !fileRef.tag()->isEmpty()) {
        trackInfo.title     = TStringToQString(fileRef.tag()->title());
        trackInfo.performer = TStringToQString(fileRef.tag()->artist());
        trackInfo.album     = TStringToQString(fileRef.tag()->album());
        trackInfo.year      = fileRef.tag()->year();
        trackInfo.track     = fileRef.tag()->track();

        emit updateTrackInfo(id, trackInfo);

        if (!trackInfo.title.isEmpty() && !trackInfo.performer.isEmpty() && !trackInfo.album.isEmpty() && (trackInfo.year > 0) && (trackInfo.track > 0)) {
            state = Success;
        }
        else if (!trackInfo.title.isEmpty() || !trackInfo.performer.isEmpty() || !trackInfo.album.isEmpty() || (trackInfo.year > 0) || (trackInfo.track > 0)) {
            state = SomeFound;
        }
        else {
            state = NotFound;
        }

        if (sendDiagnostics) {
            sendDiagnosticsData();
        }

        return;
    }

    // still return an empty trackinfo just to let them know we're done
    emit updateTrackInfo(id, trackInfo);

    state = NotFound;
    if (sendDiagnostics) {
        sendDiagnosticsData();
    }
}


// this plugin has no configuration
void TagLibInfo::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// this plugin has no configuration
void TagLibInfo::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// configuration
void TagLibInfo::sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void TagLibInfo::globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void TagLibInfo::sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(error);
}


// this plugin has no configuration
void TagLibInfo::getUiQml(QUuid uniqueId)
{
    Q_UNUSED(uniqueId);
}


// this plugin has no configuration
void TagLibInfo::uiResults(QUuid uniqueId, QJsonDocument results)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(results);
}


// client wants to receive updates of this plugin's diagnostic information
void TagLibInfo::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = true;
    sendDiagnosticsData();
}


// client doesnt want to receive updates of this plugin's diagnostic information anymore
void TagLibInfo::stopDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    sendDiagnostics = false;
}


// this is unused here
void TagLibInfo::getInfo(QUuid uniqueId, TrackInfo trackinfo)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(trackinfo);
}


// helper
void TagLibInfo::sendDiagnosticsData()
{
    DiagnosticData diagnosticData;

    switch (state) {
        case NotChecked:
            diagnosticData.append({ "Status", "Not needed to be checked" });
            break;
        case Success:
            diagnosticData.append({ "Status", "Success" });
            break;
        case SomeFound:
            diagnosticData.append({ "Status", "Some but not all tags found" });
            break;
        case NotFound:
            diagnosticData.append({ "Status", "Tags not found" });
            break;
    }

    emit diagnostics(id, diagnosticData);
}
