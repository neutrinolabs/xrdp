xrdppkgdatadir=$(datadir)/xrdp

pkglibexec_PROGRAMS = \
  xrdp-droppriv

dist_xrdppkgdata_SCRIPTS = \
  xrdp-chkpriv

AM_LDFLAGS = 

AM_CPPFLAGS = \
  -I$(top_srcdir)/common

xrdp_droppriv_SOURCES = xrdp-droppriv.c

xrdp_droppriv_LDADD = \
  $(top_builddir)/common/libcommon.la

SUBST_VARS = sed \
   -e 's|@pkglibexecdir[@]|$(pkglibexecdir)|g' \
   -e 's|@sysconfdir[@]|$(sysconfdir)|g'

subst_verbose = $(subst_verbose_@AM_V@)
subst_verbose_ = $(subst_verbose_@AM_DEFAULT_V@)
subst_verbose_0 = @echo "  SUBST    $@";

SUFFIXES = .in
.in:
	$(subst_verbose)$(SUBST_VARS) $< > $@

CLEANFILES = xrdp-chkpriv

