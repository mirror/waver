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


#ifndef PLUGININFO_H
#define PLUGININFO_H

#include "pluginbase.h"
#include "pluginsource.h"


class PluginInfo : public PluginBase {
        Q_OBJECT


    public:

        Q_INVOKABLE virtual void setUrl(QUrl url) = 0;


    signals:

        void updateTrackInfo(QUuid uniqueId, PluginSource::TrackInfo trackInfo);
        void addInfoHtml(QUuid uniqueId, QString info);

};

#endif // PLUGININFO_H
