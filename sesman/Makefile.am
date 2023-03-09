EXTRA_DIST = \
  Doxyfile

AM_CPPFLAGS = \
  -DXRDP_SYSCONF_PATH=\"${sysconfdir}\" \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_LIBEXEC_PATH=\"${libexecdir}/xrdp\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -DXRDP_SOCKET_PATH=\"${socketdir}\" \
  -DXRDP_PAMCONF_PATH=\"${pamconfdir}\" \
  -DSESMAN_RUNTIME_PATH=\"${sesmanruntimedir}\" \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/libipm

sbin_PROGRAMS = \
  xrdp-sesman

xrdp_sesman_SOURCES = \
  access.c \
  access.h \
  auth.h \
  config.c \
  config.h \
  env.c \
  env.h \
  lock_uds.c \
  lock_uds.h \
  scp_process.c \
  scp_process.h \
  sesman.c \
  sesman.h \
  session.c \
  session.h \
  session_list.c \
  session_list.h \
  sig.c \
  sig.h \
  xauth.c \
  xauth.h \
  xwait.c \
  xwait.h

# Possible authentication modules
# See https://www.gnu.org/software/automake/manual/html_node/Conditional-Sources.html
EXTRA_xrdp_sesman_SOURCES = \
 verify_user.c \
 verify_user_bsd.c \
 verify_user_kerberos.c \
 verify_user_pam.c \
 verify_user_pam_userpass.c

# Make sure the right authentication module is pulled in
xrdp_sesman_DEPENDENCIES = $(AUTHMOD_OBJ)

# Make sure the right authentication module is linked with
xrdp_sesman_LDADD = \
  $(AUTHMOD_OBJ) \
  $(top_builddir)/libipm/libipm.la \
  $(top_builddir)/common/libcommon.la \
  $(AUTHMOD_LIB) \
  -lpthread

sesmansysconfdir=$(sysconfdir)/xrdp

SUBST_VARS = sed \
   -e 's|@sesmansysconfdir[@]|$(sesmansysconfdir)|g'

subst_verbose = $(subst_verbose_@AM_V@)
subst_verbose_ = $(subst_verbose_@AM_DEFAULT_V@)
subst_verbose_0 = @echo "  SUBST    $@";

SUFFIXES = .in
.in:
	$(subst_verbose)$(SUBST_VARS) $< > $@

dist_sesmansysconf_DATA = \
  sesman.ini

dist_sesmansysconf_SCRIPTS = \
  startwm.sh \
  reconnectwm.sh

SUBDIRS = \
  tools \
  chansrv
