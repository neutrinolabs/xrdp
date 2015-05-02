EXTRA_DIST = xrdp.sh km-0407.ini  km-0409.ini km-040c.ini km-0410.ini km-0419.ini km-041d.ini km-0816.ini \
xrdp-sesman.service \
xrdp.service

SUBDIRS = \
  pam.d \
  pulse

if HAVE_SYSTEMD
systemdsystemunit_DATA = \
        xrdp-sesman.service \
        xrdp.service
else
SUBDIRS+= \
  	default \
	init.d
endif

startscriptdir=$(sysconfdir)/xrdp

startscript_DATA = \
  xrdp.sh \
  km-0407.ini \
  km-0409.ini \
  km-040c.ini \
  km-0410.ini \
  km-0411.ini \
  km-0415.ini \
  km-0419.ini \
  km-041d.ini \
  km-0807.ini \
  km-0816.ini \
  km-e0010411.ini \
  km-e0200411.ini \
  km-e0210411.ini

# must be tab below
install-data-hook:
	chmod 755 $(DESTDIR)$(sysconfdir)/xrdp/xrdp.sh
	if [ -f $(DESTDIR)$(sysconfdir)/init.d/xrdp ]; then \
		chmod 755 $(DESTDIR)$(sysconfdir)/init.d/xrdp; \
		sed -i 's|__BASE__|$(prefix)|' $(DESTDIR)$(sysconfdir)/init.d/xrdp; \
	fi
