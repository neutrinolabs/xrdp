INCLUDES = \
  -I$(top_srcdir)/common \
  -DXRDP_CFG_FILE=\"/etc/xrdp/xrdp.ini\" \
  -DXRDP_KEY_FILE=\"/etc/xrdp/rsakeys.ini\"

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
  $(top_srcdir)/common/libxrdp-common.la

libxrdp_la_LDFLAGS = \
  -version-info 5:0:5
