EXTRA_DIST = vnc.h

AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\"

INCLUDES = \
  -I$(top_srcdir)/common

lib_LTLIBRARIES = \
  libvnc.la

libvnc_la_SOURCES = vnc.c

libvnc_la_LIBADD = \
  $(top_builddir)/common/libcommon.la
