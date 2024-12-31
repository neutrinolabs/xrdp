EXTRA_DIST = openssl.conf

AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/${sysconfsubdir}\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -I$(top_srcdir)/common

bin_PROGRAMS = \
  xrdp-keygen

xrdp_keygen_SOURCES = keygen.c

xrdp_keygen_LDADD = \
  $(top_builddir)/common/libcommon.la

xrdpsysconfdir = $(sysconfdir)/$(sysconfsubdir)

install-data-hook:
	umask 077 && \
	if [ ! -f $(DESTDIR)$(xrdpsysconfdir)/rsakeys.ini ]; then \
	  ./xrdp-keygen xrdp $(DESTDIR)$(xrdpsysconfdir)/rsakeys.ini; \
	fi && \
	if [ ! -f $(DESTDIR)$(xrdpsysconfdir)/cert.pem ]; then \
	  $(OPENSSL) req -x509 -newkey rsa:2048 -sha256 -nodes \
	    -keyout $(DESTDIR)$(xrdpsysconfdir)/key.pem -out \
	    $(DESTDIR)$(xrdpsysconfdir)/cert.pem -days 365 \
	    -subj /C=US/ST=CA/L=Sunnyvale/O=xrdp/CN=www.xrdp.org \
	    -config $(srcdir)/openssl.conf; \
	fi

uninstall-hook:
	rm -f $(DESTDIR)$(xrdpsysconfdir)/rsakeys.ini
	rm -f $(DESTDIR)$(xrdpsysconfdir)/cert.pem
	rm -f $(DESTDIR)$(xrdpsysconfdir)/key.pem
