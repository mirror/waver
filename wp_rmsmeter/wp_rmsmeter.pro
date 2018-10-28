#
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
#

QT += multimedia websockets qml quick
QT -= gui

TARGET = wp_rmsmeter
TEMPLATE = lib

DEFINES += WP_RMSMETER_LIBRARY

CONFIG += c++11 qt plugin

CONFIG(debug, debug|release) {
    QMAKE_LFLAGS += -rdynamic
}

SOURCES += \
    rmsmeter.cpp \
    websocketserver.cpp \
    rmsvisual.cpp \
    rmsqml.cpp \
    websocketclient.cpp

HEADERS += \
    rmsmeter.h \
    websocketserver.h \
    wp_rmsmeter_global.h \
    ../waver/API/pluginbase_006.h \
    ../waver/API/pluginoutput_006.h \
    ../waver/pluginfactory.h \
    rmsvisual.h \
    rmsqml.h \
    websocketclient.h

RESOURCES += \
    resources.qrc

DISTFILES += \
    qmldir

unix:!android {
    target.path = /opt/waver/bin
    INSTALLS += target

    translatedestdir.commands = $(eval INSTALL_ROOT := $(DESTDIR))
    install.depends = translatedestdir
    QMAKE_EXTRA_TARGETS += install translatedestdir

    qmlcomponent.path = /opt/waver/bin/WaverRMSMeter
    qmlcomponent.files = qmldir
    qmlcomponent.extra = ln -sf /opt/waver/bin/libwp_rmsmeter.so /opt/waver/bin/WaverRMSMeter/
    INSTALLS += qmlcomponent

    qmlcomponentremove.commands = rm -f /opt/waver/bin/WaverRMSMeter/libwp_rmsmeter.so
    uninstall.depends = qmlcomponentremove
    QMAKE_EXTRA_TARGETS += uninstall qmlcomponentremove
}

