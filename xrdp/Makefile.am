INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/libxrdp

bin_PROGRAMS = \
  xrdp

xrdp_SOURCES = \
  funcs.c \
  lang.c \
  xrdp_bitmap.c \
  xrdp.c \
  xrdp_cache.c \
  xrdp_font.c \
  xrdp_listen.c \
  xrdp_login_wnd.c \
  xrdp_mm.c \
  xrdp_painter.c \
  xrdp_process.c \
  xrdp_region.c \
  xrdp_wm.c

xrdp_LDADD = \
  $(top_srcdir)/common/libxrdp-common.a \
  $(top_srcdir)/libxrdp/libxrdp.la \
  -ldl \
  -lpthread \
  -lcrypto
