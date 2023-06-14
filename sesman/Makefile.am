EXTRA_DIST = \
  Doxyfile

AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_LIBEXEC_PATH=\"${libexecdir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -DXRDP_SOCKET_ROOT_PATH=\"${socketdir}\" \
  -I$(top_srcdir)/sesman/libsesman \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/libipm

sbin_PROGRAMS = \
  xrdp-sesman

xrdp_sesman_SOURCES = \
  eicp_process.c \
  eicp_process.h \
  ercp_process.c \
  ercp_process.h \
  lock_uds.c \
  lock_uds.h \
  pre_session_list.c \
  pre_session_list.h \
  scp_process.c \
  scp_process.h \
  sesman.c \
  sesman.h \
  sesexec_control.c \
  sesexec_control.h \
  session_list.c \
  session_list.h \
  sig.c \
  sig.h

xrdp_sesman_LDADD = \
  $(top_builddir)/sesman/libsesman/libsesman.la \
  $(top_builddir)/libipm/libipm.la \
  $(top_builddir)/common/libcommon.la

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
  libsesman \
  sesexec \
  tools \
  chansrv
