EXTRA_INCLUDES =
EXTRA_LIBS =
EXTRA_FLAGS =

if XRDP_DEBUG
EXTRA_DEFINES = -DXRDP_DEBUG
else
EXTRA_DEFINES = -DXRDP_NODEBUG
endif

if XRDP_RFXCODEC
EXTRA_DEFINES += -DXRDP_RFXCODEC
EXTRA_INCLUDES += -I$(top_srcdir)/librfxcodec/include
EXTRA_LIBS += $(top_srcdir)/librfxcodec/src/librfxencode.a
endif

#if XRDP_LIBPAINTER
EXTRA_DEFINES += -DXRDP_LIBPAINTER
EXTRA_INCLUDES += -I$(top_srcdir)/libpainter/include
EXTRA_LIBS += $(top_srcdir)/libpainter/src/libpainter.la
#endif

if XRDP_PIXMAN
EXTRA_DEFINES += -DXRDP_PIXMAN
EXTRA_INCLUDES += $(XRDP_PIXMAN_CFLAGS)
EXTRA_LIBS += $(XRDP_PIXMAN_LIBS)
endif

AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -DXRDP_MODULE_PATH=\"${moduledir}\" \
  $(EXTRA_DEFINES) \
  -I$(top_builddir) \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/libxrdp \
  $(EXTRA_INCLUDES)

sbin_PROGRAMS = \
  xrdp

xrdp_SOURCES = \
  funcs.c \
  lang.c \
  xrdp.c \
  xrdp.h \
  xrdp_bitmap.c \
  xrdp_cache.c \
  xrdp_encoder.c \
  xrdp_encoder.h \
  xrdp_font.c \
  xrdp_listen.c \
  xrdp_login_wnd.c \
  xrdp_mm.c \
  xrdp_painter.c \
  xrdp_process.c \
  xrdp_region.c \
  xrdp_types.h \
  xrdp_wm.c

xrdp_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(top_builddir)/libxrdp/libxrdp.la \
  $(EXTRA_LIBS)

xrdp_LDFLAGS = \
  $(EXTRA_FLAGS)

xrdpsysconfdir=$(sysconfdir)/xrdp

dist_xrdpsysconf_DATA = \
  xrdp.ini \
  xrdp_keyboard.ini

xrdppkgdatadir=$(datadir)/xrdp

dist_xrdppkgdata_DATA = \
  ad24b.bmp \
  ad256.bmp \
  xrdp24b.bmp \
  xrdp256.bmp \
  xrdp_logo.bmp \
  sans-10.fv1 \
  cursor0.cur \
  cursor1.cur
