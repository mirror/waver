/*
    This file is part of WaverIIR
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waveriir for details
*/


#ifndef COEFFICIENTLIST_H
#define COEFFICIENTLIST_H

#include <QList>
#include "waveriir_global.h"


class WAVERIIR_EXPORT CoefficientList {

    public:

        CoefficientList();
        CoefficientList(QList<double> a, QList<double> b);
        CoefficientList(std::initializer_list<double> a, std::initializer_list<double> b);

        void appendA(double aValue);
        void appendB(double bValue);

        int aSize();
        int bSize();

        double aValue(int index);
        double bValue(int index);


    private:

        QList<double> a;
        QList<double> b;

};

#endif // COEFFICIENTLIST_H
