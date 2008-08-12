INCLUDES = \
  -I$(top_srcdir)/common

lib_LTLIBRARIES = \
  librdp.la

librdp_la_SOURCES = \
  rdp.c \
  rdp_bitmap.c \
  rdp_iso.c \
  rdp_lic.c \
  rdp_mcs.c \
  rdp_orders.c \
  rdp_rdp.c \
  rdp_sec.c \
  rdp_tcp.c

librdp_la_LIBADD = \
  $(top_srcdir)/common/libcommon.la

librdp_la_LDFLAGS = \
  -version-info @XRDP_VERSION_INFO@
