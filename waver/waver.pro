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


QT += qml quick multimedia

android {
    QT +=androidextras
}

CONFIG += c++11

HEADERS += \
    globals.h \
    servertcphandler.h \
    settingshandler.h \
    server.h \
    clienttcphandler.h \
    pluginbase.h \
    pluginlibsloader.h \
    waverapplication.h \
    ipcmessageutils.h \
    pluginsource.h \
    track.h \
    plugindecoder.h \
    plugindsppre.h \
    plugindsp.h \
    pluginoutput.h \
    plugininfo.h

SOURCES += main.cpp \
    globals.cpp \
    servertcphandler.cpp \
    settingshandler.cpp \
    server.cpp \
    clienttcphandler.cpp \
    waverapplication.cpp \
    pluginlibsloader.cpp \
    ipcmessageutils.cpp \
    track.cpp

RESOURCES += qml.qrc \
    visual.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/waver/bin
!isEmpty(target.path): INSTALLS += target

android {
    DISTFILES += \
        android/AndroidManifest.xml \
        android/gradle/wrapper/gradle-wrapper.jar \
        android/gradlew \
        android/res/values/libs.xml \
        android/build.gradle \
        android/gradle/wrapper/gradle-wrapper.properties \
        android/gradlew.bat \
        android/src/rocks/waver/waver/WaverActivity.java \
        android/src/rocks/waver/waver/WaverService.java

    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

    CONFIG(debug, debug|release) {
        ANDROID_EXTRA_LIBS += /home/pp/Fejleszt/waver/build-wp_localsource-Android_for_armeabi_v7a_GCC_4_9_Qt_5_7_0-Debug/android-build/libs/armeabi-v7a/libwp_localsource.so
        ANDROID_EXTRA_LIBS += /home/pp/Fejleszt/waver/build-wp_androiddecoder-Android_for_armeabi_v7a_GCC_4_9_Qt_5_7_0-Debug/android-build/libs/armeabi-v7a/libwp_androiddecoder.so
        ANDROID_EXTRA_LIBS += /home/pp/Fejleszt/waver/build-wp_soundoutput-Android_for_armeabi_v7a_GCC_4_9_Qt_5_7_0-Debug/android-build/libs/armeabi-v7a/libwp_soundoutput.so
    }

    CONFIG(release, debug|release) {
        ANDROID_EXTRA_LIBS += /home/pp/Fejleszt/waver/build-wp_localsource-Android_for_armeabi_v7a_GCC_4_9_Qt_5_7_0-Release/android-build/libs/armeabi-v7a/libwp_localsource.so
        ANDROID_EXTRA_LIBS += /home/pp/Fejleszt/waver/build-wp_androiddecoder-Android_for_armeabi_v7a_GCC_4_9_Qt_5_7_0-Debug/android-build/libs/armeabi-v7a/libwp_androiddecoder.so
        ANDROID_EXTRA_LIBS += /home/pp/Fejleszt/waver/build-wp_soundoutput-Android_for_armeabi_v7a_GCC_4_9_Qt_5_7_0-Release/android-build/libs/armeabi-v7a/libwp_soundoutput.so
    }
}

