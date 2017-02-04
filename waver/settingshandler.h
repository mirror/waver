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


#ifndef SETTINGSHANDLER_H
#define SETTINGSHANDLER_H

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QObject>
#include <QRegExp>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QUuid>


class SettingsHandler : public QObject
{
    Q_OBJECT

public:

    static const QString DEFAULT_COLLECTION_NAME;

    explicit SettingsHandler(QObject *parent = 0);
    ~SettingsHandler();


private:

    QDir *settingsDir;

    QString pluginSettingsFileName(QUuid persistentUniqueId, QString collectionName);


signals:

    void collectionList(QStringList collections, QString currentCollection);

    void loadedPluginSettings(QUuid id, QJsonDocument settings);


public slots:

    void run();

    void saveCollectionList(QStringList collections, QString currentCollection);
    void saveCollectionList(QString currentCollection);
    void getCollectionList();

    void savePluginSettings(QUuid persistentUniqueId, QString collectionName, QJsonDocument settings);
    void loadPluginSettings(QUuid persistentUniqueId, QString collectionName);

};

#endif // SETTINGSHANDLER_H
