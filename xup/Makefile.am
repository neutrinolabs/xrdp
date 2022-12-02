AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -I$(top_srcdir)/common

module_LTLIBRARIES = \
  libxup.la

libxup_la_SOURCES = \
  xup.c \
  xup.h

libxup_la_LIBADD = \
  $(top_builddir)/common/libcommon.la

if !MACOS
libxup_la_LDFLAGS = -avoid-version -module
endif
