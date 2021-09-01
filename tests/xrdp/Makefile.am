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
    test_bitmap_load.c

test_xrdp_CFLAGS = \
    -D IMAGEDIR=\"$(srcdir)\" \
    @CHECK_CFLAGS@

test_xrdp_LDADD = \
    $(top_builddir)/xrdp/xrdp_bitmap_load.o \
    $(top_builddir)/xrdp/xrdp_bitmap_common.o \
    $(top_builddir)/xrdp/funcs.o \
    $(top_builddir)/common/libcommon.la \
    $(IMLIB2_LIBS) \
    @CHECK_LIBS@
