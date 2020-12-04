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
