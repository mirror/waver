#ifndef NOTIFICATIONSHANDLER_H
#define NOTIFICATIONSHANDLER_H

#include <QObject>

#ifdef Q_OS_WIN

#include "trayicon.h"

#elif defined (Q_OS_LINUX)

#include <QDBusConnection>

#include "mediaplayer2dbusadaptor.h"
#include "mediaplayer2playerdbusadaptor.h"

#endif

#include "globals.h"
#include "waver.h"

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
