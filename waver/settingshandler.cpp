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


#include "settingshandler.h"

const QString SettingsHandler::DEFAULT_COLLECTION_NAME = "Default";

// constructor
SettingsHandler::SettingsHandler(QObject *parent) : QObject(parent)
{
    settingsDir       = NULL;
    waverFakePluginId = QUuid("{040020F4-155B-48E8-8382-2AC528845063}");
    cleanedUp         = false;
}


// destructor
SettingsHandler::~SettingsHandler()
{
    if (settingsDir != NULL) {
        delete settingsDir;
    }

    foreach (QString connectionName, QSqlDatabase::connectionNames()) {
        QSqlDatabase database = QSqlDatabase::database(connectionName, false);
        database.close();
        while (database.isOpen()) {
            QThread::currentThread()->msleep(50);
        }
        QSqlDatabase::removeDatabase(connectionName);
    }
}


// helper
QString SettingsHandler::pluginSettingsFileName(QUuid persistentUniqueId, QString collectionName)
{
    if (!cleanedUp && !pluginIds.contains(persistentUniqueId)) {
        pluginIds.append(persistentUniqueId);
    }

    if (collectionName.isEmpty()) {
        return settingsDir->absoluteFilePath((QString("%1.cfg").arg(persistentUniqueId.toString().replace(QRegExp("[{}]"), ""))).toLower());
    }
    return settingsDir->absoluteFilePath((QString("%1_%2.cfg").arg(persistentUniqueId.toString().replace(QRegExp("[{}]"), "")).arg(collectionName.replace(QRegExp("\\W"), "_"))).toLower());
}


// helper
QString SettingsHandler::pluginSettingsDatabaseName(QUuid persistentUniqueId, QString collectionName, bool temporary)
{
    if (!cleanedUp && !pluginIds.contains(persistentUniqueId)) {
        pluginIds.append(persistentUniqueId);
    }

    if (temporary) {
        return "";
    }
    if (collectionName.isEmpty()) {
        return settingsDir->absoluteFilePath((QString("%1.db").arg(persistentUniqueId.toString().replace(QRegExp("[{}]"), ""))).toLower());
    }
    return settingsDir->absoluteFilePath((QString("%1_%2.db").arg(persistentUniqueId.toString().replace(QRegExp("[{}]"), "")).arg(collectionName.replace(QRegExp("\\W"), "_"))).toLower());
}


// helper
QString SettingsHandler::pluginSettingsConnectionName(QUuid persistentUniqueId, QString collectionName, bool temporary)
{
    QString connectionName = QString("%1").arg(persistentUniqueId.toString().replace(QRegExp("[{}]"), ""));
    if (!collectionName.isEmpty()) {
        connectionName.append(QString("_%1").arg(collectionName.replace(QRegExp("\\W"), "_")));
    }
    if (temporary) {
        connectionName.append("_temp");
    }
    return connectionName.toLower();
}


// thread entry point
void SettingsHandler::run()
{
    // find application data location
    QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataLocation.length() > 0) {
        settingsDir = new QDir(dataLocation);
        if (!settingsDir->exists()) {
            settingsDir->mkpath(dataLocation);
        }
    }

    // courtesy of settings handler
    loadWaverSettings("");

    // perform cleanup later to have a chance to collect plugin ids
    QTimer::singleShot(60000, this, SLOT(cleanup()));
}


// slot
void SettingsHandler::saveWaverSettings(QString collectionName, QJsonDocument settings)
{
    if (settingsDir == NULL) {
        return;
    }

    QFile file(pluginSettingsFileName(waverFakePluginId, collectionName));
    if (!file.open(QFile::WriteOnly)) {
        return;
    }
    file.write(settings.toJson());
    file.flush();
    file.close();
}


// slot
void SettingsHandler::loadWaverSettings(QString collectionName)
{
    QJsonDocument returnValue;

    if (settingsDir != NULL) {
        QFile file(pluginSettingsFileName(waverFakePluginId, collectionName));
        if (file.open(QFile::ReadOnly)) {
            returnValue = QJsonDocument::fromJson(file.readAll());
            file.close();
        }
    }

    if (collectionName.isEmpty()) {
        loadedWaverGlobalSettings(returnValue);
        return;
    }

    emit loadedWaverSettings(returnValue);
}


