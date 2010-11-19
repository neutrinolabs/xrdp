EXTRA_DIST = libxrdp.h libxrdpinc.h

if XRDP_DEBUG
EXTRA_DEFINES = -DXRDP_DEBUG
else
EXTRA_DEFINES = -DXRDP_NODEBUG
endif

AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  $(EXTRA_DEFINES)

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
