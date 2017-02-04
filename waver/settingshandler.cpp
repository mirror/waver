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


#include "settingshandler.h"

const QString SettingsHandler::DEFAULT_COLLECTION_NAME = "Default";

// constructor
SettingsHandler::SettingsHandler(QObject *parent) : QObject(parent)
{
    settingsDir = NULL;

    // find application data location
    QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataLocation.length() > 0) {
        settingsDir = new QDir(dataLocation);
        if (!settingsDir->exists()) {
            settingsDir->mkpath(dataLocation);
        }
    }
}


// destructor
SettingsHandler::~SettingsHandler()
{
    if (settingsDir != NULL) {
        delete settingsDir;
    }
}


// helper
QString SettingsHandler::pluginSettingsFileName(QUuid persistentUniqueId, QString collectionName)
{
    return QString("%1_%2.cfg").arg(persistentUniqueId.toString().replace(QRegExp("[{}]"), "")).arg(collectionName.replace(QRegExp("\\W"), "_")).toLower();
}


// thread entry point
void SettingsHandler::run()
{

}


// slot
void SettingsHandler::saveCollectionList(QStringList collections, QString currentCollection)
{
    // can't delete default collection
    if (!collections.contains(DEFAULT_COLLECTION_NAME)) {
        collections.append(DEFAULT_COLLECTION_NAME);
    }

    // current collection may be deleted
    if (!collections.contains(currentCollection)) {
        currentCollection = DEFAULT_COLLECTION_NAME;
    }

    // save
    QFile file(settingsDir->absoluteFilePath("collections.cfg"));
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        return;
    }
    file.write(currentCollection.append("\n").toUtf8());
    foreach(QString collection, collections) {
        file.write(collection.append("\n").toUtf8());
    }
    file.close();

    // send it back because it might changed
    getCollectionList();
}


// slot
void SettingsHandler::saveCollectionList(QString currentCollection)
{
    // get existing collections list
    QStringList collections;
    QFile file(settingsDir->absoluteFilePath("collections.cfg"));
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        collections.append(DEFAULT_COLLECTION_NAME);
    }
    else {
        collections.append(QString(file.readAll()).split("\n"));
        file.close();
        collections.removeAll("");
        collections.removeFirst();
    }

    // save
    saveCollectionList(collections, currentCollection);
}


// slot
void SettingsHandler::getCollectionList()
{
    QStringList collections;

    QFile file(settingsDir->absoluteFilePath("collections.cfg"));
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        collections.append(DEFAULT_COLLECTION_NAME);
        emit collectionList(collections, DEFAULT_COLLECTION_NAME);
        return;
    }
    collections.append(QString(file.readAll()).split("\n"));
    file.close();
    collections.removeAll("");

    QString currentCollection = collections.at(0);
    collections.removeFirst();

    emit collectionList(collections, currentCollection);
}


// slot
void SettingsHandler::savePluginSettings(QUuid persistentUniqueId, QString collectionName, QJsonDocument settings)
{
    if (settingsDir == NULL) {
        return;
    }

    QFile file(settingsDir->absoluteFilePath(pluginSettingsFileName(persistentUniqueId, collectionName)));
    if (!file.open(QFile::WriteOnly)) {
        return;
    }
    file.write(settings.toJson());
    file.flush();
    file.close();
}


//slot
void SettingsHandler::loadPluginSettings(QUuid persistentUniqueId, QString collectionName)
{
    QJsonDocument returnValue;

    if (settingsDir == NULL) {
        emit loadedPluginSettings(persistentUniqueId, returnValue);
        return;
    }

    QFile file(settingsDir->absoluteFilePath(pluginSettingsFileName(persistentUniqueId, collectionName)));
    if (!file.open(QFile::ReadOnly)) {
        emit loadedPluginSettings(persistentUniqueId, returnValue);
        return;
    }
    returnValue = QJsonDocument::fromJson(file.readAll());
    file.close();

    emit loadedPluginSettings(persistentUniqueId, returnValue);
}
