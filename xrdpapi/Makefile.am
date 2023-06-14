EXTRA_DIST = \
  simple.c \
  vrplayer.c \
  vrplayer.mk

AM_CPPFLAGS = \
  -DXRDP_SOCKET_ROOT_PATH=\"${socketdir}\" \
  -I$(top_srcdir)/common

module_LTLIBRARIES = \
  libxrdpapi.la

libxrdpapi_la_SOURCES = \
  xrdpapi.c \
  xrdpapi.h

libxrdpapi_la_LIBADD = \
  $(top_builddir)/common/libcommon.la

# Build the 'simple' example program, so it's added to the CI
noinst_PROGRAMS = xrdp-xrdpapi-simple

xrdp_xrdpapi_simple_SOURCES = \
  simple.c

# If you change this, update the standalone build instructions in simple.c
xrdp_xrdpapi_simple_LDADD = \
  libxrdpapi.la \
  $(top_builddir)/common/libcommon.la
