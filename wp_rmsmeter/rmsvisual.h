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

#ifndef RMSVISUAL_H
#define RMSVISUAL_H

#include <QByteArray>
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QHash>
#include <QPainter>
#include <QPen>
#include <QRect>
#include <QQuickPaintedItem>
#include <QSharedMemory>
#include <QThread>
#include <QTimer>
#include <math.h>

#include "rmsmeter.h"

#ifdef QT_DEBUG
    #include <QDebug>
#endif


class RMSVisual : public QQuickPaintedItem {
        Q_OBJECT

        Q_PROPERTY(int channel READ channel WRITE setChannel NOTIFY channelChanged)

    public:

        RMSVisual(QQuickItem *parent = nullptr);
        ~RMSVisual();

        int  channel() const;
        void setChannel(const int channel);

        void paint(QPainter *painter);


    private:

        struct PeakHold {
            double peakHold;
            qint64 peakHoldTime;
        };

        int audioChannel;

        QHash<int, PeakHold> peakHold;

        QSharedMemory sharedMemory;

        QTimer timer;


    signals:

        void channelChanged();


    private slots:

        void timerTimeout();
};

#endif // RMSVISUAL_H
