EXTRA_DIST = \
  arch.h \
  d3des.h \
  defines.h \
  file.h \
  file_loc.h \
  list.h \
  list16.h \
  fifo.h \
  log.h \
  os_calls.h \
  os_calls.h \
  parse.h \
  rail.h \
  ssl_calls.h \
  thread_calls.h \
  trans.h \
  xrdp_client_info.h \
  xrdp_constants.h \
  xrdp_rail.h \
  crc16.h

AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -DXRDP_LOG_PATH=\"${localstatedir}/log\"

lib_LTLIBRARIES = \
  libcommon.la

libcommon_la_SOURCES = \
  d3des.c \
  file.c \
  list.c \
  list16.c \
  fifo.c \
  log.c \
  os_calls.c \
  ssl_calls.c \
  thread_calls.c \
  trans.c

libcommon_la_LIBADD = \
  -lcrypto \
  -lssl \
  -lpthread
