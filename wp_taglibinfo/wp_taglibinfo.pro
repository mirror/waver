#    This file is part of Waver
#
#    Copyright (C) 2017 Peter Papp <peter.papp.p@gmail.com>
#
#    Please visit https://launchpad.net/waver for details
#
#    Waver is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    Waver is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    (GPL.TXT) along with Waver. If not, see <http://www.gnu.org/licenses/>.



QT -= gui
QT += multimedia

TARGET = wp_taglibinfo
TEMPLATE = lib

DEFINES += WP_TAGLIBINFO_LIBRARY

SOURCES += taglibinfo.cpp

HEADERS += taglibinfo.h\
        wp_taglibinfo_global.h \
    ../waver/pluginbase.h \
    ../waver/pluginsource.h \
    ../waver/plugininfo.h

LIBS += /usr/lib/x86_64-linux-gnu/libtag.so

unix {
    target.path = /opt/waver/bin
    INSTALLS += target
}
