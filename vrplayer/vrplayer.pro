#-------------------------------------------------
#
# Project created by QtCreator 2012-11-13T11:52:36
#
#-------------------------------------------------

QT       += core gui

TARGET = vrplayer
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    decoder.cpp \
    decoderthread.cpp

HEADERS  += mainwindow.h \
    decoder.h \
    decoderthread.h

FORMS    += mainwindow.ui

# added by LK
INCLUDEPATH += ../xrdpvr
INCLUDEPATH += ../xrdpapi

LIBS += -L../xrdpvr/.libs -lxrdpvr
LIBS += -L../xrdpapi/.libs -lxrdpapi
LIBS += -L/usr/lib/x86_64-linux-gnu -lavformat -lavcodec -lavutil
