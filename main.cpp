/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QString>
#include <QUrl>

#ifdef QT_DEBUG
    #include <QDebug>
#endif

#include "waverapplication.h"


int main(int argc, char *argv[])
{
    const QUrl applicationWindowUrl(QStringLiteral("qrc:/qml/Main.qml"));

    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    #endif
    WaverApplication app(argc, argv);

    QQmlApplicationEngine engine;

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [applicationWindowUrl](QObject *obj, const QUrl &objUrl) {
        if ((objUrl == applicationWindowUrl) && !obj) {
            QCoreApplication::exit(1);
            return;
        }
    }, Qt::QueuedConnection);

    engine.load(applicationWindowUrl);

    app.setQmlApplicationEngine(&engine);
    return app.exec();
}
