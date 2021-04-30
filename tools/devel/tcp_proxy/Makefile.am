
AM_CPPFLAGS = \
  -I$(top_srcdir)/common

if XRDP_DEBUG
AM_CPPFLAGS += -DXRDP_DEBUG
endif

noinst_PROGRAMS = \
  tcp_proxy

tcp_proxy_SOURCES = \
  main.c

tcp_proxy_LDADD = \
  $(top_builddir)/common/libcommon.la \
  -ldl
