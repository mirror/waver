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


#ifndef PLUGINSOURCE_H
#define PLUGINSOURCE_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QUrl>

#include "../pluginglobals.h"
#include "pluginbase_004.h"


class PluginSource_004 : public PluginBase_004 {
        Q_OBJECT


    public:

        Q_INVOKABLE virtual void setUserAgent(QString userAgent) = 0;


    signals:

        void ready(QUuid uniqueId);
        void unready(QUuid uniqueId);
        void playlist(QUuid uniqueId, TracksInfo tracksInfo);
        void replacement(QUuid uniqueId, TrackInfo trackInfo);
        void openTracksResults(QUuid uniqueId, OpenTracks openTracks);
        void searchResults(QUuid uniqueId, OpenTracks openTracks);
        void requestRemoveTracks(QUuid uniqueId);
        void requestRemoveTrack(QUuid uniqueId, QUrl url);
        void updateTrackInfo(QUuid uniqueId, TrackInfo trackInfo);


    public slots:

        virtual void unableToStart(QUuid uniqueId, QUrl url)                         = 0;
        virtual void castFinishedEarly(QUuid uniqueId, QUrl url, int playedSeconds)  = 0;
        virtual void getPlaylist(QUuid uniqueId, int trackCount)                     = 0;
        virtual void getReplacement(QUuid uniqueId)                                  = 0;
        virtual void getOpenTracks(QUuid uniqueId, QString parentId)                 = 0;
        virtual void resolveOpenTracks(QUuid uniqueId, QStringList selectedTrackIds) = 0;
        virtual void search(QUuid uniqueId, QString criteria)                        = 0;
        virtual void action(QUuid uniqueId, int actionKey, QUrl url)                 = 0;

};


#endif // PLUGINSOURCE_H
