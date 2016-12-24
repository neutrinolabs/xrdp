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
  xrdp-sessvc.8

EXTRA_DIST = $(man_MANS:=.in)

SUBST_VARS = sed \
   -e 's|@PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
   -e 's|@bindir[@]|$(bindir)|g' \
   -e 's|@localstatedir[@]|$(localstatedir)|g' \
   -e 's|@sysconfdir[@]|$(sysconfdir)|g'

SUFFIXES = .in
.in:
	$(SUBST_VARS) $< > $@

CLEANFILES = $(man_MANS)
