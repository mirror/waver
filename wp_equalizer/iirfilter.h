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


#ifndef IIRFILTER_H
#define IIRFILTER_H

#include <QtCore>

#include <QAudioFormat>

#include "coefficientlist.h"
#include "iirfiltercallback.h"


class IIRFilter {

    public:

        static const int MAX_ORDER    = 12;
        static const int MAX_CHANNELS = 8;

        // filter types for coefficient calculation (BandStop is also called Notch, BandShelf is also called Peak)
        enum FilterTypes { LowPass, HighPass, BandPass, BandStop, LowShelf, HighShelf, BandShelf };

        // supported sample types
        enum SampleTypes { Unknown, int8Sample, uint8Sample, int16Sample, uint16Sample, int32Sample, uint32Sample, floatSample };

        // convenience
        static CoefficientList calculateBiquadCoefficients(FilterTypes filterType, double centerFrequency, double bandwidth, int sampleRate, double gainDecibel);
        static CoefficientList calculateBiquadCoefficients(FilterTypes filterType, double centerFrequency, double bandwidth, int sampleRate);
        static SampleTypes     getSampleTypeFromAudioFormat(QAudioFormat audioFormat);

        // constructor and destructor
        IIRFilter();
        IIRFilter(CoefficientList coefficientList);
        ~IIRFilter();

        // manage
        void setCoefficients(CoefficientList coefficientList);
        void disableUpdateData();

        // callback functions
        void setCallbackRaw(IIRFilterCallback *callbackRawObject, IIRFilterCallback::FilterCallbackPointer callbackRawMember);
        void setCallbackFiltered(IIRFilterCallback *callbackFilteredObject,
            IIRFilterCallback::FilterCallbackPointer callbackFilteredMember);

        // filtering
        void processPCMData(void *data, int byteCount, SampleTypes sampleType, int channelCount);
        void reset();


    private:

        // mode of operation
        bool updateData;

        // coefficients
        double *a;
        double *b;
        int     aLength;

        // input and output buffers
        double inputBuffer[MAX_CHANNELS][MAX_ORDER];
        double outputBuffer[MAX_CHANNELS][MAX_ORDER];
        int    currentChannel;

        // callback pointers (using callbacks here because they are faster than signals)
        IIRFilterCallback                        *callbackRawObject;
        IIRFilterCallback                        *callbackFilteredObject;
        IIRFilterCallback::FilterCallbackPointer  callbackRawMember;
        IIRFilterCallback::FilterCallbackPointer  callbackFilteredMember;

        // manange
        void applyCoefficients(CoefficientList coefficientList);

        // filtering - template function works with all supported sample types
        template <class T> void process(void *data, int byteCount, int channelCount)
        {
            // variables to hold the sample's value
            T      *temp;
            double  sample;
            double  filteredSample;

            // minimum and maximum values
            double minValue = std::numeric_limits<T>::min();
            double maxValue = std::numeric_limits<T>::max();

            // process each sample in buffer
            int processedCount = 0;
            while (processedCount < byteCount) {
                // pointer to the sample
                temp = (T *)((char *)data + processedCount);

                // get the sample as double value
                sample = *temp;

                // callback
                if ((callbackRawObject != NULL) && (callbackRawMember != NULL)) {
                    (callbackRawObject->*callbackRawMember)(&sample, currentChannel);
                    if (sample < minValue) {
                        sample = minValue;
                    }
                    if (sample > maxValue) {
                        sample = maxValue;
                    }
                }

                // calculation
                filteredSample = 1e-10 + a[0] * sample;
                for (int i = 1; i < aLength; i++) {
                    filteredSample = filteredSample + a[i] * inputBuffer[currentChannel][i - 1] - b[i] * outputBuffer[currentChannel][i - 1];
                }

                // input and output buffer for the next sample calculation
                memmove(&inputBuffer[currentChannel][1], &inputBuffer[currentChannel][0], sizeof(double) * (aLength - 1));
                memmove(&outputBuffer[currentChannel][1], &outputBuffer[currentChannel][0], sizeof(double) * (aLength - 1));
                inputBuffer[currentChannel][0]  = sample;
                outputBuffer[currentChannel][0] = filteredSample;

                // callback
                if ((callbackFilteredObject != NULL) && (callbackFilteredMember != NULL)) {
                    if (filteredSample < minValue) {
                        filteredSample = minValue;
                    }
                    if (filteredSample > maxValue) {
                        filteredSample = maxValue;
                    }
                    (callbackFilteredObject->*callbackFilteredMember)(&filteredSample, currentChannel);
                }

                // put filtered sample back to buffer
                if (updateData) {
                    if (filteredSample < minValue) {
                        filteredSample = minValue;
                    }
                    if (filteredSample > maxValue) {
                        filteredSample = maxValue;
                    }

                    *temp = static_cast<T>(filteredSample);
                }

                // on to the next sample
                processedCount += sizeof(T);
                currentChannel++;
                if (currentChannel >= channelCount) {
                    currentChannel = 0;
                }
            }
        }
};

#endif // IIRFILTER_H
