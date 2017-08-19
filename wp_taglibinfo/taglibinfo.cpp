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
    return 1;
}


// overrided virtual function
QString TagLibInfo::waverVersionAPICompatibility()
{
    return "0.0.1";
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


// constructor
TagLibInfo::TagLibInfo()
{
    id = QUuid("{51C91776-E385-4444-AF8D-A3A47E3FEFB8}");
}


// thread entry point
void TagLibInfo::run()
{
    // checky-checky
    if (url.isEmpty()) {
        return;
    }

    // operates on local files only
    if (!url.isLocalFile()) {
        return;
    }

    TrackInfo trackInfo;

    TagLib::FileRef fileRef(QFile::encodeName(url.toLocalFile()).constData());

    trackInfo.title     = TStringToQString(fileRef.tag()->title());
    trackInfo.performer = TStringToQString(fileRef.tag()->artist());
    trackInfo.album     = TStringToQString(fileRef.tag()->album());
    trackInfo.year      = fileRef.tag()->year();
    trackInfo.track     = fileRef.tag()->track();

    emit updateTrackInfo(id, trackInfo);
}


// this plugin has no configuration
void TagLibInfo::loadedConfiguration(QUuid uniqueId, QJsonDocument configuration)
{
    Q_UNUSED(uniqueId);
    Q_UNUSED(configuration);
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

