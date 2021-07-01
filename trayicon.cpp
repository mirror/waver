/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "trayicon.h"

TrayIcon::TrayIcon(Waver *waver, QObject *parent) : QObject(parent)
{
    this->waver = waver;


    std::wstring appName = QGuiApplication::instance()->applicationName().toStdWString();
    const auto   aumi    = WinToast::configureAUMI(QGuiApplication::instance()->organizationName().toStdWString(), appName, L"", QGuiApplication::instance()->applicationVersion().toStdWString());

    WinToast::instance()->setAppName(appName);
    WinToast::instance()->setAppUserModelId(aumi);

    if (WinToast::instance()->initialize()) {
        toastTemplate = WinToastTemplate(WinToastTemplate::ImageAndText02);
        toastTemplate.setImagePath(QString("%1\\waver.png").arg(QGuiApplication::applicationDirPath().append("\\images").replace("/", "\\")).toStdWString());
        toastTemplate.setAudioOption(WinToastTemplate::Silent);
        toastTemplate.setDuration(WinToastTemplate::Short);
        toastTemplate.addAction(tr("Pause").toStdWString());
        toastTemplate.addAction(tr("Play").toStdWString());

        connect(waver, &Waver::notify,   this,  &TrayIcon::showMetadataMessage);
        connect(this,  &TrayIcon::pause, waver, &Waver::pauseButton);
        connect(this,  &TrayIcon::play,  waver, &Waver::playButton);
    }
}


TrayIcon::~TrayIcon()
{
    WinToast::instance()->clear();
}


void TrayIcon::showMetadataMessage()
{
    WinToast::instance()->clear();

    Track::TrackInfo trackInfo = waver->getCurrentTrackInfo();

    toastTemplate.setTextField(trackInfo.title.toStdWString(), WinToastTemplate::FirstLine);
    toastTemplate.setTextField(trackInfo.artist.toStdWString(), WinToastTemplate::SecondLine);

    QTimer::singleShot(250, this, &TrayIcon::showToast);
}


void TrayIcon::sendPause()
{
    emit pause();
}


void TrayIcon::sendPlay()
{
    emit play();
}


void TrayIcon::showToast()
{
    WinToast::instance()->showToast(toastTemplate, new WinToastHandler(this));
}


WinToastHandler::WinToastHandler(TrayIcon *trayIcon)
{
    this->trayIcon = trayIcon;
}


void WinToastHandler::toastActivated() const
{
    toastActivated(0);
}


void WinToastHandler::toastActivated(int actionIndex) const
{
    if (trayIcon == nullptr) {
        return;
    }

    WinToast::instance()->clear();

    switch (actionIndex) {
        case 0:
            trayIcon->sendPause();
            break;
        case 1:
            trayIcon->sendPlay();
            break;
    }

    QTimer::singleShot(250, trayIcon, &TrayIcon::showToast);
}


void WinToastHandler::toastDismissed(WinToastDismissalReason state) const
{
    Q_UNUSED(state);
}


void WinToastHandler::toastFailed() const
{

}
