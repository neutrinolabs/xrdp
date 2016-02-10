EXTRA_DIST = rdp.h
EXTRA_DEFINES =
 
if XRDP_DEBUG
EXTRA_DEFINES += -DXRDP_DEBUG
else
EXTRA_DEFINES += -DXRDP_NODEBUG
endif

AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  $(EXTRA_DEFINES) \
  -I$(top_srcdir)/common

module_LTLIBRARIES = \
  librdp.la

librdp_la_SOURCES = \
  rdp.c \
  rdp_bitmap.c \
  rdp_iso.c \
  rdp_lic.c \
  rdp_mcs.c \
  rdp_orders.c \
  rdp_rdp.c \
  rdp_sec.c \
  rdp_tcp.c

librdp_la_LIBADD = \
  $(top_builddir)/common/libcommon.la
