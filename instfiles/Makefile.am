
SUBDIRS = \
  pam.d

startscriptdir=$(sysconfdir)/xrdp

startscript_DATA = \
  xrdp.sh

# must be tab below
install-data-hook:
	chmod 755 $(DESTDIR)$(sysconfdir)/xrdp/xrdp.sh

#	if [ -w /etc/init.d ]; then rm -f /etc/init.d/xrdp.sh; fi
#	if [ -w /etc/init.d ]; then $(LN_S) $(DESTDIR)$(sysconfdir)/xrdp/xrdp.sh /etc/init.d/xrdp.sh; fi
#	$(DESTDIR)$(bindir)/xrdp-keygen xrdp $(DESTDIR)$(sysconfdir)/xrdp/rsakeys.ini
