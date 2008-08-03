INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/sesman/libscp

bin_PROGRAMS = \
  xrdp-sesman \
  xrdp-sessvc

xrdp_sesman_SOURCES = \
  scp.c \
  scp_v0.c \
  scp_v1.c \
  sesman.c \
  session.c \
  sig.c \
  thread.c \
  verify_user_pam.c \
  lock.c \
  access.c \
  config.c \
  env.c

xrdp_sessvc_SOURCES = \
  sessvc.c

xrdp_sesman_LDADD = \
  $(top_srcdir)/common/libxrdp-common.a \
  $(top_srcdir)/sesman/libscp/libscp.la \
  -ldl \
  -lpthread \
  -lpam

xrdp_sessvc_LDADD = \
  $(top_srcdir)/common/libxrdp-common.a \
  -ldl

SUBDIRS = libscp
