/*
    This file is part of WaverIIR
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waveriir for details
*/


#ifndef IIRFILTERCALLBACK_H
#define IIRFILTERCALLBACK_H


class IIRFilterCallback {

    public:

        // convenience pointer-to-callback type
        typedef void (IIRFilterCallback::*FilterCallbackPointer)(double *, int);

        // need a virtual destructor
        virtual ~IIRFilterCallback();

        // callback definition
        virtual void filterCallback(double *sample, int channelIndex) = 0;

};

#endif // IIRFILTERCALLBACK_H
