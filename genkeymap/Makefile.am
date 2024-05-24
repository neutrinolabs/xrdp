EXTRA_DIST = \
  dump-keymaps.sh \
  readme.txt

AM_CPPFLAGS = \
  -I$(top_srcdir)/common

AM_CFLAGS = $(X_CFLAGS)

bin_PROGRAMS = \
  xrdp-genkeymap

xrdp_genkeymap_SOURCES = genkeymap.c

xrdp_genkeymap_LDFLAGS = \
  $(X_LIBS)

xrdp_genkeymap_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(X_PRE_LIBS) -lxkbfile -lX11 $(X_EXTRA_LIBS)
