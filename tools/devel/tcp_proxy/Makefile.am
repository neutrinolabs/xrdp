
AM_CPPFLAGS = \
  -I$(top_srcdir)/common

noinst_PROGRAMS = \
  tcp_proxy

tcp_proxy_SOURCES = \
  main.c

tcp_proxy_LDADD = \
  $(top_builddir)/common/libcommon.la \
  -ldl
