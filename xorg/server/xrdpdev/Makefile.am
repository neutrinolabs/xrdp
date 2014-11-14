EXTRA_DIST = \
  xorg.conf

AM_CFLAGS = \
  $(XORG_SERVER_CFLAGS) \
  -I../module \
  -I../../../common

xrdpdev_drv_la_LTLIBRARIES = xrdpdev_drv.la

xrdpdev_drv_la_LDFLAGS = -module -avoid-version

xrdpdev_drv_ladir = $(moduledir)/drivers

xrdpdev_drv_la_SOURCES = xrdpdev.c

xrdpdev_drv_la_LIBADD =
