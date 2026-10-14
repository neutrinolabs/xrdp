AM_CPPFLAGS = \
  -I$(top_builddir) \
  -I$(top_srcdir)/wlup \
  -I$(top_srcdir)/common

if XRDP_WLUP_MUTTER
AM_CPPFLAGS += -DXRDP_WLUP_MUTTER
endif

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
                  $(top_srcdir)/tap-driver.sh

PACKAGE_STRING="wlup module"

TESTS = test_wlup
check_PROGRAMS = test_wlup

# The module core as built for libwlup, with a mock backend instead of
# the real ones
test_wlup_SOURCES = \
    test_wlup.h \
    test_wlup_main.c \
    test_wlup_core.c \
    test_wlup_gfx.c \
    mock_backend.c

test_wlup_CFLAGS = \
    @CHECK_CFLAGS@

test_wlup_LDADD = \
    $(top_builddir)/wlup/wlup.lo \
    $(top_builddir)/wlup/wlup_gfx.lo \
    $(top_builddir)/wlup/wlup_clip.lo \
    $(top_builddir)/common/libcommon.la \
    @CHECK_LIBS@
