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



#include "lyrics.h"


// plugin factory
void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal)
{
    if (pluginTypesMask & PLUGIN_TYPE_INFO) {
        retVal->append((QObject *) new Lyrics());
    }
}


// global method
int Lyrics::pluginType()
{
    return PLUGIN_TYPE_INFO;
}


// global method
QString Lyrics::pluginName()
{
    return "Lyrics Search";
}


// global method
int Lyrics::pluginVersion()
{
    return 1;
}


// overrided virtual function
QString Lyrics::waverVersionAPICompatibility()
{
    return "0.0.5";
}


// global method
bool Lyrics::hasUI()
{
    return false;
}


// global method
QUuid Lyrics::persistentUniqueId()
{
    return id;
}


// global method
void Lyrics::setUrl(QUrl url)
{
    Q_UNUSED(url);
}


// global method
void Lyrics::setUserAgent(QString userAgent)
{
    Q_UNUSED(userAgent);
}


// global method
QUrl Lyrics::menuImageURL()
{
    return QUrl();
}


// constructor
Lyrics::Lyrics()
{
    id = QUuid("{DB1F54D9-B650-4B66-B99A-693595F48A1C}");
}


// thread entry point
void Lyrics::run()
{
    TrackInfo trackInfo;
    trackInfo.track = 0;
    trackInfo.year  = 0;
    trackInfo.actions.append({ id, -1000, "Lyrics search" });
    emit updateTrackInfo(id, trackInfo);
}


// this plugin has no configuration
void Lyrics::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// this plugin has no configuration
void Lyrics::loadedGlobalConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
}


// configuration
void Lyrics::sqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void Lyrics::globalSqlResults(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, SqlResults results)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(results);
}


// configuration
void Lyrics::sqlError(QUuid persistentUniqueId, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString error)
{
    Q_UNUSED(persistentUniqueId);
    Q_UNUSED(temporary);
    Q_UNUSED(clientIdentifier);
    Q_UNUSED(clientSqlIdentifier);
    Q_UNUSED(error);
}


// this plugin has no configuration
void Lyrics::getUiQml(QUuid uniqueId)
{
    Q_UNUSED(uniqueId);
}


// this plugin has no configuration
void Lyrics::uiResults(QUuid uniqueId, QJsonDocument results)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(results);
}


// client wants to receive updates of this plugin's diagnostic information
void Lyrics::startDiagnostics(QUuid uniqueId)
{
    if (uniqueId != id) {
        return;
    }

    DiagnosticData diagnosticData;

    diagnosticData.append({ "Status", "This plugin has no diagnostics data" });

    emit diagnostics(id, diagnosticData);
}


// client doesn't want to receive updates of this plugin's diagnostic information anymore
void Lyrics::stopDiagnostics(QUuid uniqueId)
{
    Q_UNUSED(uniqueId);
}


// track action
void Lyrics::action(QUuid uniqueId, int actionKey, TrackInfo trackInfo)
{
    if (uniqueId != id) {
        return;
    }

    if (actionKey != -1000) {
        return;
    }

    if (trackInfo.cast) {
        emit infoMessage(id, QString("Can not search lyrics for %1").arg(trackInfo.title));
        return;
    }

    emit openUrl(QUrl(QString("http://google.com/search?q=%1 %2 lyrics").arg(trackInfo.performer).arg(trackInfo.title)));
}

