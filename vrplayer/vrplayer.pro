#-------------------------------------------------
#
# Project created by QtCreator 2012-11-13T11:52:36
#
#-------------------------------------------------

QT       += core gui

TARGET = vrplayer
TEMPLATE = app


SOURCES += main.cpp\
    playvideo.cpp \
    mainwindow.cpp \
    mediapacket.cpp \
    playaudio.cpp \
    demuxmedia.cpp \
    ourinterface.cpp \
    dlgabout.cpp

HEADERS  += mainwindow.h \
    mediapacket.h \
    playvideo.h \
    playaudio.h \
    demuxmedia.h \
    ourinterface.h \
    dlgabout.h

FORMS    += mainwindow.ui \
    dlgabout.ui

# added by LK
INCLUDEPATH += ../xrdpvr
INCLUDEPATH += ../xrdpapi

LIBS += -Wl,-rpath
LIBS += -Wl,/usr/local/lib/xrdp

LIBS += -L../xrdpvr/.libs -lxrdpvr
LIBS += -L../xrdpapi/.libs -lxrdpapi
LIBS += -L/usr/lib/x86_64-linux-gnu -lavformat -lavcodec -lavutil
