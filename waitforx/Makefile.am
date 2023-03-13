bin_PROGRAMS = \
  xrdp-waitforx

AM_LDFLAGS = -lX11 -lXrandr

AM_CPPFLAGS = -I$(top_srcdir)/common

AM_CFLAGS = $(X_CFLAGS)

xrdp_waitforx_SOURCES = waitforx.c

xrdp_waitforx_LDADD = \
  $(top_builddir)/common/libcommon.la
