/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "filesearcher.h"
#include "qdebug.h"


FileSearcher::FileSearcher(QString dir, QString filter, QString uiData) : QThread()
{
    this->dir    = dir;
    this->uiData = uiData;

    filter.replace(QRegExp("\\s+"), "[\\s_-]+");
    this->filter.setPattern(filter);
    this->filter.setCaseSensitivity(Qt::CaseInsensitive);
}


void FileSearcher::run()
{
    searchDir(QDir(dir));
}


QString FileSearcher::getUiData()
{
    return uiData;
}


QList<QFileInfo> FileSearcher::getResults()
{
    std::sort(results.begin(), results.end(), [](QFileInfo a, QFileInfo b) {
        return a.baseName().compare(b.baseName(), Qt::CaseInsensitive) < 0;
    });
    return results;
}


void FileSearcher::searchDir(QDir dirToSearch)
{
    if (dirToSearch.exists()) {
        const QFileInfoList entries = dirToSearch.entryInfoList();
        foreach (QFileInfo entry, entries) {
            if (isInterruptionRequested()) {
                break;
            }

            if ((entry.fileName().compare(".") == 0) || (entry.fileName().compare("..") == 0)) {
                continue;
            }

            if (entry.isDir()) {
                searchDir(QDir(entry.absoluteFilePath()));
                continue;
            }

            if (entry.isFile() && !entry.isSymLink() && (entry.fileName().endsWith(".mp3", Qt::CaseInsensitive) || mimeDatabase.mimeTypeForFile(entry).name().startsWith("audio", Qt::CaseInsensitive)) && entry.fileName().contains(filter)) {
                results.append(entry);
            }
        }
    }
}
