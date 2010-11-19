EXTRA_DIST = xrdp-sesman

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

pamddir=$(sysconfdir)/pam.d

pamd_DATA = \
  $(PAMFILE)
