AM_CPPFLAGS = \
  -I$(top_builddir) \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/sesman/chansrv

check_PROGRAMS = test_clipboard_overlap
test_clipboard_overlap_SOURCES = test_clipboard_overlap.c
test_clipboard_overlap_CFLAGS = -ffunction-sections -fdata-sections
test_clipboard_overlap_LDFLAGS = $(X_LIBS) -Wl,--gc-sections
test_clipboard_overlap_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(X_PRE_LIBS) -lX11 -lXfixes $(X_EXTRA_LIBS)

TESTS = test_clipboard_overlap.sh
EXTRA_DIST = test_clipboard_overlap.sh readme.txt
