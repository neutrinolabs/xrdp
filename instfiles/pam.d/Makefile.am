PAM_FILES = \
  xrdp-sesman.debian \
  xrdp-sesman.redhat \
  xrdp-sesman.suse \
  xrdp-sesman.freebsd \
  xrdp-sesman.macos \
  xrdp-sesman.unix \
  xrdp-sesman.arch

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

pamddir = $(sysconfdir)/pam.d

pamd_DATA = \
  $(PAMFILE)

xrdp-sesman: mkpamrules
	$(srcdir)/mkpamrules $(PAM_RULES) $(srcdir) $@
