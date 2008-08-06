INCLUDES = \
  -I$(top_srcdir)/common

lib_LTLIBRARIES = \
  libvnc.la

libvnc_la_SOURCES = vnc.c

libvnc_la_LIBADD = \
  $(top_srcdir)/common/libxrdp-common.la

libvnc_la_LDFLAGS = \
  -version-info 5:0:5
