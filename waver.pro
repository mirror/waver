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
    coefficientlist.h \
    decodergeneric.h \
    decodergenericnetworksource.h \
    decodingcallback.h \
    equalizer.h \
    filescanner.h \
    filesearcher.h \
    globals.h \
    iirfilter.h \
    iirfiltercallback.h \
    iirfilterchain.h \
    notificationshandler.h \
    outputfeeder.h \
    pcmcache.h \
    peakcallback.h \
    radiotitlecallback.h \
    replaygaincoefficients.h \
    replaygaincalculator.h \
    soundoutput.h \
    track.h \
    waver.h \
    waverapplication.h

SOURCES += \
    ampacheserver.cpp \
    analyzer.cpp \
    coefficientlist.cpp \
    decodergeneric.cpp \
    decodergenericnetworksource.cpp \
    decodingcallback.cpp \
    equalizer.cpp \
    filescanner.cpp \
    filesearcher.cpp \
    iirfilter.cpp \
    iirfiltercallback.cpp \
    iirfilterchain.cpp \
    main.cpp \
    notificationshandler.cpp \
    outputfeeder.cpp \
    pcmcache.cpp \
    peakcallback.cpp \
    radiotitlecallback.cpp \
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
    QT += dbus

    HEADERS += \
        mediaplayer2dbusadaptor.h \
        mediaplayer2playerdbusadaptor.h

    SOURCES += \
        mediaplayer2dbusadaptor.cpp \
        mediaplayer2playerdbusadaptor.cpp

    LIBS += -L/usr/lib/i386-linux-gnu -L/usr/lib/x86_64-linux-gnu -lqt5keychain -ltag

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
    INCLUDEPATH += $$PWD/.. $$PWD/../qt5keychain $$PWD/../qt5keychain/build

    HEADERS += \
        trayicon.h \
        wintoastlib.h

    SOURCES += \
        trayicon.cpp \
        wintoastlib.cpp

    !winrt {
        INCLUDEPATH += $$PWD/../taglib-1.12 $$PWD/../taglib-1.12/taglib/toolkit
        LIBS += $$PWD/../qt5keychain/build/Release/qt5keychain.lib $$PWD/../taglib-1.12/taglib/Release/tag.lib
    }
    winrt {
        LIBS += $$PWD/../qt5keychain/build-uwp/Release/qt5keychain.lib
    }
}
