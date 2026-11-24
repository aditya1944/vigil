QT += core gui quick quickcontrols2 testlib dbus multimedia concurrent network
CONFIG += c++17 testcase
TARGET = cctv_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/rtspplaybackproxy.h ../src/theme.h ../src/translator.h
SOURCES += cctv_tests.cpp ../src/backend.cpp ../src/rtspplaybackproxy.cpp ../src/theme.cpp ../src/translator.cpp
