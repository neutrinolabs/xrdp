lib_LTLIBRARIES = \
  libcommon.la

libcommon_la_SOURCES = \
  d3des.c \
  file.c \
  list.c \
  log.c \
  os_calls.c \
  ssl_calls.c \
  thread_calls.c

libcommon_la_LIBADD = \
  -lcrypto \
  -lpthread

libcommon_la_LDFLAGS = \
  -version-info @XRDP_VERSION_INFO@
