pkglibexec_PROGRAMS = \
  waitforx

AM_LDFLAGS = -lX11 -lXrandr

AM_CPPFLAGS = \
  -I$(top_srcdir)/sesman \
  -I$(top_srcdir)/common

AM_CFLAGS = $(X_CFLAGS)

waitforx_SOURCES = waitforx.c

waitforx_LDADD = \
  $(top_builddir)/common/libcommon.la
