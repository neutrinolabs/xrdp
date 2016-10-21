EXTRA_DIST = \
   xrdp.sh \
   xrdp-sesman.service \
   xrdp.service

#
# files for all platforms
#
startscriptdir=$(sysconfdir)/xrdp

dist_startscript_DATA = \
  km-0407.ini \
  km-0409.ini \
  km-040a.ini \
  km-040c.ini \
  km-0410.ini \
  km-0411.ini \
  km-0414.ini \
  km-0415.ini \
  km-0416.ini \
  km-0419.ini \
  km-041d.ini \
  km-0807.ini \
  km-0809.ini \
  km-080c.ini \
  km-0813.ini \
  km-0816.ini \
  km-100c.ini \
  km-e0010411.ini \
  km-e0200411.ini \
  km-e0210411.ini

#
# platform specific files
#
SUBDIRS =
if LINUX
SUBDIRS += \
  pam.d \
  pulse
dist_startscript_SCRIPTS = xrdp.sh
if HAVE_SYSTEMD
dist_systemdsystemunit_DATA = \
  xrdp-sesman.service \
  xrdp.service
else
SUBDIRS += \
  default \
  init.d
endif # HAVE_SYSTEMD
endif # LINUX

if FREEBSD
SUBDIRS += \
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
	if [ -f $(DESTDIR)$(sysconfdir)/init.d/xrdp ]; then \
		sed -i 's|__BASE__|$(prefix)|' $(DESTDIR)$(sysconfdir)/init.d/xrdp; \
	fi
endif

if FREEBSD
# must be tab below
install-data-hook:
	sed -i '' 's|%%PREFIX%%|$(prefix)|g' $(DESTDIR)$(sysconfdir)/rc.d/xrdp
endif
