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


#include "iirfilter.h"

// calculate coefficients for biquad filters (gainDecibell is ignored for non-gaining filter types)
CoefficientList IIRFilter::calculateBiquadCoefficients(FilterTypes filterType, double centerFrequency, double bandwidth, int sampleRate, double gainDecibel)
{
    CoefficientList returnValue;

    double temp;
    double gainFactor = pow(10, std::abs(gainDecibel) / 20);
    double Q          = centerFrequency / bandwidth;
    double K          = tan(M_PI * (centerFrequency / sampleRate));

    switch (filterType) {

        case LowPass:
            temp = 1 / (1 + K / Q + K * K);
            returnValue.appendA(K * K * temp);
            returnValue.appendA(2 * returnValue.aValue(0));
            returnValue.appendA(returnValue.aValue(0));
            returnValue.appendB(2 * (K * K - 1) * temp);
            returnValue.appendB((1 - K / Q + K * K) * temp);
            break;

        case HighPass:
            temp = 1 / (1 + K / Q + K * K);
            returnValue.appendA(temp);
            returnValue.appendA(-2 * returnValue.aValue(0));
            returnValue.appendA(returnValue.aValue(0));
            returnValue.appendB(2 * (K * K - 1) * temp);
            returnValue.appendB((1 - K / Q + K * K) * temp);
            break;

        case BandPass:
            temp = 1 / (1 + K / Q + K * K);
            returnValue.appendA(K / Q * temp);
            returnValue.appendA(0);
            returnValue.appendA(-1 * returnValue.aValue(0));
            returnValue.appendB(2 * (K * K - 1) * temp);
            returnValue.appendB((1 - K / Q + K * K) * temp);
            break;

        case BandStop:
            temp = 1 / (1 + K / Q + K * K);
            returnValue.appendA((1 + K * K) * temp);
            returnValue.appendA(2 * (K * K - 1) * temp);
            returnValue.appendA(returnValue.aValue(0));
            returnValue.appendB(returnValue.aValue(1));
            returnValue.appendB((1 - K / Q + K * K) * temp);
            break;

        case LowShelf:
            if (gainDecibel > 0) {
                temp = 1 / (1 + sqrt(2) * K + K * K);
                returnValue.appendA((1 + sqrt(2 * gainFactor) * K + gainFactor * K * K) * temp);
                returnValue.appendA(2 * (gainFactor * K * K - 1) * temp);
                returnValue.appendA((1 - sqrt(2 * gainFactor) * K + gainFactor * K * K) * temp);
                returnValue.appendB(2 * (K * K - 1) * temp);
                returnValue.appendB((1 - sqrt(2) * K + K * K) * temp);
            }
            else if (gainDecibel == 0) {
                returnValue.appendA(1);
                returnValue.appendA(0);
                returnValue.appendA(0);
                returnValue.appendB(0);
                returnValue.appendB(0);
            }
            else {
                temp = 1 / (1 + sqrt(2 * gainFactor) * K + gainFactor * K * K);
                returnValue.appendA((1 + sqrt(2) * K + K * K) * temp);
                returnValue.appendA(2 * (K * K - 1) * temp);
                returnValue.appendA((1 - sqrt(2) * K + K * K) * temp);
                returnValue.appendB(2 * (gainFactor * K * K - 1) * temp);
                returnValue.appendB((1 - sqrt(2 * gainFactor) * K + gainFactor * K * K) * temp);
            }
            break;

        case HighShelf:
            if (gainDecibel > 0) {
                temp = 1 / (1 + sqrt(2) * K + K * K);
                returnValue.appendA((gainFactor + sqrt(2 * gainFactor) * K + K * K) * temp);
                returnValue.appendA(2 * (K * K - gainFactor) * temp);
                returnValue.appendA((gainFactor - sqrt(2 * gainFactor) * K + K * K) * temp);
                returnValue.appendB(2 * (K * K - 1) * temp);
                returnValue.appendB((1 - sqrt(2) * K + K * K) * temp);
            }
            else if (gainDecibel == 0) {
                returnValue.appendA(1);
                returnValue.appendA(0);
                returnValue.appendA(0);
                returnValue.appendB(0);
                returnValue.appendB(0);
            }
            else {
                temp = 1 / (gainFactor + sqrt(2 * gainFactor) * K + K * K);
                returnValue.appendA((1 + sqrt(2) * K + K * K) * temp);
                returnValue.appendA(2 * (K * K - 1) * temp);
                returnValue.appendA((1 - sqrt(2) * K + K * K) * temp);
                returnValue.appendB(2 * (K * K - gainFactor) * temp);
                returnValue.appendB((gainFactor - sqrt(2 * gainFactor) * K + K * K) * temp);
            }
            break;

        case BandShelf:
            if (gainDecibel > 0) {
                temp = 1 / (1 + 1 / Q * K + K * K);
                returnValue.appendA((1 + gainFactor / Q * K + K * K) * temp);
                returnValue.appendA(2 * (K * K - 1) * temp);
                returnValue.appendA((1 - gainFactor / Q * K + K * K) * temp);
                returnValue.appendB(returnValue.aValue(1));
                returnValue.appendB((1 - 1 / Q * K + K * K) * temp);
            }
            else if (gainDecibel == 0) {
                returnValue.appendA(1);
                returnValue.appendA(0);
                returnValue.appendA(0);
                returnValue.appendB(0);
                returnValue.appendB(0);
            }
            else {
                temp = 1 / (1 + gainFactor / Q * K + K * K);
                returnValue.appendA((1 + 1 / Q * K + K * K) * temp);
                returnValue.appendA(2 * (K * K - 1) * temp);
                returnValue.appendA((1 - 1 / Q * K + K * K) * temp);
                returnValue.appendB(returnValue.aValue(1));
                returnValue.appendB((1 - gainFactor / Q * K + K * K) * temp);
            }
    }

    return returnValue;
}


