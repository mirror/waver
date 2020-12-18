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


#include "iirfilterchain.h"

// constructor one: empty chain
IIRFilterChain::IIRFilterChain()
{
    // initialization
    filterCount = 0;
    for (int i = 0; i < MAX_FILTERS; i++) {
        filters[i] = NULL;
    }
}


// constructor two: chain from list of coefficient lists
IIRFilterChain::IIRFilterChain(QList<CoefficientList> coefficientLists)
{
    // initialization
    filterCount = 0;
    for (int i = 0; i < MAX_FILTERS; i++) {
        filters[i] = NULL;
    }

    // append the filters
    for (int i = 0; i < coefficientLists.size(); i++) {
        appendFilter(coefficientLists.at(i));
    }
}


// destructor
IIRFilterChain::~IIRFilterChain()
{
    for (int i = 0; i < MAX_FILTERS; i++) {
        if (filters[i] != NULL) {
            delete filters[i];
        }
    }
}


// append a filter
void IIRFilterChain::appendFilter(CoefficientList coefficientList)
{
    if (filterCount >= MAX_FILTERS) {
        return;
    }

    filters[filterCount] = new IIRFilter(coefficientList);

    filterCount++;
}


// return pointer to a filter
IIRFilter *IIRFilterChain::getFilter(int index)
{
    if ((index < 0) || (index >= filterCount)) {
        return NULL;
    }
    return filters[index];
}


// return the current count of filters
int  IIRFilterChain::getFilterCount()
{
    return filterCount;
}


// apply the whole chain of filters to PCM data
void IIRFilterChain::processPCMData(void *data, int byteCount, IIRFilter::SampleTypes sampleType, int channelCount)
{
    for (int i = 0; i < filterCount; i++) {
        filters[i]->processPCMData(data, byteCount, sampleType, channelCount);
    }
}


// reset buffers of every filter in the chain
void   IIRFilterChain::reset()
{
    for (int i = 0; i < filterCount; i++) {
        filters[i]->reset();
    }
}
