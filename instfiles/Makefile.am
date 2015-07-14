EXTRA_DIST = xrdp.sh km-0407.ini  km-0409.ini km-040c.ini km-0410.ini km-0411.ini km-0419.ini km-041d.ini km-0816.ini km-0415.ini km-0807.ini \
km-e0010411.ini km-e0200411.ini km-e0210411.ini \
xrdp-sesman.service \
xrdp.service

#
# files for all platforms
#
startscriptdir=$(sysconfdir)/xrdp

startscript_DATA = \
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

#
# platform specific files
#
SUBDIRS=
if LINUX
SUBDIRS+= \
        pam.d \
        pulse
startscript_DATA+= xrdp.sh
if HAVE_SYSTEMD
systemdsystemunit_DATA = \
        xrdp-sesman.service \
        xrdp.service
else
SUBDIRS+= \
  	default \
	init.d
endif # HAVE_SYSTEMD
endif # LINUX

if FREEBSD
SUBDIRS+= \
        pam.d \
        rc.d \
        pulse
endif

#
# install-data-hook for each platform
#
if LINUX
# must be tab below
install-data-hook:
	chmod 755 $(DESTDIR)$(sysconfdir)/xrdp/xrdp.sh
	if [ -f $(DESTDIR)$(sysconfdir)/init.d/xrdp ]; then \
		chmod 755 $(DESTDIR)$(sysconfdir)/init.d/xrdp; \
		sed -i 's|__BASE__|$(prefix)|' $(DESTDIR)$(sysconfdir)/init.d/xrdp; \
	fi
endif

if FREEBSD
# must be tab below
install-data-hook:
	chmod 755 $(DESTDIR)$(sysconfdir)/rc.d/xrdp
	sed -i '' 's|%%PREFIX%%|$(prefix)|g' $(DESTDIR)$(sysconfdir)/rc.d/xrdp
endif
