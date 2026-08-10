QT += widgets

TEMPLATE = app
TARGET = MH4GSaveEditor
CONFIG += c++17

DESTDIR = $$PWD/bin
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc

INCLUDEPATH += $$PWD/src

SOURCES += \
    src/main.cpp \
    src/mh4g.cpp \
    src/mainwindow.cpp

HEADERS += \
    src/mh4g.hpp \
    src/mainwindow.hpp

win32: LIBS += -lcrypto
unix:!macx: LIBS += -l:libcrypto.so.3
macx: LIBS += -lcrypto
