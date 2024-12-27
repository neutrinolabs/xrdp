AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/${sysconfsubdir}\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -I$(top_srcdir)/common

module_LTLIBRARIES = \
  libmc.la

libmc_la_SOURCES = \
  mc.c \
  mc.h

libmc_la_LIBADD = \
  $(top_builddir)/common/libcommon.la

if !MACOS
libmc_la_LDFLAGS = -avoid-version -module
endif
