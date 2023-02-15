AM_CPPFLAGS = \
  -I$(top_builddir) \
  -I$(top_srcdir)/xrdp \
  -I$(top_srcdir)/libxrdp \
  -I$(top_srcdir)/common \
  $(IMLIB2_CFLAGS)

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
                  $(top_srcdir)/tap-driver.sh

PACKAGE_STRING="XRDP daemon"

EXTRA_DIST = \
  test_4bit.bmp \
  test_8bit.bmp \
  test_24bit.bmp \
  test_not4_4bit.bmp \
  test_not4_8bit.bmp \
  test_not4_24bit.bmp \
  test1.jpg \
  test_alpha_blend.png

TESTS = test_xrdp
check_PROGRAMS = test_xrdp

test_xrdp_SOURCES = \
    test_xrdp.h \
    test_xrdp_main.c \
    test_xrdp_egfx.c \
    test_xrdp_region.c \
    test_bitmap_load.c

test_xrdp_CFLAGS = \
    -D IMAGEDIR=\"$(srcdir)\" \
    @CHECK_CFLAGS@ \
    @CMOCKA_CFLAGS@

test_xrdp_LDFLAGS = -Wl,--wrap=pixman_region_extents, \
    -Wl,--wrap=pixman_region_not_empty

test_xrdp_LDADD = \
    $(top_builddir)/xrdp/xrdp_bitmap_load.o \
    $(top_builddir)/xrdp/xrdp_bitmap_common.o \
    $(top_builddir)/xrdp/funcs.o \
    $(top_builddir)/common/libcommon.la \
    $(top_builddir)/libipm/libipm.la \
    $(top_builddir)/libxrdp/libxrdp.la \
    $(top_builddir)/libpainter/src/libpainter.la \
    $(top_builddir)/librfxcodec/src/librfxencode.la \
    $(top_builddir)/xrdp/lang.o \
    $(top_builddir)/xrdp/xrdp_mm.o \
    $(top_builddir)/xrdp/xrdp_wm.o \
    $(top_builddir)/xrdp/xrdp_font.o \
    $(top_builddir)/xrdp/xrdp_egfx.o \
    $(top_builddir)/xrdp/xrdp_cache.o \
    $(top_builddir)/xrdp/xrdp_region.o \
    $(top_builddir)/xrdp/xrdp_listen.o \
    $(top_builddir)/xrdp/xrdp_bitmap.o \
    $(top_builddir)/xrdp/xrdp_painter.o \
    $(top_builddir)/xrdp/xrdp_encoder.o \
    $(top_builddir)/xrdp/xrdp_process.o \
    $(top_builddir)/xrdp/xrdp_login_wnd.o \
    $(top_builddir)/xrdp/xrdp_main_utils.o \
    $(PIXMAN_LIBS) \
    $(IMLIB2_LIBS) \
    @CHECK_LIBS@ \
    @CMOCKA_LIBS@
