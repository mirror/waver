/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


/*
    historically this used to use QTrayIcon, but that requires Qt's Widgets
    now using more modern Windows toast notifications
    see https://github.com/mohabouje/WinToast
*/


#ifndef TRAYICON_H
#define TRAYICON_H

#include <QGuiApplication>
#include <QList>
#include <QObject>
#include <QTimer>
#include <wintoastlib.h>

#include "globals.h"
#include "track.h"
#include "waver.h"

using namespace WinToastLib;


class TrayIcon;


class WinToastHandler : public IWinToastHandler {

    public:

        WinToastHandler(TrayIcon *trayIcon = nullptr);

        void toastActivated() const override;
        void toastActivated(int actionIndex) const override;
        void toastDismissed(WinToastDismissalReason state) const override;
        void toastFailed() const override;


    private:

        TrayIcon *trayIcon;
};


class TrayIcon : public QObject {
        Q_OBJECT

    public:

        explicit TrayIcon(Waver *waver, QObject *parent);
        ~TrayIcon();

        void sendPause();
        void sendPlay();
        void sendPlayPause();


    private:

        Waver    *waver;
        WinToast  winToast;

        void showMetadataMessage();

        WinToastTemplate toastTemplate;


    public slots:

        void showToast();


    signals:

        void pause() const;
        void play() const;
};



#endif // TRAYICON_H
