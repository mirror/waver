/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef PEAKCALLBACK_H
#define PEAKCALLBACK_H

#include <QtCore>


class PeakCallback
{
    public:

        typedef void (PeakCallback::*PeakCallbackPointer)(double, double, qint64, void *);

        struct PeakCallbackInfo {
            PeakCallback                      *callbackObject;
            PeakCallback::PeakCallbackPointer  callbackMethod;
            void                              *trackPointer;
        };

        virtual ~PeakCallback();

        virtual void peakCallback(double lPeak, double rPeak, qint64 delayMicroseconds, void *trackPointer) = 0;
};

#endif // PEAKCALLBACK_H
