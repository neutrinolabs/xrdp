
AM_CPPFLAGS = \
  -I$(top_builddir) \
  -I$(top_srcdir)/common

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
                  $(top_srcdir)/tap-driver.sh

TESTS = test_common
check_PROGRAMS = test_common

test_common_SOURCES = \
    test_common.h \
    test_common_main.c \
    test_string_calls.c

test_common_CFLAGS = \
    @CHECK_CFLAGS@

test_common_LDADD = \
    $(top_builddir)/common/libcommon.la \
    @CHECK_LIBS@
