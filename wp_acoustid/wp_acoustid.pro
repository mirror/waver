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


QT -= gui
QT += multimedia

TARGET = wp_acoustid
TEMPLATE = lib

DEFINES += WP_ACOUSTID_LIBRARY

CONFIG += c++11

CONFIG(debug, debug|release) {
    QMAKE_LFLAGS += -rdynamic
}


SOURCES += \
    analyzer.cpp \
    acoustid.cpp \
    main.cpp

HEADERS += \
    wp_acoustid_global.h \
    ../waver/API/pluginbase_006.h \
    ../waver/API/plugindsppre_006.h \
    ../waver/API/plugininfo_006.h \
    ../waver/pluginfactory.h \
    main.h \
    analyzer.h \
    acoustid.h

unix:!android {
    LIBS += -L/usr/lib/i386-linux-gnu -L/usr/lib/x86_64-linux-gnu -lchromaprint

    target.path = /opt/waver/bin
    INSTALLS += target

    translatedestdir.commands = $(eval INSTALL_ROOT := $(DESTDIR))
    install.depends = translatedestdir
    QMAKE_EXTRA_TARGETS += install translatedestdir
}

DISTFILES +=

RESOURCES += \
    resources.qrc
