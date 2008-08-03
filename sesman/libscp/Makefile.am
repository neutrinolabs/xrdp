INCLUDES = \
  -I$(top_srcdir)/common

lib_LTLIBRARIES = \
  libscp.la

libscp_la_SOURCES = \
  libscp_connection.c \
  libscp_init.c \
  libscp_lock.c \
  libscp_session.c \
  libscp_tcp.c \
  libscp_v0.c \
  libscp_v1c.c \
  libscp_v1s.c \
  libscp_vX.c

libscp_la_LDFLAGS = \
  -version-info 5:0:5
