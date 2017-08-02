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


#include "filescanner.h"

// constructor
FileScanner::FileScanner(QObject *parent, QString startDir, QStringList *results, QStringList *exclude,
    QMutex *mutex) : QThread(parent)
{
    this->startDir = startDir;
    this->results  = results;
    this->exclude  = exclude;
    this->mutex    = mutex;
}


// point of entry for thread
void FileScanner::run()
{
    QDir dir(startDir);

    if (dir.exists()) {
        scan(startDir);
    }
}


// recursive search
void FileScanner::scan(QString startDir)
{
    if (isInterruptionRequested()) {
        return;
    }

    const QFileInfoList entries = QDir(startDir).entryInfoList();
    foreach (QFileInfo entry, entries) {
        if ((entry.fileName().compare(".") == 0) || (entry.fileName().compare("..") == 0)) {
            continue;
        }

        if (entry.isDir()) {
            scan(entry.absoluteFilePath());
        }


        if (entry.isFile() && !entry.isSymLink() && (entry.fileName().endsWith(".mp3", Qt::CaseInsensitive) ||
                mimeDatabase.mimeTypeForFile(entry).name().startsWith("audio", Qt::CaseInsensitive))) {
            mutex->lock();
            if (!exclude->contains(entry.absoluteFilePath())) {
                results->append(entry.absoluteFilePath());
                if (results->count() == 1) {
                    emit foundFirst();
                }
            }
            mutex->unlock();
        }
    }
}
