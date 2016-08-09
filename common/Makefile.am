
EXTRA_FILES =

if XRDP_PIXMAN
else
  EXTRA_FILES += pixman-region16.c pixman-region.h
endif

AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -DXRDP_LOG_PATH=\"${localstatedir}/log\"

module_LTLIBRARIES = \
  libcommon.la

libcommon_la_SOURCES = \
  arch.h \
  crc16.h \
  defines.h \
  fifo.c \
  fifo.h \
  file.c \
  file.h \
  file_loc.h \
  list.c \
  list.h \
  list16.c \
  list16.h \
  log.c \
  log.h \
  os_calls.c \
  os_calls.h \
  os_calls.h \
  parse.h \
  rail.h \
  ssl_calls.c \
  ssl_calls.h \
  thread_calls.c \
  thread_calls.h \
  trans.c \
  trans.h \
  xrdp_client_info.h \
  xrdp_constants.h \
  xrdp_rail.h \
  $(EXTRA_FILES)

libcommon_la_LIBADD = \
  -lcrypto \
  -lssl \
  -lpthread
