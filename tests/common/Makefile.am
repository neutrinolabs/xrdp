
AM_CPPFLAGS = \
  -I$(top_builddir) \
  -I$(top_srcdir)/common

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
                  $(top_srcdir)/tap-driver.sh

PACKAGE_STRING = "libcommon"

TESTS = test_common
check_PROGRAMS = test_common

test_common_SOURCES = \
    test_common.h \
    test_common_main.c \
    test_fifo_calls.c \
    test_list_calls.c \
    test_parse.c \
    test_string_calls.c \
    test_string_calls_unicode.c \
    test_os_calls.c \
    test_os_calls_signals.c \
    test_ssl_calls.c \
    test_base64.c \
    test_guid.c

test_common_CFLAGS = \
    @CHECK_CFLAGS@ \
    -D TOP_SRCDIR=\"$(top_srcdir)\"

test_common_LDADD = \
    $(top_builddir)/common/libcommon.la \
    @CHECK_LIBS@
