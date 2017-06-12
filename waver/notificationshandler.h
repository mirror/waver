#ifndef NOTIFICATIONSHANDLER_H
#define NOTIFICATIONSHANDLER_H

#include <QObject>

#include "server.h"

#ifdef Q_OS_LINUX

    #include <QDBusConnection>

    #include "mediaplayer2dbusadaptor.h"
    #include "mediaplayer2playerdbusadaptor.h"

#endif


#ifdef Q_OS_WIN

    #include "trayicon.h"

#endif


#ifdef QT_DEBUG
    #include <QDebug>
#endif


class NotificationsHandler : public QObject
{
    Q_OBJECT

public:

    explicit NotificationsHandler(WaverServer *waverServer);
    ~NotificationsHandler();

};

#endif // NOTIFICATIONSHANDLER_H
