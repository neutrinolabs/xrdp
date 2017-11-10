if XRDP_PIXMAN
  PIXMAN_SOURCES =
else
  PIXMAN_SOURCES = pixman-region16.c pixman-region.h
endif

EXTRA_DIST = pixman-region.c

include_HEADERS = \
  xrdp_client_info.h \
  xrdp_constants.h \
  xrdp_rail.h \
  xrdp_sockets.h

AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -DXRDP_LOG_PATH=\"${localstatedir}/log\" \
  -DXRDP_SOCKET_PATH=\"${socketdir}\"

if XRDP_DEBUG
AM_CPPFLAGS += -DXRDP_DEBUG
endif

AM_CFLAGS = $(OPENSSL_CFLAGS)

module_LTLIBRARIES = \
  libcommon.la

libcommon_la_SOURCES = \
  arch.h \
  base64.h \
  base64.c \
  defines.h \
  fifo.c \
  fifo.h \
  file.c \
  file.h \
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
  $(PIXMAN_SOURCES)

libcommon_la_LIBADD = \
  -lpthread \
  $(OPENSSL_LIBS) \
  $(DLOPEN_LIBS)
