QT += core gui qml quick quickcontrols2 multimedia dbus concurrent network

CONFIG += c++17 release
TARGET = cctv
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/rtspplaybackproxy.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/rtspplaybackproxy.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
