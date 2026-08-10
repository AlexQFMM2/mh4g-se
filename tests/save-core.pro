QT -= gui
QT += core

TEMPLATE = app
TARGET = test_save_core
CONFIG += console c++17
CONFIG -= app_bundle

INCLUDEPATH += ../src
SOURCES += test_save_core.cpp ../src/mh4g.cpp
HEADERS += ../src/mh4g.hpp

win32: LIBS += -lcrypto
unix:!macx: LIBS += -l:libcrypto.so.3
macx: LIBS += -lcrypto
