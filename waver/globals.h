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


#ifndef GLOBALS_H
#define GLOBALS_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QTextStream>
#include <QUuid>
#include <QVector>

const int THREAD_TIMEOUT = 500;

class Globals : public QObject {
        Q_OBJECT

    public:

        static QString appName();
        static QString appVersion();
        static QString appDesc();

        static QString author();
        static QString email();
        static QString website();
        static QString copyright();
        static QString license();

        static QString userAgent();

        static void consoleOutput(QString text, bool error);

};

#endif // GLOBALS_H
