#ifndef TRAYICON_H
#define TRAYICON_H

#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

#include "ipcmessageutils.h"
#include "pluginsource.h"
#include "server.h"


class TrayIcon : public QObject {
        Q_OBJECT

    public:

        explicit TrayIcon(QObject *parent, WaverServer *waverServer);
        ~TrayIcon();


    private:

        WaverServer     *waverServer;
        IpcMessageUtils *ipcMessageUtils;
        QAction         *playPauseAction;
        QMenu           *systemTrayMenu;
        QSystemTrayIcon *systemTrayIcon;

        bool firstTrack;

        void showMetadataMessage();


    public slots:

        void menuPlayPause();
        void menuNext();
        void menuShowWaver();

        void activated(QSystemTrayIcon::ActivationReason reason);

        void waverServerIpcSend(QString data);
};

#endif // TRAYICON_H
