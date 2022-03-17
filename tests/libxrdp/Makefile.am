AM_CPPFLAGS = \
  -I$(top_builddir) \
  -I$(top_srcdir)/libxrdp \
  -I$(top_srcdir)/common

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
                  $(top_srcdir)/tap-driver.sh

PACKAGE_STRING = "libxrdp"

TESTS = test_libxrdp
check_PROGRAMS = test_libxrdp

test_libxrdp_SOURCES = \
    test_libxrdp.h \
    test_libxrdp_main.c \
    test_monitor_processing.c

test_libxrdp_CFLAGS = \
    @CHECK_CFLAGS@

test_libxrdp_LDADD = \
    $(top_builddir)/common/libcommon.la \
    $(top_builddir)/libxrdp/libxrdp.la \
    @CHECK_LIBS@
