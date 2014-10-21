EXTRA_DIST =

AM_CFLAGS = \
  $(XORG_SERVER_CFLAGS) \
  -I../module \
  -I../../../common

xrdpmouse_drv_la_LTLIBRARIES = xrdpmouse_drv.la

xrdpmouse_drv_la_LDFLAGS = -module -avoid-version

xrdpmouse_drv_ladir = $(moduledir)/input

xrdpmouse_drv_la_SOURCES = rdpMouse.c

xrdpmouse_drv_la_LIBADD =
