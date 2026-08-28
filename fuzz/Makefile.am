AM_CPPFLAGS = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/libxrdp

AM_LDFLAGS = \
  -fsanitize=fuzzer \
  -fsanitize=memory

bin_PROGRAMS = \
  fuzz_xrdp_caps_process_confirm_active

fuzz_xrdp_caps_process_confirm_active_SOURCES = \
  fuzz_common.c \
  fuzz_xrdp_caps_process_confirm_active.c

fuzz_xrdp_caps_process_confirm_active_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(top_builddir)/libxrdp/libxrdp.la
