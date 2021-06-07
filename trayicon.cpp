/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include "trayicon.h"

TrayIcon::TrayIcon(Waver *waver, QObject *parent) : QObject(parent)
{
    this->waver = waver;

    imagesPath = QGuiApplication::applicationDirPath().append("\\images").replace("/", "\\");

    std::wstring appName = QGuiApplication::instance()->applicationName().toStdWString();
    const auto   aumi    = WinToast::configureAUMI(QGuiApplication::instance()->organizationName().toStdWString().c_str(), appName.c_str(), L"", QGuiApplication::instance()->applicationVersion().toStdWString().c_str());

    WinToast::instance()->setAppName(appName.c_str());
    WinToast::instance()->setAppUserModelId(aumi);

    if (WinToast::instance()->initialize()) {
        toastTemplate = WinToastTemplate(WinToastTemplate::ImageAndText02);
        toastTemplate.setTextField(appName.c_str(), WinToastTemplate::FirstLine);
        toastTemplate.setTextField(L"Idle", WinToastTemplate::SecondLine);
        toastTemplate.setImagePath(QString("%1\\waver.png").arg(imagesPath).toStdWString().c_str());
        toastTemplate.setAudioOption(WinToastTemplate::Silent);
        toastTemplate.addAction(tr("Pause").toStdWString().c_str());
        toastTemplate.addAction(tr("Play").toStdWString().c_str());

        WinToast::instance()->showToast(toastTemplate, this);

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
    Track::TrackInfo trackInfo = waver->getCurrentTrackInfo();

    WinToast::instance()->clear();

    toastTemplate.setTextField(trackInfo.title.toStdWString().c_str(), WinToastTemplate::FirstLine);
    toastTemplate.setTextField(trackInfo.artist.toStdWString().c_str(), WinToastTemplate::SecondLine);

    QTimer::singleShot(250, this, &TrayIcon::showToast);
}


void TrayIcon::showToast()
{
    WinToast::instance()->showToast(toastTemplate, this);
}


void TrayIcon::toastActivated() const
{
    toastActivated(0);

}


void TrayIcon::toastActivated(int actionIndex) const
{
    WinToast::instance()->clear();

    switch (actionIndex) {
        case 0:
            emit pause();
            break;
        case 1:
            emit play();
            break;
    }

    QTimer::singleShot(250, this, &TrayIcon::showToast);
}


void TrayIcon::toastDismissed(WinToastDismissalReason state) const
{
    Q_UNUSED(state);
}


void TrayIcon::toastFailed() const
{

}
