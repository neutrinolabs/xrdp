EXTRA_DIST = \
  dump-keymaps.sh \
  readme.txt

AM_CFLAGS = $(X_CFLAGS)

bin_PROGRAMS = \
  xrdp-genkeymap

xrdp_genkeymap_SOURCES = genkeymap.c evdev-map.c

xrdp_genkeymap_LDFLAGS = \
  $(X_LIBS)

xrdp_genkeymap_LDADD = \
  $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS)
