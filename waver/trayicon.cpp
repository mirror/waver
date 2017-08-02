#include "trayicon.h"

TrayIcon::TrayIcon(QObject *parent, WaverServer *waverServer) : QObject(parent)
{
    this->waverServer = waverServer;

    firstTrack      = true;
    playPauseAction = NULL;
    systemTrayIcon  = NULL;
    systemTrayMenu  = NULL;
    ipcMessageUtils = NULL;

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    ipcMessageUtils = new IpcMessageUtils();

    playPauseAction = new QAction(QIcon(":/images/resume.png"), "Play/Pause");
    connect(playPauseAction, SIGNAL(triggered(bool)), this, SLOT(menuPlayPause()));

    systemTrayMenu = new QMenu("Waver");
    systemTrayMenu->addAction(playPauseAction);
    systemTrayMenu->addAction(QIcon(":/images/next.png"),   "Next",       this, SLOT(menuNext()));
    systemTrayMenu->addAction("Show Waver", this, SLOT(menuShowWaver()));

    systemTrayIcon = new QSystemTrayIcon(QIcon(":/images/waver.png"));
    systemTrayIcon->setToolTip("Waver");
    systemTrayIcon->setContextMenu(systemTrayMenu);
    systemTrayIcon->show();

    connect(systemTrayIcon,    SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this,
        SLOT(activated(QSystemTrayIcon::ActivationReason)));
    connect(this->waverServer, SIGNAL(ipcSend(QString)),                             this, SLOT(waverServerIpcSend(QString)));
}


TrayIcon::~TrayIcon()
{
    if (systemTrayIcon != NULL) {
        delete systemTrayIcon;
    }
    if (systemTrayMenu != NULL) {
        delete systemTrayMenu;
    }
    if (ipcMessageUtils != NULL) {
        delete ipcMessageUtils;
    }
    if (playPauseAction != NULL) {
        delete playPauseAction;
    }
}


void TrayIcon::menuPlayPause()
{
    waverServer->notificationsHelper_PlayPause();
}


void TrayIcon::menuNext()
{
    waverServer->notificationsHelper_Next();
}


void TrayIcon::menuShowWaver()
{
    waverServer->notificationsHelper_Raise();
}


void TrayIcon::activated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        showMetadataMessage();
    }
}


void TrayIcon::waverServerIpcSend(QString data)
{
    this->ipcMessageUtils->processIpcString(data);

    for (int i = 0; i < this->ipcMessageUtils->processedCount(); i++) {
        switch (ipcMessageUtils->processedIpcMessage(i)) {
            case IpcMessageUtils::Pause:
                playPauseAction->setIcon(QIcon(":/images/resume.png"));
                playPauseAction->setText("Resume");
                break;
            case IpcMessageUtils::Resume:
                playPauseAction->setIcon(QIcon(":/images/pause.png"));
                playPauseAction->setText("Pause");
                break;
            case IpcMessageUtils::TrackInfo:
                if (firstTrack) {
                    firstTrack = false;
                    playPauseAction->setIcon(QIcon(":/images/pause.png"));
                    playPauseAction->setText("Pause");
                }
                break;
            default:
                break;
        }
    }
}


void TrayIcon::showMetadataMessage()
{
    if (!QSystemTrayIcon::supportsMessages()) {
        return;
    }

    PluginSource::TrackInfo trackInfo = waverServer->notificationsHelper_Metadata();
    systemTrayIcon->showMessage(trackInfo.title, trackInfo.performer, QSystemTrayIcon::NoIcon, 4000);
}
