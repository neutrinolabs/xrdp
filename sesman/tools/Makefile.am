INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/sesman/libscp \
  -I$(top_srcdir)/sesman

bin_PROGRAMS = \
  xrdp-sesrun \
  xrdp-sestest

xrdp_sesrun_SOURCES = \
  sesrun.c \
  tcp.c \
  ../config.c

xrdp_sestest_SOURCES = \
  sestest.c

xrdp_sesrun_LDADD = \
  $(top_srcdir)/common/libcommon.la

xrdp_sestest_LDADD = \
  $(top_srcdir)/common/libcommon.la \
  $(top_srcdir)/sesman/libscp/libscp.la
