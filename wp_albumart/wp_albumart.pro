#
#    This file is part of Waver
#
#    Copyright (C) 2017-2020 Peter Papp <peter.papp.p@gmail.com>
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
#

QT += multimedia
QT -= gui

TARGET = wp_albumart
TEMPLATE = lib

DEFINES += WP_ALBUMART_LIBRARY

CONFIG += c++11

CONFIG(debug, debug|release) {
    QMAKE_LFLAGS += -rdynamic
}


SOURCES += \
        albumart.cpp

HEADERS += \
    albumart.h \
    wp_albumart_global.h \
    ../waver/API/pluginbase_004.h \
    ../waver/API/plugininfo_004.h \
    ../waver/pluginfactory.h \

unix:!android {
    target.path = /opt/waver/bin
    INSTALLS += target

    translatedestdir.commands = $(eval INSTALL_ROOT := $(DESTDIR))
    install.depends = translatedestdir
    QMAKE_EXTRA_TARGETS += install translatedestdir
}

