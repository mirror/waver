/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/

#ifndef WAVERAPPLICATION_H
#define WAVERAPPLICATION_H

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QString>
#include <QSysInfo>
#include <QThread>

#include "globals.h"
#include "notificationshandler.h"
#include "waver.h"


class WaverApplication : public QGuiApplication
{
    Q_OBJECT

    public:

        explicit WaverApplication(int &argc, char **argv);
        ~WaverApplication();

        void setQmlApplicationEngine(QQmlApplicationEngine *qmlApplicationEngine);


    private:

        const int VERSION_MAJOR = 2;
        const int VERSION_MINOR = 1;
        const int VERSION_SUB   = 5;

        QQmlApplicationEngine *qmlApplicationEngine;

        QThread *waverThread;
        Waver   *waver;

        NotificationsHandler *notificationsHandler;


    public slots:

        void uiSaveGeometry(int x, int y, int width, int height);


    signals:

        void shutdownWaver();

};

#endif // WAVERAPPLICATION_H
