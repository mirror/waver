/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef NOTIFICATIONSHANDLER_H
#define NOTIFICATIONSHANDLER_H

#include <QObject>

#include "globals.h"
#include "waver.h"

#ifdef Q_OS_WIN
    #include "trayicon.h"
#elif defined (Q_OS_LINUX)
    #include <QDBusConnection>
    #include "mediaplayer2dbusadaptor.h"
    #include "mediaplayer2playerdbusadaptor.h"
#endif

#ifdef QT_DEBUG
    #include <QDebug>
#endif


class NotificationsHandler : public QObject {
        Q_OBJECT

    public:

        explicit NotificationsHandler(Waver *waver);
        ~NotificationsHandler();
};

#endif // NOTIFICATIONSHANDLER_H
