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


#ifndef PLUGINSOURCE_H
#define PLUGINSOURCE_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QUrl>

#include "../pluginglobals.h"
#include "pluginbase_001.h"


class PluginSource_001 : public PluginBase_001 {
        Q_OBJECT


    signals:

        void ready(QUuid uniqueId);
        void unready(QUuid uniqueId);
        void playlist(QUuid uniqueId, TracksInfo tracksInfo);
        void openTracksResults(QUuid uniqueId, OpenTracks openTracks);
        void searchResults(QUuid uniqueId, OpenTracks openTracks);
        void requestRemoveTracks(QUuid uniqueId);


    public slots:

        virtual void unableToStart(QUuid uniqueId, QUrl url)                         = 0;
        virtual void getPlaylist(QUuid uniqueId, int maxCount)                       = 0;
        virtual void getOpenTracks(QUuid uniqueId, QString parentId)                 = 0;
        virtual void resolveOpenTracks(QUuid uniqueId, QStringList selectedTrackIds) = 0;
        virtual void search(QUuid uniqueId, QString criteria)                        = 0;
        virtual void action(QUuid uniqueId, int actionKey, QUrl url)                 = 0;

};


#endif // PLUGINSOURCE_H
