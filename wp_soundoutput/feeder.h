/*
    This file is part of Waver

    Copyright (C) 2017-2020 Peter Papp <peter.papp.p@gmail.com>

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


#ifndef FEEDER_H
#define FEEDER_H

#include <QAudioFormat>
#include <QByteArray>
#include <QIODevice>
#include <QMutex>
#include <QObject>
#include <QThread>


class Feeder : public QObject {
        Q_OBJECT

    public:

        explicit Feeder(QByteArray *outputBuffer, QMutex *outputBufferMutex, QAudioFormat audioFormat, int minWriteBytes);

        void setOutputDevice(QIODevice *outputDevice);


    private:

        int minWriteBytes;

        QByteArray *outputBuffer;
        QMutex     *outputBufferMutex;
        QIODevice  *outputDevice;

        QAudioFormat audioFormat;

        QMutex outputDeviceMutex;


    public slots:

        void run();

};

#endif // FEEDER_H
