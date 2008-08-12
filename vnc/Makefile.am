INCLUDES = \
  -I$(top_srcdir)/common

lib_LTLIBRARIES = \
  libvnc.la

libvnc_la_SOURCES = vnc.c

libvnc_la_LIBADD = \
  $(top_srcdir)/common/libcommon.la

libvnc_la_LDFLAGS = \
  -version-info @XRDP_VERSION_INFO@
