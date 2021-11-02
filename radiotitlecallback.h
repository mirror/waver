/*
    This file is part of Waver
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waver for details
*/


#ifndef RADIOTITLECALLBACK_H
#define RADIOTITLECALLBACK_H

#include <QtCore>


class RadioTitleCallback
{
    public:

        typedef void (RadioTitleCallback::*RadioTitleCallbackPointer)(QString);

        struct RadioTitleCallbackInfo {
            RadioTitleCallback                            *callbackObject;
            RadioTitleCallback::RadioTitleCallbackPointer  callbackMethod;
        };

        virtual ~RadioTitleCallback();

        virtual void radioTitleCallback(QString title) = 0;

};

#endif // RADIOTITLECALLBACK_H
