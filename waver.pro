#
#    This file is part of Waver
#    Copyright (C) 2021 Peter Papp
#    Please visit https://launchpad.net/waver for details
#

QT += gui multimedia quick quickcontrols2

CONFIG += c++11
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x051210

HEADERS += \
    ampacheserver.h \
    analyzer.h \
    decodergeneric.h \
    decodergenericnetworksource.h \
    equalizer.h \
    filescanner.h \
    globals.h \
    outputfeeder.h \
    pcmcache.h \
    peakcallback.h \
    replaygaincalculator.h \
    soundoutput.h \
    track.h \
    waver.h \
    waverapplication.h

SOURCES += \
    ampacheserver.cpp \
    analyzer.cpp \
    decodergeneric.cpp \
    decodergenericnetworksource.cpp \
    equalizer.cpp \
    filescanner.cpp \
    main.cpp \
    outputfeeder.cpp \
    pcmcache.cpp \
    peakcallback.cpp \
    replaygaincalculator.cpp \
    soundoutput.cpp \
    track.cpp \
    waver.cpp \
    waverapplication.cpp

RESOURCES += \
    res.qrc

TRANSLATIONS += \
    waver_new_en_US.ts

unix:!android {
    LIBS += -L/usr/lib/i386-linux-gnu -L/usr/lib/x86_64-linux-gnu -lqt5keychain -ltag -lwaveriir

    target.path = /opt/waver/bin
    INSTALLS += target
    
    launcher.path = /usr/share/applications
    launcher.files = launcher/waver.desktop
    icon.path = /opt/waver/pixmaps
    icon.files = launcher/waver_icon.png
    INSTALLS += launcher icon

    updatedestdir.commands = $(eval INSTALL_ROOT := $(DESTDIR))
    install.depends = updatedestdir
    QMAKE_EXTRA_TARGETS += install updatedestdir
}

windows {
    LIBS += -lqt5keychain #-L$$PWD/mpg123lib/win32
}