// slot
void SettingsHandler::savePluginSettings(QUuid persistentUniqueId, QString collectionName, QJsonDocument settings)
{
    if (settingsDir == NULL) {
        return;
    }

    QFile file(pluginSettingsFileName(persistentUniqueId, collectionName));
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

    if (settingsDir != NULL) {
        QFile file(pluginSettingsFileName(persistentUniqueId, collectionName));
        if (file.open(QFile::ReadOnly)) {
            returnValue = QJsonDocument::fromJson(file.readAll());
            file.close();
        }
    }

    if (collectionName.isEmpty()) {
        loadedPluginGlobalSettings(persistentUniqueId, returnValue);
        return;
    }

    emit loadedPluginSettings(persistentUniqueId, returnValue);
}


// slot
void SettingsHandler::executeSql(QUuid persistentUniqueId, QString collectionName, bool temporary, QString clientIdentifier, int clientSqlIdentifier, QString sql, QVariantList values)
{
    QString connectionName = pluginSettingsConnectionName(persistentUniqueId, collectionName, temporary);

    QSqlDatabase database = QSqlDatabase::database(connectionName, true);
    if (!database.isValid()) {
        database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        database.setDatabaseName(pluginSettingsDatabaseName(persistentUniqueId, collectionName, temporary));
        database.open();
    }
    if (!database.isOpen()) {
        emit sqlError(persistentUniqueId, temporary, clientIdentifier, clientSqlIdentifier, database.lastError().text());
        return;
    }

    QSqlQuery query(database);
    if (!query.prepare(sql)) {
        emit sqlError(persistentUniqueId, temporary, clientIdentifier, clientSqlIdentifier, query.lastError().text());
        return;
    }

    foreach (QVariant value, values) {
        query.addBindValue(value);
    }

    if (!query.exec()) {
        emit sqlError(persistentUniqueId, temporary, clientIdentifier, clientSqlIdentifier, query.lastError().text());
    }

    SqlResults results;
    if (query.isSelect()) {
        while (query.next()) {
            QSqlRecord   record = query.record();
            QVariantHash result;
            for (int i = 0; i < record.count(); i++) {
                result.insert(record.fieldName(i), record.value(i));
            }
            results.append(result);
        }
    }
    else {
        database.exec("COMMIT");
    }

    query.finish();

    if (collectionName.isEmpty()) {
        emit globalSqlResults(persistentUniqueId, temporary, clientIdentifier, clientSqlIdentifier, results);
        return;
    }

    emit sqlResults(persistentUniqueId, temporary, clientIdentifier, clientSqlIdentifier, results);
}


// private slot
void SettingsHandler::cleanup()
{
    cleanedUp = true;

    if (settingsDir == NULL) {
        return;
    }

    QJsonDocument jsonDocument;
    QFile file(pluginSettingsFileName(waverFakePluginId, ""));
    if (file.open(QFile::ReadOnly)) {
        jsonDocument = QJsonDocument::fromJson(file.readAll());
        file.close();
    }

    QStringList collections;
    if (jsonDocument.object().contains("collections")) {
        foreach (QJsonValue jsonValue, jsonDocument.object().value("collections").toArray()) {
            collections.append(jsonValue.toString());
        }
    }
    if (!collections.contains(SettingsHandler::DEFAULT_COLLECTION_NAME)) {
        collections.prepend(SettingsHandler::DEFAULT_COLLECTION_NAME);
    }

    QStringList allPossibleConfigFilesAndDatabases;
    foreach (QUuid pluginId, pluginIds) {
        allPossibleConfigFilesAndDatabases.append(pluginSettingsFileName(pluginId, ""));
        allPossibleConfigFilesAndDatabases.append(pluginSettingsDatabaseName(pluginId, "", false));
        foreach (QString collection, collections) {
            allPossibleConfigFilesAndDatabases.append(pluginSettingsFileName(pluginId, collection));
            allPossibleConfigFilesAndDatabases.append(pluginSettingsDatabaseName(pluginId, collection, false));
        }
    }

    QFileInfoList configFiles = settingsDir->entryInfoList();
    foreach (QFileInfo configFile, configFiles) {
        if (configFile.suffix().isEmpty() || ((configFile.suffix().compare("cfg") != 0) && (configFile.suffix().compare("db") != 0))) {
            continue;
        }

        if (!allPossibleConfigFilesAndDatabases.contains(configFile.absoluteFilePath())) {
            QFile::remove(configFile.absoluteFilePath());
        }
    }
}
