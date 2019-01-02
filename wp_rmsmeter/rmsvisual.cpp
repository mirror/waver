/*
    This file is part of Waver

    Copyright (C) 2018 Peter Papp <peter.papp.p@gmail.com>

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

#include "rmsvisual.h"

// constructor
RMSVisual::RMSVisual(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    audioChannel  = -1;

    sharedMemory = new QSharedMemory(RMSMeter::SHAREDMEMORY_KEY);

    sharedMemory->attach();

    connect(&timer, SIGNAL(timeout()), this, SLOT(timerTimeout()));

    timer.setInterval(1000 / 24);
    timer.setSingleShot(false);
    timer.setTimerType(Qt::PreciseTimer);
    timer.start();
}


// destructor
RMSVisual::~RMSVisual()
{
    timer.stop();
    sharedMemory->detach();
    delete sharedMemory;
}


// property get
int  RMSVisual::channel() const
{
    return audioChannel;
}


// property set
void RMSVisual::setChannel(const int channel)
{
    if (channel != audioChannel) {
        audioChannel = channel;

        emit channelChanged();
    }
}


// paint it
void RMSVisual::paint(QPainter *painter)
{
    int               *shMemInstanceCount = static_cast<int *>(sharedMemory->data());
    RMSMeter::RMSData *shMemRMSData       = reinterpret_cast<RMSMeter::RMSData *>(static_cast<char *>(sharedMemory->data()) + sizeof(int));

    sharedMemory->lock();
    int trackCount = *shMemInstanceCount;
    sharedMemory->unlock();

    QRect window = painter->window();

    if ((shMemRMSData == nullptr) || (trackCount < 1) || (audioChannel < 0) || (audioChannel > 1)) {
        peakHold.clear();

        painter->setBrush(QBrush(QColor(0, 0, 0, 255)));
        painter->drawRect(0, 0, window.width(), window.height());
        return;
    }

    painter->setRenderHints(QPainter::Antialiasing, true);
    painter->setPen(QPen(Qt::NoPen));

    int trackHeight = qRound(static_cast<double>(window.height() / trackCount));

    int count = 0;
    while (count < trackCount) {
        sharedMemory->lock();
        RMSMeter::RMSData rmsData = *shMemRMSData;
        sharedMemory->unlock();

        if (!peakHold.contains(rmsData.instanceId)) {
            peakHold.insert(rmsData.instanceId, { -99.9, 0 });
        }

        if (peakHold.value(rmsData.instanceId).peakHoldTime < (QDateTime::currentMSecsSinceEpoch() - 750)) {
            peakHold[rmsData.instanceId].peakHold = -99.9;
        }

        double peak = audioChannel == 0 ? rmsData.lpeak : rmsData.rpeak;
        double rms  = audioChannel == 0 ? rmsData.rrms  : rmsData.rrms;

        if (peak >= peakHold.value(rmsData.instanceId).peakHold) {
            peakHold[rmsData.instanceId] = { peak, QDateTime::currentMSecsSinceEpoch() };
        }

        double rmsRatio      = pow(10.0, rms / 20);
        double peakRatio     = pow(10.0, peak / 20);
        double peakHoldRatio = pow(10.0, peakHold.value(rmsData.instanceId).peakHold / 20);

        int top = qRound(static_cast<double>(trackHeight * count));
        if ((count == trackCount - 1) && (top + trackHeight < window.height())) {
            trackHeight = window.height() - top;
        }

        painter->setBrush(QBrush(QColor(0, 0, 0, 127)));
        painter->drawRect(0, top, qRound(width() * rmsRatio), trackHeight);

        if (window.width() * peakRatio < window.width() - 4) {
            painter->setBrush(QBrush(QColor(0, 0, 0, 255)));
            painter->drawRect(qRound(window.width() * peakRatio), top, qRound((window.width() * peakHoldRatio - 3) - window.width() * peakRatio), trackHeight);
            painter->drawRect(qRound(window.width() * peakHoldRatio), top, window.width(), trackHeight);

            painter->setBrush(QBrush(QColor(0, 0, 0, qMax(0, qMin(255, static_cast<int>((static_cast<double>(QDateTime::currentMSecsSinceEpoch() - peakHold.value(rmsData.instanceId).peakHoldTime) / 500) * 255))))));
            painter->drawRect(qRound(window.width() * peakHoldRatio - 4), top, 5, trackHeight);
        }
        else {
            painter->setBrush(QBrush(QColor(0, 0, 0, 255)));
            painter->drawRect(qRound(window.width() * peakRatio), top, window.width(), trackHeight);
        }

        count++;
        shMemRMSData++;
    }
}


// refresh
void RMSVisual::timerTimeout()
{
    update();
}

