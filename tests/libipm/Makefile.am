
AM_CPPFLAGS = \
  -I$(top_builddir) \
  -I$(top_srcdir)/libipm \
  -I$(top_srcdir)/common

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
                  $(top_srcdir)/tap-driver.sh

PACKAGE_STRING = "libipm"

TESTS = test_libipm
check_PROGRAMS = test_libipm

test_libipm_SOURCES = \
    test_libipm_main.c \
    test_libipm.h \
    test_libipm_send_calls.c \
    test_libipm_recv_calls.c

test_libipm_CFLAGS = \
    @CHECK_CFLAGS@ \
    -D TOP_SRCDIR=\"$(top_srcdir)\"

test_libipm_LDADD = \
    $(top_builddir)/libipm/libipm.la \
    $(top_builddir)/common/libcommon.la \
    @CHECK_LIBS@
