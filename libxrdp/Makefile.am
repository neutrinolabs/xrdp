EXTRA_DIST = \
  xrdp_surface.c

AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -I$(top_srcdir)/common

AM_CFLAGS = $(OPENSSL_CFLAGS)

AM_LDFLAGS =

LIBXRDP_EXTRA_LIBS =

if XRDP_DEBUG
AM_CPPFLAGS += -DXRDP_DEBUG
endif

if XRDP_NEUTRINORDP
AM_CPPFLAGS += -DXRDP_NEUTRINORDP
LIBXRDP_EXTRA_LIBS += $(FREERDP_LIBS)
endif

if XRDP_RFXCODEC
AM_CPPFLAGS += -DXRDP_RFXCODEC
endif

if XRDP_TJPEG
AM_CPPFLAGS += -DXRDP_JPEG -DXRDP_TJPEG @TurboJpegIncDir@
AM_LDFLAGS += @TurboJpegLibDir@
LIBXRDP_EXTRA_LIBS += -lturbojpeg
else
if XRDP_JPEG
AM_CPPFLAGS += -DXRDP_JPEG
LIBXRDP_EXTRA_LIBS += -ljpeg
endif
endif

module_LTLIBRARIES = \
  libxrdp.la

libxrdp_la_SOURCES = \
  libxrdp.c \
  libxrdp.h \
  libxrdpinc.h \
  xrdp_bitmap32_compress.c \
  xrdp_bitmap_compress.c \
  xrdp_caps.c \
  xrdp_channel.c \
  xrdp_fastpath.c \
  xrdp_iso.c \
  xrdp_jpeg_compress.c \
  xrdp_mcs.c \
  xrdp_mppc_enc.c \
  xrdp_orders.c \
  xrdp_orders_rail.c \
  xrdp_orders_rail.h \
  xrdp_rdp.c \
  xrdp_sec.c

libxrdp_la_LIBADD = \
  $(top_builddir)/common/libcommon.la \
  $(LIBXRDP_EXTRA_LIBS)
