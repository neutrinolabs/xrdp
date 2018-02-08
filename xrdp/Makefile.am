EXTRA_DIST = \
  czech.txt \
  rdp-scan-codes.txt \
  xrdpwin.c

AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -DXRDP_MODULE_PATH=\"${moduledir}\" \
  -DXRDP_SOCKET_PATH=\"${socketdir}\" \
  -I$(top_builddir) \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/libxrdp

XRDP_EXTRA_LIBS =

if XRDP_DEBUG
AM_CPPFLAGS += -DXRDP_DEBUG
endif

if XRDP_RFXCODEC
AM_CPPFLAGS += -DXRDP_RFXCODEC
AM_CPPFLAGS += -I$(top_srcdir)/librfxcodec/include
XRDP_EXTRA_LIBS += $(top_builddir)/librfxcodec/src/.libs/librfxencode.a
endif

if XRDP_PIXMAN
AM_CPPFLAGS += -DXRDP_PIXMAN
AM_CPPFLAGS += $(PIXMAN_CFLAGS)
XRDP_EXTRA_LIBS += $(PIXMAN_LIBS)
endif

if XRDP_PAINTER
AM_CPPFLAGS += -DXRDP_PAINTER
AM_CPPFLAGS += -I$(top_srcdir)/libpainter/include
XRDP_EXTRA_LIBS += $(top_builddir)/libpainter/src/.libs/libpainter.a
endif

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
  $(XRDP_EXTRA_LIBS)

xrdpsysconfdir=$(sysconfdir)/xrdp

if MACOS
lib_extension = dylib
else
lib_extension = so
endif

SUBST_VARS = sed \
   -e 's|@lib_extension[@]|$(lib_extension)|g'

subst_verbose = $(subst_verbose_@AM_V@)
subst_verbose_ = $(subst_verbose_@AM_DEFAULT_V@)
subst_verbose_0 = @echo "  SUBST    $@";

SUFFIXES = .in
.in:
	$(subst_verbose)$(SUBST_VARS) $< > $@

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
