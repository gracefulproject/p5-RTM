TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    chipFwdr.c \
    init.c \
    eHandlers.c

INCLUDEPATH += /opt/spinnaker_tools_3.1.0/include

DISTFILES += \
    Makefile

HEADERS += \
    chipFwdr.h

