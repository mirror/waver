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


#include "globals.h"


QString Globals::appName()
{
    // don't forget to update the android manifest too
    return "Waver";
}


QString Globals::appVersion()
{
    // don't forget to update the android manifest and debian changelog too
    return "0.0.3";
}


QString Globals::appDesc()
{
    return "Open source sound player with plug-in architecture";
}


// simple helper console output
void Globals::consoleOutput(QString text, bool error)
{
    QTextStream cout(error ? stderr : stdout);
    cout << text;
    cout << "\n\r";
    cout.flush();
}
