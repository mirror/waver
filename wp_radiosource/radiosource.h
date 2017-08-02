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


#ifndef RADIOSOURCE_H
#define RADIOSOURCE_H

#include "wp_radiosource_global.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QUrl>
#include <QUuid>
#include <QVariantHash>
#include <QVector>
#include <QXmlStreamAttribute>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>

#include "../waver/pluginsource.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


extern "C" WP_RADIOSOURCE_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);

// TODO "Ban station" -> Ez rakja bele egy speckó <BANNED> kategóriába

class WP_RADIOSOURCE_EXPORT RadioSource : public PluginSource {
        Q_OBJECT

    public:

        int     pluginType()         override;
        QString pluginName()         override;
        int     pluginVersion()      override;
        QUuid   persistentUniqueId() override;
        bool    hasUI()              override;

        explicit RadioSource();
        ~RadioSource();


    private:

        struct Station {
            QString name;
            QString category;
            QUrl    url;
            int     unableToStartCount;
        };

        QUuid id;

        QVector<Station> stations;
        QVector<Station> selectedStations;

        QJsonDocument configToJson();
        void          jsonToConfig(QJsonDocument jsonDocument);


    public slots:

        void run() override;

        void loadedConfiguration(QUuid uniqueId, QJsonDocument configuration) override;

        void getUiQml(QUuid uniqueId)                         override;
        void uiResults(QUuid uniqueId, QJsonDocument results) override;

        void unableToStart(QUuid uniqueId, QUrl url)                       override;
        void getPlaylist(QUuid uniqueId, int maxCount)                     override;
        void getOpenTracks(QUuid uniqueId, QString parentId)               override;
        void resolveOpenTracks(QUuid uniqueId, QStringList selectedTracks) override;

        void search(QUuid uniqueId, QString criteria) override;
        void action(QUuid uniqueId, int actionKey)    override;

};

#endif // RADIOSOURCE_H
