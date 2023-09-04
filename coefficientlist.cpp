/*
    This file is part of WaverIIR
    Copyright (C) 2021 Peter Papp
    Please visit https://launchpad.net/waveriir for details
*/


#include "coefficientlist.h"


CoefficientList::CoefficientList()
{
    // nothing to do here
}


CoefficientList::CoefficientList(QList<double> a, QList<double> b)
{
    this->a.append(a);
    this->b.append(b);
}


CoefficientList::CoefficientList(std::initializer_list<double> a, std::initializer_list<double> b)
{
    this->a = QList<double>(a);
    this->b = QList<double>(b);
}


void CoefficientList::appendA(double aValue)
{
    a.append(aValue);
}


void CoefficientList::appendB(double bValue)
{
    b.append(bValue);
}


int CoefficientList::aSize()
{
    return a.size();
}


int CoefficientList::bSize()
{
    return b.size();
}


double CoefficientList::aValue(int index)
{
    return a.at(index);
}


double CoefficientList::bValue(int index)
{
    return b.at(index);
}
