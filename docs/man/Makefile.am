man_MANS = \
  xrdp-dis.1 \
  sesman.ini.5 \
  xrdp.ini.5 \
  xrdp.8 \
  xrdp-chansrv.8 \
  xrdp-genkeymap.8 \
  xrdp-keygen.8 \
  xrdp-sesadmin.8 \
  xrdp-sesman.8 \
  xrdp-sesrun.8 \
  xrdp-dumpfv1.8

EXTRA_DIST = xrdp-mkfv1.8.in $(man_MANS:=.in)

if USE_FREETYPE2
  man_MANS += xrdp-mkfv1.8
endif

SUBST_VARS = sed \
   -e 's|@PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
   -e 's|@bindir[@]|$(bindir)|g' \
   -e 's|@sbindir[@]|$(sbindir)|g' \
   -e 's|@localstatedir[@]|$(localstatedir)|g' \
   -e 's|@sysconfdir[@]|$(sysconfdir)|g' \
   -e 's|@socketdir[@]|$(socketdir)|g' \
   -e 's|@xrdpconfdir[@]|$(sysconfdir)/xrdp|g' \
   -e 's|@xrdpdatadir[@]|$(datadir)/xrdp|g' \
   -e 's|@xrdphomeurl[@]|http://www.xrdp.org/|g'

subst_verbose = $(subst_verbose_@AM_V@)
subst_verbose_ = $(subst_verbose_@AM_DEFAULT_V@)
subst_verbose_0 = @echo "  SUBST    $@";

SUFFIXES = .in
.in:
	$(subst_verbose)$(SUBST_VARS) $< > $@

CLEANFILES = $(man_MANS)
