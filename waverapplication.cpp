/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/

#include "waverapplication.h"

WaverApplication::WaverApplication(int &argc, char **argv) : QGuiApplication(argc, argv)
{
    qRegisterMetaType<TimedChunk>("TimedChunk");

    waverThread = nullptr;
    waver       = nullptr;

    setOrganizationName("4phun");
    setOrganizationDomain("pppphun.com");
    setApplicationName("Waver");
    setApplicationVersion(VERSION_SUB == 0 ? QString("%1.%2").arg(VERSION_MAJOR).arg(VERSION_MINOR) : QString("%1.%2.%3").arg(VERSION_MAJOR).arg(VERSION_MINOR).arg(VERSION_SUB));
    setWindowIcon(QIcon("qrc:/images/waver.png"));

    if ((QSysInfo::productType().compare("windows", Qt::CaseInsensitive) == 0) || (QSysInfo::productType().compare("winrt", Qt::CaseInsensitive) == 0)) {
        QQuickStyle::setStyle("Universal");
    }
    else {
        QQuickStyle::setStyle("Material");
    }

    notificationsHandler = nullptr;
}


WaverApplication::~WaverApplication()
{
    if (notificationsHandler != nullptr) {
        delete notificationsHandler;
    }

    emit shutdownWaver();
    while (!waver->isShutdownCompleted()) {
        QThread::currentThread()->msleep(50);
    }

    if (waverThread != nullptr) {
        waverThread->quit();
        waverThread->wait();
        waverThread->deleteLater();
    }
}


