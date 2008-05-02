
CFGDIR = /etc/xrdp
PIDDIR = /var/run
MANDIR = /usr/local/man
LIBDIR = /usr/local/lib/xrdp
BINDIR = /usr/local/bin
SBINDIR = /usr/local/sbin
SHAREDIR = /usr/local/share/xrdp

all: world

world: base
	$(MAKE) -C sesman

base:
	$(MAKE) -C vnc
	$(MAKE) -C libxrdp
	$(MAKE) -C xrdp
	$(MAKE) -C rdp
	$(MAKE) -C xup

nopam: base
	$(MAKE) -C sesman nopam
	$(MAKE) -C sesman tools

kerberos: base
	$(MAKE) -C sesman kerberos
	$(MAKE) -C sesman tools

clean:
	$(MAKE) -C vnc clean
	$(MAKE) -C libxrdp clean
	$(MAKE) -C xrdp clean
	$(MAKE) -C rdp clean
	$(MAKE) -C sesman clean
	$(MAKE) -C xup clean

install:
	mkdir -p $(CFGDIR)
	mkdir -p $(PIDDIR)
	mkdir -p $(MANDIR)
	mkdir -p $(LIBDIR)
	mkdir -p $(BINDIR)
	mkdir -p $(SBINDIR)
	mkdir -p $(SHAREDIR)
	$(MAKE) -C vnc install
	$(MAKE) -C libxrdp install
	$(MAKE) -C xrdp install
	$(MAKE) -C rdp install
	$(MAKE) -C sesman install
	$(MAKE) -C xup install
	$(MAKE) -C docs install
	if [ -d /etc/pam.d ]; then install instfiles/pam.d/sesman /etc/pam.d/sesman; fi
	install instfiles/xrdp_control.sh $(DESTDIR)/xrdp_control.sh