// calculate coefficients for biquad filters overload: no gain
CoefficientList IIRFilter::calculateBiquadCoefficients(FilterTypes filterType, double centerFrequency, double bandwidth, int sampleRate)
{
    return calculateBiquadCoefficients(filterType, centerFrequency, bandwidth, sampleRate, 0);
}


// return our sample type from Qt's audio format
IIRFilter::SampleTypes IIRFilter::getSampleTypeFromAudioFormat(QAudioFormat audioFormat)
{
    SampleTypes sampleType = Unknown;

    if (audioFormat.sampleType() == QAudioFormat::Float) {
        sampleType = floatSample;

    }
    else if (audioFormat.sampleType() == QAudioFormat::SignedInt) {
        switch (audioFormat.sampleSize()) {
            case 8:
                sampleType = int8Sample;
                break;
            case 16:
                sampleType = int16Sample;
                break;
            case 32:
                sampleType = int32Sample;
        }

    }
    else if (audioFormat.sampleType() == QAudioFormat::UnSignedInt) {
        switch (audioFormat.sampleSize()) {
            case 8:
                sampleType = uint8Sample;
                break;
            case 16:
                sampleType = uint16Sample;
                break;
            case 32:
                sampleType = uint32Sample;
        }
    }

    return sampleType;
}


// constructor
IIRFilter::IIRFilter()
{
    updateData = true;

    a       = NULL;
    b       = NULL;
    aLength = 0;

    reset();

    callbackRawMember      = NULL;
    callbackRawObject      = NULL;
    callbackFilteredObject = NULL;
    callbackFilteredMember = NULL;
}


// constructor overload
IIRFilter::IIRFilter(CoefficientList coefficientList)
{
    updateData = true;

    a       = NULL;
    b       = NULL;
    aLength = 0;

    reset();

    callbackRawMember      = NULL;
    callbackRawObject      = NULL;
    callbackFilteredObject = NULL;
    callbackFilteredMember = NULL;

    applyCoefficients(coefficientList);
}


// destructor
IIRFilter::~IIRFilter()
{
    // free memory
    if (a != NULL) {
        delete[] a;
    }
    if (b != NULL) {
        delete[] b;
    }
}


// change coefficients
void IIRFilter::setCoefficients(CoefficientList coefficientList)
{
    applyCoefficients(coefficientList);
}


// treat PCM data readonly, this can be useful when using callbacks in a paralell filters situation
void IIRFilter::disableUpdateData()
{
    updateData = false;
}


// change coefficients (helper method called from multiple public methods)
void IIRFilter::applyCoefficients(CoefficientList coefficientList)
{
    // reset current coefficients if any
    if (a != NULL) {
        delete[] a;
        a = NULL;
    }
    if (b != NULL) {
        delete[] b;
        b = NULL;
    }
    aLength = 0;

    // parameter checking (either b is one element shorter than a, or the two are the same size in which case b[0] is ignored)
    if ((coefficientList.aSize() < 2) || !((coefficientList.aSize() == coefficientList.bSize()) ||
            (coefficientList.aSize() == (coefficientList.bSize() + 1)))) {
        return;
    }

    // cap number of coefficients
    aLength = qMin(coefficientList.aSize(), MAX_ORDER + 1);

    // create the arrays
    a = new double[aLength];
    b = new double[aLength];

    // copy the data
    b[0] = 0;
    for (int i = 0; i < aLength; i++) {
        a[i] = coefficientList.aValue(i);
        if (i < coefficientList.bSize()) {
            b[coefficientList.aSize() > coefficientList.bSize() ? i + 1 : i] = coefficientList.bValue(i);
        }
    }
}


// set callback pointer for raw data
void IIRFilter::setCallbackRaw(IIRFilterCallback *callbackRawObject, IIRFilterCallback::FilterCallbackPointer callbackRawMember)
{
    this->callbackRawObject = callbackRawObject;
    this->callbackRawMember = callbackRawMember;
}


// set callback pointer for filtered data
void IIRFilter::setCallbackFiltered(IIRFilterCallback *callbackFilteredObject,
    IIRFilterCallback::FilterCallbackPointer callbackFilteredMember)
{
    this->callbackFilteredObject = callbackFilteredObject;
    this->callbackFilteredMember = callbackFilteredMember;
}


// apply the filter to PCM data
void IIRFilter::processPCMData(void *data, int byteCount, SampleTypes sampleType, int channelCount)
{
    // make sure coefficiants were set
    if ((a == NULL) || (b == NULL) || (aLength < 2)) {
        return;
    }

    // call the template method (see in header) with appropriate data type for each supported sample type
    switch (sampleType) {

        case Unknown:
            break;

        case int8Sample:
            process<qint8>(data, byteCount, channelCount);
            break;

        case uint8Sample:
            process<quint8>(data, byteCount, channelCount);
            break;

        case int16Sample:
            process<qint16>(data, byteCount, channelCount);
            break;

        case uint16Sample:
            process<quint16>(data, byteCount, channelCount);
            break;

        case int32Sample:
            process<qint32>(data, byteCount, channelCount);
            break;

        case uint32Sample:
            process<quint32>(data, byteCount, channelCount);
            break;

        case floatSample:
            process<float>(data, byteCount, channelCount);
    }
}


// reset buffers
void IIRFilter::reset()
{
    memset(&inputBuffer,  0, MAX_CHANNELS * MAX_ORDER * sizeof(double));
    memset(&outputBuffer, 0, MAX_CHANNELS * MAX_ORDER * sizeof(double));
    currentChannel = 0;
}
