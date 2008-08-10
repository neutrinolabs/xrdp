INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/sesman/libscp

if SESMAN_NOPAM
AUTH_C = verify_user.c
AUTH_LIB = -lcrypt
else
if SESMAN_PAMUSERPASS
AUTH_C = verify_user_pam_userpass.c
AUTH_LIB = -lpam -lpam_userpass
else
if SESMAN_KERBEROS
AUTH_C = verify_user_kerberos.c
AUTH_LIB = -lkrb5
else
AUTH_C = verify_user_pam.c
AUTH_LIB = -lpam
endif
endif
endif

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
  lock.c \
  access.c \
  config.c \
  env.c \
  $(AUTH_C)

xrdp_sessvc_SOURCES = \
  sessvc.c

xrdp_sesman_LDADD = \
  $(top_srcdir)/common/libxrdp-common.la \
  $(top_srcdir)/sesman/libscp/libscp.la \
  $(AUTH_LIB)

xrdp_sessvc_LDADD = \
  $(top_srcdir)/common/libxrdp-common.la

SUBDIRS = \
  libscp \
  tools
