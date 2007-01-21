
DESTDIR = /usr/local/xrdp
CFGDIR = /etc/xrdp
PIDDIR = /var/run
MANDIR = /usr/local/man
DOCDIR = /usr/doc/xrdp

all: world

world: base
	make -C sesman

base:
	make -C vnc
	make -C libxrdp
	make -C xrdp
	make -C rdp
	make -C xup

nopam: base
	make -C sesman nopam
	make -C sesman tools

kerberos: base
	make -C sesman kerberos
	make -C sesman tools

clean:
	make -C vnc clean
	make -C libxrdp clean
	make -C xrdp clean
	make -C rdp clean
	make -C sesman clean
	make -C xup clean

install:
	mkdir -p $(DESTDIR)
	mkdir -p $(CFGDIR)
	mkdir -p $(PIDDIR)
	mkdir -p $(MANDIR)
	mkdir -p $(DOCDIR)
	make -C vnc install
	make -C libxrdp install
	make -C xrdp install
	make -C rdp install
	make -C sesman install
	make -C xup install
	make -C docs install
	if [ -d /etc/pam.d ]; then install instfiles/pam.d/sesman /etc/pam.d/sesman; fi
	install instfiles/xrdp_control.sh $(DESTDIR)/xrdp_control.sh

installdeb:
	mkdir -p $(DESTDIRDEB)/usr/lib/xrdp
	mkdir -p $(DESTDIRDEB)/etc/xrdp
	mkdir -p $(DESTDIRDEB)/etc/pam.d
	mkdir -p $(DESTDIRDEB)/etc/init.d
	mkdir -p $(DESTDIRDEB)/usr/man
	mkdir -p $(DESTDIRDEB)/usr/man/man5
	mkdir -p $(DESTDIRDEB)/usr/man/man8
	make -C vnc installdeb DESTDIRDEB=$(DESTDIRDEB)
	make -C libxrdp installdeb DESTDIRDEB=$(DESTDIRDEB)
	make -C xrdp installdeb DESTDIRDEB=$(DESTDIRDEB)
	make -C rdp installdeb DESTDIRDEB=$(DESTDIRDEB)
	make -C sesman installdeb DESTDIRDEB=$(DESTDIRDEB)
	make -C xup installdeb DESTDIRDEB=$(DESTDIRDEB)
	make -C docs installdeb DESTDIRDEB=$(DESTDIRDEB)
	install instfiles/pam.d/sesman $(DESTDIRDEB)/etc/pam.d/sesman
	install instfiles/xrdp_control1.sh $(DESTDIRDEB)/etc/init.d/xrdp_control.sh
