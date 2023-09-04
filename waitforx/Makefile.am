pkglibexec_PROGRAMS = \
  waitforx

AM_LDFLAGS = $(X_LIBS) -lX11 -lXrandr

AM_CPPFLAGS = \
  -I$(top_srcdir)/sesman/sesexec \
  -I$(top_srcdir)/common

AM_CFLAGS = $(X_CFLAGS)

waitforx_SOURCES = waitforx.c

waitforx_LDADD = \
  $(top_builddir)/common/libcommon.la
