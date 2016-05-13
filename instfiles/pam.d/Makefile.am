EXTRA_DIST = \
  xrdp-sesman.common \
  xrdp-sesman.other \
  xrdp-sesman.password-auth

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

xrdp-sesman:
	if test -e /etc/pam.d/password-auth; then \
	  pamrules=xrdp-sesman.password-auth; \
	else \
	  if test -e /etc/pam.d/common-auth; then \
	    pamrules=xrdp-sesman.common; \
	  else \
	    pamrules=xrdp-sesman.other; \
	  fi; \
	fi; \
	$(LN_S) $(srcdir)/$$pamrules $@
