INCLUDES = \
  -I$(top_srcdir)/common

lib_LTLIBRARIES = \
  libxrdp.la

libxrdp_la_SOURCES = \
  libxrdp.c \
  xrdp_channel.c \
  xrdp_iso.c \
  xrdp_mcs.c \
  xrdp_orders.c \
  xrdp_rdp.c \
  xrdp_sec.c \
  xrdp_tcp.c \
  xrdp_bitmap_compress.c

libxrdp_la_LIBADD = \
  $(top_srcdir)/common/libcommon.la
