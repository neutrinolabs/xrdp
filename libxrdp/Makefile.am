EXTRA_DIST = libxrdp.h libxrdpinc.h

EXTRA_DEFINES =
EXTRA_INCLUDES =
EXTRA_LIBS =
EXTRA_FLAGS =

if XRDP_DEBUG
EXTRA_DEFINES += -DXRDP_DEBUG
else
EXTRA_DEFINES += -DXRDP_NODEBUG
endif

if XRDP_FREERDP1
EXTRA_DEFINES += -DXRDP_FREERDP1
EXTRA_LIBS += -lfreerdp-codec -lfreerdp-utils
endif

if XRDP_JPEG
EXTRA_DEFINES += -DXRDP_JPEG
EXTRA_LIBS += -ljpeg
endif

if GOT_PREFIX
EXTRA_INCLUDES += -I$(prefix)/include
EXTRA_FLAGS += -L$(prefix)/lib -Wl,-rpath -Wl,$(prefix)/lib
endif

AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  $(EXTRA_DEFINES)

INCLUDES = \
  -I$(top_srcdir)/common \
  $(EXTRA_INCLUDES)

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
  xrdp_bitmap_compress.c \
  xrdp_jpeg_compress.c \
  xrdp_orders_rail.c

libxrdp_la_LDFLAGS = \
  $(EXTRA_FLAGS)

libxrdp_la_LIBADD = \
  $(top_builddir)/common/libcommon.la \
  $(EXTRA_LIBS)