void WaverApplication::setQmlApplicationEngine(QQmlApplicationEngine *qmlApplicationEngine)
{
    this->qmlApplicationEngine = qmlApplicationEngine;

    waverThread = new QThread();
    waver       = new Waver();

    waverThread->setObjectName("waverThread");
    waver->moveToThread(waverThread);

    QObject::connect(waverThread, &QThread::started, waver, &Waver::run);
    QObject::connect(waverThread, &QThread::finished, waver, &Waver::deleteLater);

    QQuickWindow *uiMainWindow = qobject_cast<QQuickWindow *>(this->qmlApplicationEngine->rootObjects().at(0));

    QObject::connect(waver, SIGNAL(explorerAddItem(QVariant,QVariant,QVariant,QVariant,QVariant,QVariant,QVariant,QVariant,QVariant)), uiMainWindow, SLOT(explorerAddItem(QVariant,QVariant,QVariant,QVariant,QVariant,QVariant,QVariant,QVariant,QVariant)));
    QObject::connect(waver, SIGNAL(explorerDisableQueueable(QVariant)), uiMainWindow, SLOT(explorerDisableQueueable(QVariant)));
    QObject::connect(waver, SIGNAL(explorerRemoveAboveLevel(QVariant)), uiMainWindow, SLOT(explorerRemoveAboveLevel(QVariant)));
    QObject::connect(waver, SIGNAL(explorerRemoveChildren(QVariant)), uiMainWindow, SLOT(explorerRemoveChildren(QVariant)));
    QObject::connect(waver, SIGNAL(explorerRemoveItem(QVariant)), uiMainWindow, SLOT(explorerRemoveItem(QVariant)));
    QObject::connect(waver, SIGNAL(explorerSetBusy(QVariant,QVariant)), uiMainWindow, SLOT(explorerSetBusy(QVariant,QVariant)));
    QObject::connect(waver, SIGNAL(explorerSetFlagExtra(QVariant,QVariant)), uiMainWindow, SLOT(explorerSetFlagExtra(QVariant,QVariant)));
    QObject::connect(waver, SIGNAL(explorerSetError(QVariant,QVariant,QVariant)), uiMainWindow, SLOT(explorerSetError(QVariant,QVariant,QVariant)));
    QObject::connect(waver, SIGNAL(explorerSetSelected(QVariant,QVariant)), uiMainWindow, SLOT(explorerSetSelected(QVariant,QVariant)));
    QObject::connect(waver, SIGNAL(explorerToggleSelected(QVariant)), uiMainWindow, SLOT(explorerToggleSelected(QVariant)));

    QObject::connect(waver, SIGNAL(uiSetImage(QVariant)), uiMainWindow, SLOT(setImage(QVariant)));
    QObject::connect(waver, SIGNAL(uiSetTempImage(QVariant)), uiMainWindow, SLOT(setTempImage(QVariant)));
    QObject::connect(waver, SIGNAL(uiSetStatusText(QVariant)), uiMainWindow, SLOT(setStatusText(QVariant)));
    QObject::connect(waver, SIGNAL(uiSetStatusTempText(QVariant)), uiMainWindow, SLOT(setStatusTempText(QVariant)));
    QObject::connect(waver, SIGNAL(uiSetPeakMeter(QVariant,QVariant,QVariant)), uiMainWindow, SLOT(setPeakMeter(QVariant,QVariant,QVariant)));
    QObject::connect(waver, SIGNAL(uiSetPeakMeterReplayGain(QVariant)), uiMainWindow, SLOT(setPeakMeterReplayGain(QVariant)));
    QObject::connect(waver, SIGNAL(uiSetPeakFPS(QVariant)), uiMainWindow, SLOT(setPeakFPS(QVariant)));
    QObject::connect(waver, SIGNAL(uiSetShuffleCountdown(QVariant)), uiMainWindow, SLOT(setShuffleCountdown(QVariant)));
    QObject::connect(waver, SIGNAL(uiSetFavorite(QVariant)), uiMainWindow, SLOT(setFavorite(QVariant)));

    QObject::connect(waver, SIGNAL(uiSetTrackBusy(QVariant)), uiMainWindow, SLOT(setTrackBusy(QVariant)));
    QObject::connect(waver, SIGNAL(uiSetTrackData(QVariant,QVariant,QVariant,QVariant,QVariant)), uiMainWindow, SLOT(setTrackData(QVariant,QVariant,QVariant,QVariant,QVariant)));
    QObject::connect(waver, SIGNAL(uiSetTrackLength(QVariant)), uiMainWindow, SLOT(setTrackLength(QVariant)));
    QObject::connect(waver, SIGNAL(uiSetTrackPosition(QVariant,QVariant)), uiMainWindow, SLOT(setTrackPosition(QVariant,QVariant)));
    QObject::connect(waver, SIGNAL(uiSetTrackDecoding(QVariant,QVariant)), uiMainWindow, SLOT(setTrackDecoding(QVariant,QVariant)));
    QObject::connect(waver, SIGNAL(uiSetTrackTags(QVariant)), uiMainWindow, SLOT(setTrackTags(QVariant)));

    QObject::connect(waver, SIGNAL(playlistAddItem(QVariant,QVariant,QVariant,QVariant,QVariant)), uiMainWindow, SLOT(playlistAddItem(QVariant,QVariant,QVariant,QVariant,QVariant)));
    QObject::connect(waver, SIGNAL(playlistClearItems()), uiMainWindow, SLOT(playlistClearItems()));
    QObject::connect(waver, SIGNAL(playlistBusy(QVariant,QVariant)), uiMainWindow, SLOT(playlistBusy(QVariant,QVariant)));
    QObject::connect(waver, SIGNAL(playlistDecoding(QVariant,QVariant,QVariant)), uiMainWindow, SLOT(playlistDecoding(QVariant,QVariant,QVariant)));
    QObject::connect(waver, SIGNAL(playlistBigBusy(QVariant)), uiMainWindow, SLOT(playlistBigBusy(QVariant)));
    QObject::connect(waver, SIGNAL(playlistTotalTime(QVariant)), uiMainWindow, SLOT(playlistTotalTime(QVariant)));
    QObject::connect(waver, SIGNAL(playlistSelected(QVariant,QVariant)), uiMainWindow, SLOT(playlistSelected(QVariant,QVariant)));

    QObject::connect(waver,        SIGNAL(uiPromptServerPsw(QVariant,QVariant)),  uiMainWindow, SLOT(promptServerPsw(QVariant,QVariant)));
    QObject::connect(uiMainWindow, SIGNAL(addNewServer(QString,QString,QString)), waver,        SLOT(addServer(QString,QString,QString)));
    QObject::connect(uiMainWindow, SIGNAL(deleteServer(QString)),                 waver,        SLOT(deleteServer(QString)));
    QObject::connect(uiMainWindow, SIGNAL(setServerPassword(QString,QString)),    waver,        SLOT(setServerPassword(QString,QString)));

    QObject::connect(uiMainWindow, SIGNAL(explorerItemClicked(QString,int,QString)), waver, SLOT(explorerItemClicked(QString,int,QString)));
    QObject::connect(uiMainWindow, SIGNAL(playlistItemClicked(int,int)), waver, SLOT(playlistItemClicked(int,int)));
    QObject::connect(uiMainWindow, SIGNAL(playlistItemDragDropped(int,int)), waver, SLOT(playlistItemDragDropped(int,int)));
    QObject::connect(uiMainWindow, SIGNAL(positioned(double)), waver, SLOT(positioned(double)));

    QObject::connect(uiMainWindow, SIGNAL(previousButton(int)), waver, SLOT(previousButton(int)));
    QObject::connect(uiMainWindow, SIGNAL(nextButton()), waver, SLOT(nextButton()));
    QObject::connect(uiMainWindow, SIGNAL(playButton()), waver, SLOT(playButton()));
    QObject::connect(uiMainWindow, SIGNAL(pauseButton()), waver, SLOT(pauseButton()));
    QObject::connect(uiMainWindow, SIGNAL(stopButton()), waver, SLOT(stopButton()));
    QObject::connect(uiMainWindow, SIGNAL(favoriteButton(bool)), waver, SLOT(favoriteButton(bool)));

    QObject::connect(waver,        SIGNAL(optionsAsRequested(QVariant)), uiMainWindow, SLOT(optionsAsRequested(QVariant)));
    QObject::connect(uiMainWindow, SIGNAL(requestOptions()),             waver,        SLOT(requestOptions()));
    QObject::connect(uiMainWindow, SIGNAL(updatedOptions(QString)),      waver,        SLOT(updatedOptions(QString)));

    QObject::connect(waver,        SIGNAL(uiHistoryAdd(QVariant)),        uiMainWindow, SLOT(historyAdd(QVariant)));
    QObject::connect(waver,        SIGNAL(uiRaise()),                     uiMainWindow, SLOT(bringToFront()));
    QObject::connect(waver,        SIGNAL(uiSetIsSnap(QVariant)),         uiMainWindow, SLOT(quickStartGuideSetIsSnap(QVariant)));
    QObject::connect(uiMainWindow, SIGNAL(peakUILag()),                   waver,        SLOT(peakUILag()));
    QObject::connect(uiMainWindow, SIGNAL(saveGeometry(int,int,int,int)), this,         SLOT(uiSaveGeometry(int,int,int,int)));
    QObject::connect(this,         SIGNAL(shutdownWaver()),               waver,        SLOT(shutdown()));

    QSettings settings;
    uiMainWindow->setGeometry(settings.value("MainWindow/x", 150).toInt(), settings.value("MainWindow/y", 150).toInt(), settings.value("MainWindow/width", uiMainWindow->width()).toInt(), settings.value("MainWindow/height", uiMainWindow->height()).toInt());

    notificationsHandler = new NotificationsHandler(waver);

    waverThread->start();
}


void WaverApplication::uiSaveGeometry(int x, int y, int width, int height)
{
    QSettings settings;

    settings.setValue("MainWindow/x", x);
    settings.setValue("MainWindow/y", y);
    settings.setValue("MainWindow/width", width);
    settings.setValue("MainWindow/height", height);

    settings.sync();
}
