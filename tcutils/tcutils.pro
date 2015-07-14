#-------------------------------------------------
#
# Project created by QtCreator 2013-08-18T13:54:44
#
#-------------------------------------------------

QT       += core gui

TARGET = tcutils
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    utils.cpp

HEADERS  += mainwindow.h \
    utils.h

FORMS    += mainwindow.ui

# added by LK Rashinkar
INCLUDEPATH += ../xrdpapi

LIBS += -Wl,-rpath
LIBS += -Wl,/usr/local/lib/xrdp
LIBS += -L../xrdpapi/.libs -lxrdpapi

RESOURCES += \
    resources.qrc
