/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef FILESEARCHER_H
#define FILESEARCHER_H

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QMimeDatabase>
#include <QMimeType>
#include <QRegExp>
#include <QString>
#include <QThread>


class FileSearcher : public QThread
{
    Q_OBJECT

    public:

        FileSearcher(QString dir, QString filter, QString uiData);

        void run() override;

        QString getUiData();

        QList<QFileInfo> getResults();


    private:

        QString dir;
        QString uiData;
        QRegExp filter;

        QMimeDatabase mimeDatabase;

        QList<QFileInfo> results;

        void searchDir(QDir dirToSearch);
};

#endif // FILESEARCHER_H
