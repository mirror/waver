/*
    This file is part of Waver

    Copyright (C) 2017-2019 Peter Papp <peter.papp.p@gmail.com>

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


#include "feeder.h"

// constructor
Feeder::Feeder(QByteArray *outputBuffer, QMutex *outputBufferMutex, QAudioFormat audioFormat, int minWriteBytes) : QObject(NULL)
{
    this->outputBuffer      = outputBuffer;
    this->outputBufferMutex = outputBufferMutex;
    this->audioFormat       = audioFormat;
    this->minWriteBytes     = minWriteBytes;

    outputDevice  = NULL;
}


// set output device - this meant to be ran from another thread!
void Feeder::setOutputDevice(QIODevice *outputDevice)
{
    outputDeviceMutex.lock();
    this->outputDevice = outputDevice;
    outputDeviceMutex.unlock();
}


// thread entry point
void Feeder::run()
{
    int bytesWritten;
    while (!QThread::currentThread()->isInterruptionRequested()) {
        bytesWritten = 0;

        if (outputDevice == NULL) {
            QThread::currentThread()->msleep(100);
        }

        outputDeviceMutex.lock();
        if ((outputDevice != NULL) && (outputBuffer->count() >= minWriteBytes)) {
            bytesWritten = outputDevice->write(outputBuffer->data(), outputBuffer->count());

            outputBufferMutex->lock();
            outputBuffer->remove(0, bytesWritten);
            outputBufferMutex->unlock();
        }
        outputDeviceMutex.unlock();

        if (!QThread::currentThread()->isInterruptionRequested()) {
            int sleepUSec = 50 * 1000;
            if (bytesWritten > 0) {
                sleepUSec = audioFormat.durationForBytes(bytesWritten) / 4 * 3;
            }

            QThread::currentThread()->usleep(sleepUSec);
        }
    }
}
