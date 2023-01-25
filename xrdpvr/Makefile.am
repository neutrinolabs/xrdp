AM_CPPFLAGS = \
  -I$(top_srcdir)/xrdpapi \
  $(LIBAV_CFLAGS)

module_LTLIBRARIES = \
  libxrdpvr.la

libxrdpvr_la_SOURCES = \
  xrdpvr.c \
  xrdpvr.h \
  xrdpvr_internal.h

libxrdpvr_la_LIBADD = \
  $(LIBAV_LIBS)
