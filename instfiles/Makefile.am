EXTRA_DIST = xrdp.sh km-0407.ini  km-0409.ini km-040c.ini km-0410.ini km-0419.ini km-041d.ini \
xrdp-sesman.service \
xrdp.service

SUBDIRS = \
  pam.d

if HAVE_SYSTEMD
systemdsystemunit_DATA = \
        xrdp-sesman.service \
        xrdp.service
endif

startscriptdir=$(sysconfdir)/xrdp

startscript_DATA = \
  xrdp.sh \
  km-0407.ini \
  km-0409.ini \
  km-040c.ini \
  km-0410.ini \
  km-0419.ini \
  km-041d.ini

# must be tab below
install-data-hook:
	chmod 755 $(DESTDIR)$(sysconfdir)/xrdp/xrdp.sh
