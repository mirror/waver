#    This file is part of Waver
#
#    Copyright (C) 2017-2019 Peter Papp <peter.papp.p@gmail.com>
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

QT += multimedia
QT -= gui

TARGET   = wp_mpg123decoder
TEMPLATE = lib

DEFINES += WP_MPG123DECODER_LIBRARY

CONFIG += c++11

CONFIG(debug, debug|release) {
    QMAKE_LFLAGS += -rdynamic
}

SOURCES += mpg123decoder.cpp \
    feed.cpp

HEADERS += \
    mpg123lib/fmt123.h        \
    mpg123lib/mpg123.h        \
    wp_mpg123decoder_global.h \
    mpg123decoder.h \
    ../waver/API/pluginbase_006.h \
    ../waver/API/plugindecoder_006.h \
    ../waver/pluginfactory.h \
    feed.h

unix:!android {
    LIBS += -L/usr/lib/i386-linux-gnu -L/usr/lib/x86_64-linux-gnu -lmpg123

    target.path = /opt/waver/bin
    INSTALLS += target

    translatedestdir.commands = $(eval INSTALL_ROOT := $(DESTDIR))
    install.depends = translatedestdir
    QMAKE_EXTRA_TARGETS += install translatedestdir
}

windows {
    LIBS += -L$$PWD/mpg123lib/win32 -lmpg123-0
}

