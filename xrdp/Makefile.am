EXTRA_DIST = xrdp.ini ad24b.bmp ad256.bmp xrdp24b.bmp xrdp256.bmp sans-10.fv1 cursor0.cur cursor1.cur xrdp.h xrdp_types.h

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
  -DXRDP_LIB_PATH=\"${libdir}\" \
  $(EXTRA_DEFINES)

INCLUDES = \
  -I$(top_builddir) \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/libxrdp

sbin_PROGRAMS = \
  xrdp

xrdp_SOURCES = \
  funcs.c \
  lang.c \
  xrdp_bitmap.c \
  xrdp.c \
  xrdp_cache.c \
  xrdp_font.c \
  xrdp_listen.c \
  xrdp_login_wnd.c \
  xrdp_mm.c \
  xrdp_painter.c \
  xrdp_process.c \
  xrdp_region.c \
  xrdp_wm.c

xrdp_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(top_builddir)/libxrdp/libxrdp.la

xrdpsysconfdir=$(sysconfdir)/xrdp

xrdpsysconf_DATA = \
  xrdp.ini 

xrdppkgdatadir=$(datadir)/xrdp

xrdppkgdata_DATA = \
  ad24b.bmp \
  ad256.bmp \
  xrdp24b.bmp \
  xrdp256.bmp \
  sans-10.fv1 \
  cursor0.cur \
  cursor1.cur

