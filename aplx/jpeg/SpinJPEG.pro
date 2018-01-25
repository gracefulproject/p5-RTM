TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    enc/mSpinJPEGenc.c \
    enc/encHandlers.c \  
    enc/encoder.c \
    enc/encHuff.c \
    dec/mSpinJPEGdec.c \
    dec/decoder.c \
    dec/huffman.c \
    dec/idct.c \
    dec/parse.c \
    dec/tree_vld.c \
    dec/utils.c \
    dec/decHandlers.c

INCLUDEPATH += /opt/spinnaker_tools_3.1.0/include \
    ./

DISTFILES += \
    enc/compile.enc \
    enc/Makefile.enc \
    dec/compile.dec \
    dec/Makefile.dec

HEADERS += \
    enc/mSpinJPEGenc.h \
    enc/conf.h \
    dec/mSpinJPEGdec.h \
    SpinJPEG.h

