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
#include <QObject>
#include <QTimer>
#include <wintoastlib.h>

#include "globals.h"
#include "track.h"
#include "waver.h"

using namespace WinToastLib;


class TrayIcon : public QObject, IWinToastHandler {
        Q_OBJECT

    public:

        explicit TrayIcon(Waver *waver, QObject *parent);
        ~TrayIcon();

        void toastActivated() const override;
        void toastActivated(int actionIndex) const override;
        void toastDismissed(WinToastDismissalReason state) const override;
        void toastFailed() const override;


    private:

        Waver *waver;

        void showMetadataMessage();

        QString          imagesPath;
        WinToastTemplate toastTemplate;


    private slots:

        void showToast();


    signals:

        void pause() const;
        void play() const;
};

#endif // TRAYICON_H
