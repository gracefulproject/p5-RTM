TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    mSpinJPEGenc.c \
    eHandlers.c \  
    encoder.c \
    huff.c

INCLUDEPATH += /opt/spinnaker_tools_3.1.0/include \
    ../../

DISTFILES += \
    Makefile \
    README \
    compile

HEADERS += \
    mSpinJPEGenc.h \
    conf.h
