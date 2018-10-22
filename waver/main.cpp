/*
    This file is part of Waver

    Copyright (C) 2017 Peter Papp <peter.papp.p@gmail.com>

    Please visit https://launchpad.net/waver for details

    Waver is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Waver is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    (GPL.TXT) along with Waver. If not, see <http://www.gnu.org/licenses/>.

*/


#include <QtGlobal>

#include <QByteArray>
#include <QCommandLineParser>
#include <QFont>
#include <QObject>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QString>
#include <QStringList>
#include <QTcpSocket>

#ifdef Q_OS_WIN
    #include <QApplication>
#else
    #include <QCoreApplication>
#endif

#include "globals.h"
#include "ipcmessageutils.h"
#include "notificationshandler.h"
#include "server.h"
#include "waverapplication.h"


// helper to set up command line parser
void initializeCommandLineParser(QCommandLineParser *commandLineParser)
{
    commandLineParser->setApplicationDescription(Globals::appDesc());
    commandLineParser->addPositionalArgument("additional_arguments", "Track URLs and/or control commands");
    commandLineParser->addOptions({
        { {"s", "server" }, "Starts the server instead of the user interface" },
    });
    commandLineParser->addHelpOption();
    commandLineParser->addVersionOption();
}


// helper to send data through TCP
void sendData(QStringList data)
{
    if (data.size() < 1) {
        return;
    }

    QTcpSocket tcpSocket;
    tcpSocket.connectToHost("127.0.0.1", IpcMessageUtils::tcpPort());
    if (tcpSocket.waitForConnected(TCP_TIMEOUT)) {
        tcpSocket.write(data.join(MESSAGE_SEPARATOR).append(MESSAGE_SEPARATOR).toUtf8().data());
        tcpSocket.waitForBytesWritten(TCP_TIMEOUT);
        tcpSocket.disconnectFromHost();
    }
}


// main program
int main(int argc, char *argv[])
{
    IpcMessageUtils ipcMessageUtils;

    // look for --server argument (can't use command line parser here yet because that requires QCoreApplication)
    bool serverRequested = false;
    for (int i = 1; i < argc; ++i) {
        if ((qstrcmp(argv[i], "--server") == 0) || (qstrcmp(argv[i], "-s") == 0)) {
            serverRequested = true;
            break;
        }
    }

    // check if server is already runnig
    bool serverRunning = false;
    QTcpSocket tcpSocket;
    tcpSocket.connectToHost("127.0.0.1", IpcMessageUtils::tcpPort());
    if (tcpSocket.waitForConnected(TCP_TIMEOUT)) {
        tcpSocket.write(ipcMessageUtils.constructIpcString(IpcMessageUtils::AreYouAlive).toUtf8());
        if (tcpSocket.waitForBytesWritten(TCP_TIMEOUT)) {
            if (tcpSocket.waitForReadyRead(TCP_TIMEOUT)) {
                QByteArray byteArray(64, 0);
                if (tcpSocket.read(byteArray.data(), byteArray.size()) > 0) {
                    ipcMessageUtils.processIpcString(QString(byteArray.data()));
                    if ((ipcMessageUtils.processedCount() > 0) && (ipcMessageUtils.processedIpcMessage(0) == IpcMessageUtils::ImAlive)) {
                        serverRunning = true;
                    }
                }
            }
        }
        tcpSocket.disconnectFromHost();
    }

    // server requested
    if (serverRequested) {
        // create application object
        #ifdef Q_OS_WIN
        // on Windows, GUI and Widgets stuff is needed for the system tray icon
        QApplication coreApplication(argc, argv);
        #else
        // keep it light on other platforms
        QCoreApplication coreApplication(argc, argv);
        #endif
        coreApplication.setApplicationName(Globals::appName());
        coreApplication.setApplicationVersion(Globals::appVersion());

        // parse command line (this also automatically handles help and errors)
        QCommandLineParser commandLineParser;
        initializeCommandLineParser(&commandLineParser);
        commandLineParser.process(coreApplication);
        QStringList additionalArguments = commandLineParser.positionalArguments();

        // if server already running, just send arguments and then it's done
        if (serverRunning) {
            Globals::consoleOutput("Server already running", false);
            sendData(additionalArguments);
            return 0;
        }

        // feedback
        Globals::consoleOutput("Server mode", false);

        // create the server, pass arguments to it
        WaverServer *waverServer = new WaverServer(NULL, additionalArguments);
        QObject::connect(waverServer, SIGNAL(finished()), &coreApplication, SLOT(quit()));

        // start the notifications handler
        NotificationsHandler *notificationsHandler = new NotificationsHandler(waverServer);

        // start the server
        QMetaObject::invokeMethod(waverServer, "run", Qt::QueuedConnection);
        int returnValue = coreApplication.exec();

        // housekeeping
        delete notificationsHandler;

        // it's all done now
        return returnValue;
    }

    // application
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    WaverApplication application(argc, argv);

    application.setApplicationName(Globals::appName());
    application.setApplicationVersion(Globals::appVersion());

    // set font to be consistent through OS versions
    QFont appFont;
    appFont.setFamily("SansSerif");
    appFont.setStyleHint(QFont::SansSerif);
    appFont.setPointSize(10);
    application.setFont(appFont);

    // parse command line (this also automatically handles help and errors) (QGuiApplication inherits from QCoreApplication)
    QCommandLineParser commandLineParser;
    initializeCommandLineParser(&commandLineParser);
    commandLineParser.process(application);
    QStringList additionalArguments = commandLineParser.positionalArguments();

    // if server is already running and arguments are received then just send the arguments and be done
    if (serverRunning && (additionalArguments.size() > 0)) {
        sendData(additionalArguments);
        return 0;
    }

    #ifndef Q_OS_ANDROID

    // start server in a separate process if not yet running (on Android, it is started by the activity, see WaverActivtity.java)
    if (!serverRunning) {
        additionalArguments.prepend("--server");
        QProcess::startDetached(QCoreApplication::applicationFilePath(), additionalArguments);
    }

    #endif

    // start user interface
    QQmlApplicationEngine engine;
    engine.load(QUrl(QLatin1String("qrc:/qml/MainWindow.qml")));
    application.setQmlApplicationEngine(&engine);
    return application.exec();
}
