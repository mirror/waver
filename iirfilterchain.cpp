/*
    This file is part of WaverIIR
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waveriir for details
*/


#include "iirfilterchain.h"

// constructor one: empty chain
IIRFilterChain::IIRFilterChain()
{
    // initialization
    filterCount = 0;
    for (int i = 0; i < MAX_FILTERS; i++) {
        filters[i] = nullptr;
    }
}


// constructor two: chain from list of coefficient lists
IIRFilterChain::IIRFilterChain(QList<CoefficientList> coefficientLists)
{
    // initialization
    filterCount = 0;
    for (int i = 0; i < MAX_FILTERS; i++) {
        filters[i] = nullptr;
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
        if (filters[i] != nullptr) {
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
        return nullptr;
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
