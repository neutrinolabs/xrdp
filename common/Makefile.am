EXTRA_DIST = d3des.h arch.h os_calls.h list.h file.h parse.h defines.h file_loc.h log.h os_calls.h ssl_calls.h thread_calls.h trans.h xrdp_constants.h


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
  log.c \
  os_calls.c \
  ssl_calls.c \
  thread_calls.c \
  trans.c

libcommon_la_LIBADD = \
  -lcrypto \
  -lssl \
  -lpthread
