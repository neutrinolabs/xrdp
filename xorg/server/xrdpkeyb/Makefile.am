EXTRA_DIST =

AM_CFLAGS = \
  $(XORG_SERVER_CFLAGS) \
  -I../module \
  -I../../../common

xrdpkeyb_drv_la_LTLIBRARIES = xrdpkeyb_drv.la

xrdpkeyb_drv_la_LDFLAGS = -module -avoid-version

xrdpkeyb_drv_ladir = $(moduledir)/input

xrdpkeyb_drv_la_SOURCES = rdpKeyboard.c

xrdpkeyb_drv_la_LIBADD =
