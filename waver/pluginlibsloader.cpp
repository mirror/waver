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


#include "pluginlibsloader.h"


// helping hand
bool PluginLibsLoader::isPluginCompatible(QString pluginWaverVersionAPICompatibility, QString waverVersionAPI)
{
    QStringList pluginVersion = pluginWaverVersionAPICompatibility.split('.');
    QStringList waverVersion  = waverVersionAPI.split('.');

    if ((pluginVersion.count() != 3) || (waverVersion.count() != 3)) {
        return false;
    }

    bool OK;
    int pMajor;
    int pMinor;
    int pRev;
    int wMajor;
    int wMinor;
    int wRev;

    pMajor = pluginVersion.at(0).toInt(&OK);
    if (!OK) {
        return false;
    }
    pMinor = pluginVersion.at(1).toInt(&OK);
    if (!OK) {
        return false;
    }
    pRev = pluginVersion.at(2).toInt(&OK);
    if (!OK) {
        return false;
    }
    wMajor = waverVersion.at(0).toInt(&OK);
    if (!OK) {
        return false;
    }
    wMinor = waverVersion.at(1).toInt(&OK);
    if (!OK) {
        return false;
    }
    wRev = waverVersion.at(2).toInt(&OK);
    if (!OK) {
        return false;
    }

    if (pMajor > wMajor) {
        return false;
    }
    if ((pMajor == wMajor) && (pMinor > wMinor)) {
        return false;
    }
    if ((pMajor == wMajor) && (pMinor == wMinor) && (pRev < wRev)) {
        return false;
    }

    return true;
}


// constructor
PluginLibsLoader::PluginLibsLoader(QObject *parent, LoadedLibs *loadedLibs) : QObject(parent)
{
    this->loadedLibs = loadedLibs;
}


// entry point for dedicated thread
void PluginLibsLoader::run()
{
    // list of directories to search in
    QStringList searchDirs;

    QDir appDir(QCoreApplication::applicationDirPath());

    #ifdef QT_DEBUG
    appDir.cdUp();
    #ifdef Q_OS_WIN
    appDir.cdUp();
    #endif
    #endif

    searchDirs.append(appDir.absolutePath());

    searchDirs.append(QCoreApplication::libraryPaths());

    QString libEnv = QProcessEnvironment::systemEnvironment().value("LD_LIBRARY_PATH");
    if (libEnv.length() > 0) {
        searchDirs.append(libEnv.split(":"));
    }

    QString easyPluginInstallDirPath = QDir::homePath() + "/waver_plugins";

    #ifdef Q_OS_WIN
    easyPluginInstallDirPath = QDir::fromNativeSeparators("C:\\waver_plugins");
    #endif

    #ifdef Q_OS_ANDROID
    easyPluginInstallDirPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/waver_plugins";
    #endif

    QDir easyPluginInstallDir(easyPluginInstallDirPath);
    if (!easyPluginInstallDir.exists()) {
        easyPluginInstallDir.mkpath(easyPluginInstallDirPath);
    }
    if (easyPluginInstallDir.exists()) {
        searchDirs.append(easyPluginInstallDir.absolutePath());
    }

    searchDirs.removeDuplicates();

    // search
    QStringList libPaths;
    foreach (QString searchDir, searchDirs) {
        libSearch(searchDir, &libPaths);
    }

    // load
    QStringList loadedFileNames;
    foreach (QString libPath, libPaths) {

        // prevent duplications if the same file is in multiple dirs, or if some of the searchDirs are parents of others
        QFileInfo libFileInfo(libPath);
        if (loadedFileNames.contains(libFileInfo.fileName())) {
            continue;
        }

        // attempt to load
        QLibrary lib(libPath);
        if (!lib.load()) {
            emit failInfo(lib.errorString());
            continue;
        }

        // prevent duplications (see above)
        loadedFileNames.append(libFileInfo.fileName());

        // attempt to resolve the factory
        WpPluginFactory pluginFactory = (WpPluginFactory) lib.resolve("wp_plugin_factory");
        if (!pluginFactory) {
            emit failInfo(lib.errorString());
            continue;
        }

        // add to factories
        LoadedLib loadedLib;
        loadedLib.fromEasyPluginInstallDir = libPath.startsWith(easyPluginInstallDirPath);
        loadedLib.pluginFactory            = pluginFactory;
        loadedLibs->append(loadedLib);
    }

    emit finished();
}


// recursive search
void PluginLibsLoader::libSearch(QString startDir, QStringList *results)
{
    const QFileInfoList entries = QDir(startDir).entryInfoList();
    foreach (QFileInfo entry, entries) {
        if ((entry.fileName().compare(".") == 0) || (entry.fileName().compare("..") == 0)) {
            continue;
        }

        if (entry.isDir()) {
            libSearch(entry.absoluteFilePath(), results);
        }

        if (entry.isFile() && !entry.isSymLink() && (entry.fileName().count("wp_") == 1) &&
            QLibrary::isLibrary(entry.absoluteFilePath())) {
            results->append(entry.absoluteFilePath());
        }
    }
}
