EXTRA_DIST = \
   keymap-names.txt \
   xrdp-sesman.service.in \
   xrdp.service.in

#
# substitute directories in service file
#
CLEANFILES= \
  xrdp-sesman.service \
  xrdp.service

SUBST_VARS = sed \
   -e 's|@sbindir[@]|$(sbindir)|g' \
   -e 's|@sysconfdir[@]|$(sysconfdir)|g' \
   -e 's|@localstatedir[@]|$(localstatedir)|g'

subst_verbose = $(subst_verbose_@AM_V@)
subst_verbose_ = $(subst_verbose_@AM_DEFAULT_V@)
subst_verbose_0 = @echo "  SUBST    $@";

SUFFIXES = .in
.in:
	$(subst_verbose)$(SUBST_VARS) $< > $@

#
# files for all platforms
#
startscriptdir=$(sysconfdir)/xrdp

dist_startscript_DATA = \
  km-00000406.toml \
  km-00000407.toml \
  km-00000409.toml \
  km-0000040a.toml \
  km-0000040b.toml \
  km-0000040c.toml \
  km-00000410.toml \
  km-00000411.toml \
  km-00000412.toml \
  km-00000414.toml \
  km-00000415.toml \
  km-00000416.toml \
  km-00000419.toml \
  km-0000041d.toml \
  km-00000807.toml \
  km-00000809.toml \
  km-0000080a.toml \
  km-0000080c.toml \
  km-00000813.toml \
  km-00000816.toml \
  km-0000100c.toml \
  km-00010409.toml \
  km-19360409.toml

#
# platform specific files
#
SUBDIRS =
if LINUX
SUBDIRS += \
  pam.d \
  pulse
if HAVE_SYSTEMD
systemdsystemunit_DATA = \
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

if MACOS
SUBDIRS += pam.d
endif

#
# install-data-hook for each platform
# TODO: subst these directories as well as service files
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
	sed -i '' 's|%%PREFIX%%|$(prefix)|g' $(DESTDIR)$(sysconfdir)/rc.d/xrdp \
                                         $(DESTDIR)$(sysconfdir)/rc.d/xrdp-sesman
endif
