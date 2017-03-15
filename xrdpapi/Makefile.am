EXTRA_DIST = \
  simple.c \
  vrplayer.c \
  vrplayer.mk

AM_CPPFLAGS = \
  -DXRDP_SOCKET_PATH=\"${socketdir}\" \
  -I$(top_srcdir)/common

module_LTLIBRARIES = \
  libxrdpapi.la

libxrdpapi_la_SOURCES = \
  xrdpapi.c \
  xrdpapi.h
