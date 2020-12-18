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


#include "globals.h"


QString Globals::appName()
{
    // don't forget to update the android manifest too
    return "Waver";
}


QString Globals::appVersion()
{
    // don't forget to update the android manifest and debian changelog too
    return "0.0.5";
}


QString Globals::appDesc()
{
    return "Open source sound player";
}


QString Globals::author()
{
    return "Peter Papp";
}


QString Globals::email(bool linkOk)
{
    if (linkOk) {
        return "<a href=\"mailto:peter.papp.p@gmail.com\">peter.papp.p@gmail.com</a>";
    }

    return "peter.papp.p@gmail.com";
}


QString Globals::website(bool linkOk)
{
    if (linkOk) {
        return "<a href=\"https://launchpad.net/waver\">launchpad.net/waver</a>";
    }

    return "https://launchpad.net/waver";
}


QString Globals::copyright()
{
    return "Copyright (C) 2017-2020 Peter Papp";
}


QString Globals::license()
{
    return "This is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.\n\nThis software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.\n\nYou should have received a copy of the GNU General Public License (GPL.TXT) along with this software. If not, see http://www.gnu.org/licenses/";
}

QString Globals::credits()
{
    return "Waver is built using the <a href=\"https://www.qt.io/\">Qt</a> framework.\nOnline Radio Stations plugin is powered by <a href=\"https://www.shoutcast.com\">SHOUTcast</a>.";
}


QString Globals::privacy()
{
    return "Waver does not collect or send personal information of any kind";
}


QString Globals::userAgent()
{
    return appName() + "/" + appVersion() + " ( " + website(false) + " )";
}


QString Globals::lastUsedCollectionOption()
{
    return "--- last used ---";
}


// simple helper console output
void Globals::consoleOutput(QString text, bool error)
{
    QTextStream cout(error ? stderr : stdout);
    cout << text;
    cout << "\n\r";
    cout.flush();
}
