QT -= gui
QT += core

TEMPLATE = app
TARGET = test_save_core
CONFIG += console c++17
CONFIG -= app_bundle

INCLUDEPATH += ../src
SOURCES += test_save_core.cpp ../src/mh4g.cpp ../src/mh4g_equipment_values.cpp ../src/mh4g_transfer.cpp ../src/mh4g_ui_compat.cpp
HEADERS += ../src/mh4g.hpp ../src/mh4g_equipment_values.hpp ../src/mh4g_transfer.hpp ../src/mh4g_ui_compat.hpp

win32: LIBS += -lcrypto
unix:!macx: LIBS += -l:libcrypto.so.3
macx: LIBS += -lcrypto
