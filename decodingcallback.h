/*
    This file is part of Waver
    Copyright (C) 2022 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef DECODINGCALLBACK_H
#define DECODINGCALLBACK_H

#include <QtCore>
#include <QMutex>


class DecodingCallback
{
    public:

        typedef void (DecodingCallback::*DecodingCallbackPointer)(double, double, void *);

        struct DecodingCallbackInfo {
            DecodingCallback                          *callbackObject;
            DecodingCallback::DecodingCallbackPointer  callbackMethod;
            void                                      *trackPointer;
        };

        virtual ~DecodingCallback();

        virtual void decodingCallback(double downloadPercent, double PCMPercent, void *trackPointer) = 0;
};

#endif // DECODINGCALLBACK_H
