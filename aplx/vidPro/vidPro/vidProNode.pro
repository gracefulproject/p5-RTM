TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    master/mVidPro.c \
    worker/wVidPro.c \
    master/mInit.c \
    worker/wInit.c \
    master/mHandlers.c \
    worker/wHandlers.c

INCLUDEPATH += /opt/spinnaker_tools_3.1.0/include

DISTFILES += \
    Makefile \ 
    README

HEADERS += \
    vidProNode.h \
    master/mVidPro.h \
    worker/wVidPro.h
