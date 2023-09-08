/*
    This file is part of WaverIIR
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waveriir for details
*/


#ifndef IIRFILTERCHAIN_H
#define IIRFILTERCHAIN_H

#include <QList>
#include "coefficientlist.h"
#include "iirfilter.h"


class IIRFilterChain {

    public:

        static const int MAX_FILTERS = 50;

        // constructors and destructor
        IIRFilterChain();
        IIRFilterChain(QList<CoefficientList> coefficientLists);
        ~IIRFilterChain();

        // filter management
        void       appendFilter(CoefficientList coefficientList);
        IIRFilter *getFilter(int index);
        int        getFilterCount();

        // filtering
        void processPCMData(void *data, int byteCount, IIRFilter::SampleTypes sampleType, int channelCount);
        void reset();


    private:

        // filters
        int        filterCount;
        IIRFilter *filters[MAX_FILTERS];

};

#endif // IIRFILTERCHAIN_H
