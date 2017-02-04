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


#ifndef IIRFILTERCHAIN_H
#define IIRFILTERCHAIN_H

#include <QList>
#include "iirfilter.h"
#include "coefficientlist.h"


class IIRFilterChain
{

public:

    static const int MAX_FILTERS = 25;

    // constructors and destructor
    IIRFilterChain();
    IIRFilterChain(QList<CoefficientList> coefficientLists);
    ~IIRFilterChain();

    // filter management
    void       appendFilter(CoefficientList coefficientList);
    IIRFilter* getFilter(int index);
    int        getFilterCount();

    // filtering
    void processPCMData(void *data, int byteCount, IIRFilter::SampleTypes sampleType, int channelCount);
    void reset();


private:

    // filters
    int        filterCount;
    IIRFilter* filters[MAX_FILTERS];

};

#endif // IIRFILTERCHAIN_H
