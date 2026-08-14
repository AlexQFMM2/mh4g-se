QT += widgets

TEMPLATE = app
TARGET = MH4GSaveEditor
CONFIG += c++17

DESTDIR = $$PWD/bin
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc
RCC_DIR = $$PWD/build/rcc

INCLUDEPATH += $$PWD/src $$PWD/src/mh3u-ui $$PWD/src/mh3u-ui/widget

SOURCES += \
    src/mh4g.cpp \
    src/mh4g_equipment_values.cpp \
    src/mh4g_transfer.cpp \
    src/mh4g_ui_compat.cpp \
    src/mh3u-ui/main.cpp \
    src/mh3u-ui/mh3u_sv.cpp \
    src/mh3u-ui/widget/qarmor.cpp \
    src/mh3u-ui/widget/qbox.cpp \
    src/mh3u-ui/widget/qcharacter.cpp \
    src/mh3u-ui/widget/qcharm.cpp \
    src/mh3u-ui/widget/qchest.cpp \
    src/mh3u-ui/widget/qequipment.cpp \
    src/mh3u-ui/widget/qitem.cpp \
    src/mh3u-ui/widget/qoption.cpp \
    src/mh3u-ui/widget/qweapon.cpp

HEADERS += \
    src/mh4g.hpp \
    src/mh4g_equipment_values.hpp \
    src/mh4g_transfer.hpp \
    src/mh4g_ui_compat.hpp \
    src/mh3u-ui/main.hpp \
    src/mh3u-ui/mh3u_sv.hpp \
    src/mh3u-ui/widget/qarmor.hpp \
    src/mh3u-ui/widget/qbox.hpp \
    src/mh3u-ui/widget/qcharacter.hpp \
    src/mh3u-ui/widget/qcharm.hpp \
    src/mh3u-ui/widget/qchest.hpp \
    src/mh3u-ui/widget/qequipment.hpp \
    src/mh3u-ui/widget/qitem.hpp \
    src/mh3u-ui/widget/qoption.hpp \
    src/mh3u-ui/widget/qweapon.hpp

RESOURCES += resources/resources.qrc

win32: LIBS += -lcrypto
unix:!macx: LIBS += -l:libcrypto.so.3
macx: LIBS += -lcrypto
