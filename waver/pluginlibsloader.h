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


#ifndef PLUGINLIBSLOADER_H
#define PLUGINLIBSLOADER_H

#include <QtGlobal>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QLibrary>
#include <QObject>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QVector>

#include "pluginbase.h"


class PluginLibsLoader : public QObject
{
    Q_OBJECT

public:

    struct LoadedLib {
        bool            fromEasyPluginInstallDir;
        WpPluginFactory pluginFactory;
    };
    typedef QVector<LoadedLib> LoadedLibs;

    explicit PluginLibsLoader(QObject *parent = 0, LoadedLibs *loadedLibs = NULL);


private:

    LoadedLibs *loadedLibs;

    void libSearch(QString startDir, QStringList *results);


signals:

    void finished();
    void failInfo(QString info);


public slots:

    void run();


};

#endif // PLUGINLIBSLOADER_H
