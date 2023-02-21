PAM_FILES = \
  xrdp-sesman.arch \
  xrdp-sesman.debian \
  xrdp-sesman.freebsd \
  xrdp-sesman.macos \
  xrdp-sesman.redhat \
  xrdp-sesman.suse \
  xrdp-sesman.system \
  xrdp-sesman.unix

EXTRA_DIST = $(PAM_FILES) mkpamrules

CLEANFILES = xrdp-sesman

if SESMAN_NOPAM
PAMFILE =
else
if SESMAN_PAMUSERPASS
PAMFILE =
else
if SESMAN_KERBEROS
PAMFILE =
else
PAMFILE = xrdp-sesman
endif
endif
endif

pamddir = $(pamconfdir)

pamd_DATA = \
  $(PAMFILE)

xrdp-sesman: mkpamrules
	$(srcdir)/mkpamrules $(PAM_RULES) $(srcdir) $@
