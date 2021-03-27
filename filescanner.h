/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef FILESCANNER_H
#define FILESCANNER_H

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QMimeDatabase>
#include <QMimeType>
#include <QString>
#include <QThread>


class FileScanner : public QThread
{
    Q_OBJECT

    public:

        FileScanner(QString dir, QString uiData);

        void run() override;

        QString getUiData();

        QList<QFileInfo> getDirs();
        QList<QFileInfo> getFiles();


    private:

        QString dir;
        QString uiData;

        QMimeDatabase mimeDatabase;

        QList<QFileInfo> dirs;
        QList<QFileInfo> files;
};

#endif // FILESCANNER_H
