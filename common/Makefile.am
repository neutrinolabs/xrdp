
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\"

lib_LTLIBRARIES = \
  libcommon.la

libcommon_la_SOURCES = \
  d3des.c \
  file.c \
  list.c \
  log.c \
  os_calls.c \
  ssl_calls.c \
  thread_calls.c \
  trans.c

libcommon_la_LIBADD = \
  -lcrypto \
  -lpthread
