QT += core sql
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_loadout_core
INCLUDEPATH += ../src
SOURCES += test_loadout_core.cpp ../src/game_data_repository.cpp ../src/equipment_validator.cpp ../src/loadout.cpp
HEADERS += ../src/game_data_repository.hpp ../src/equipment_validator.hpp ../src/loadout.hpp ../src/mh4g_ui_compat.hpp
