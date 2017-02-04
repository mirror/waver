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


#ifndef LOCALSOURCE_H
#define LOCALSOURCE_H

#include "wp_localsource_global.h"

#include <QtGlobal>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMimeDatabase>
#include <QMutex>
#include <QObject>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUuid>
#include <QVariant>
#include <QVector>

#include <QDebug>

#include "filescanner.h"
#include "../waver/pluginsource.h"


extern "C" WP_LOCALSOURCE_EXPORT void wp_plugin_factory(int pluginTypesMask, PluginFactoryResults *retVal);


class WP_LOCALSOURCE_EXPORT LocalSource : public PluginSource
{
    Q_OBJECT

public:

    int     pluginType()         override;
    QString pluginName()         override;
    int     pluginVersion()      override;
    QUuid   persistentUniqueId() override;
    bool    hasUI()              override;


    explicit LocalSource();
    ~LocalSource();


private:

    QUuid id;

    QMutex                mutex;
    QVector<FileScanner*> scanners;
    bool                  readyEmitted;

    QStringList directories;
    QStringList trackFileNames;
    QStringList alreadyPlayedTrackFileNames;

    QMimeDatabase mimeDatabase;

    void scanDir(QString dir);

    QJsonDocument configToJson();
    void          jsonToConfig(QJsonDocument jsonDocument);

    bool      isTrackFile(QFileInfo fileInfo);
    TrackInfo trackInfoFromFilePath(QString filePath);


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


private slots:

    void scannerFoundFirst();
    void scannerFinished();

    void readyTimer();

};

#endif // LOCALSOURCE_H
