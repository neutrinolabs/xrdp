lib_LTLIBRARIES = \
  libxrdp-common.la

libxrdp_common_la_SOURCES = \
  d3des.c \
  file.c \
  list.c \
  log.c \
  os_calls.c \
  ssl_calls.c \
  thread_calls.c

libxrdp_common_la_LIBADD = \
  -lcrypto \
  -lpthread

libxrdp_common_la_LDFLAGS = \
  -version-info 5:0:5
