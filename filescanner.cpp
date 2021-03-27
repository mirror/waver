/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "filescanner.h"


FileScanner::FileScanner(QString dir, QString uiData) : QThread()
{
    this->dir    = dir;
    this->uiData = uiData;
}


void FileScanner::run()
{
    QDir dir(this->dir);

    if (dir.exists()) {
        const QFileInfoList entries = dir.entryInfoList();
        foreach (QFileInfo entry, entries) {
            if (isInterruptionRequested()) {
                break;
            }

            if ((entry.fileName().compare(".") == 0) || (entry.fileName().compare("..") == 0)) {
                continue;
            }

            if (entry.isDir()) {
                dirs.append(entry);
                continue;
            }

            if (entry.isFile() && !entry.isSymLink() && (entry.fileName().endsWith(".mp3", Qt::CaseInsensitive) || mimeDatabase.mimeTypeForFile(entry).name().startsWith("audio", Qt::CaseInsensitive))) {
                files.append(entry);
            }
        }

        if (!isInterruptionRequested()) {
            std::sort(dirs.begin(), dirs.end(), [](QFileInfo a, QFileInfo b) {
                return a.baseName().compare(b.baseName(), Qt::CaseInsensitive) < 0;
            });
        }
        if (!isInterruptionRequested()) {
            std::sort(files.begin(), files.end(), [](QFileInfo a, QFileInfo b) {
                return a.baseName().compare(b.baseName(), Qt::CaseInsensitive) < 0;
            });
        }
    }
}


QString FileScanner::getUiData()
{
    return uiData;
}


QList<QFileInfo> FileScanner::getDirs()
{
    return dirs;
}


QList<QFileInfo> FileScanner::getFiles()
{
    return files;
}
