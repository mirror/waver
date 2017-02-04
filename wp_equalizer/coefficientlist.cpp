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


#include "coefficientlist.h"


// constructor
CoefficientList::CoefficientList()
{
    // nothing to do here
}


// constructor overload
CoefficientList::CoefficientList(QList<double> a, QList<double> b)
{
    this->a.append(a);
    this->b.append(b);
}


// constructor overload
CoefficientList::CoefficientList(std::initializer_list<double> a, std::initializer_list<double> b)
{
    this->a = QList<double>(a);
    this->b = QList<double>(b);
}


// manually add coefficient
void CoefficientList::appendA(double aValue)
{
    a.append(aValue);
}


// manually add coefficient
void CoefficientList::appendB(double bValue)
{
    b.append(bValue);
}


// return coefficient list size
int CoefficientList::aSize()
{
    return a.size();
}


// return coefficient list size
int CoefficientList::bSize()
{
    return b.size();
}


// return coefficient value
double CoefficientList::aValue(int index)
{
    return a.at(index);
}


// return coefficient value
double CoefficientList::bValue(int index)
{
    return b.at(index);
}
