
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

nopam: base
	make -C sesman nopam

kerberos: base
	make -C sesman kerberos

clean:
	make -C vnc clean
	make -C libxrdp clean
	make -C xrdp clean
	make -C rdp clean
	make -C sesman clean

install:
	mkdir -p $(DESTDIR)
	mkdir -p $(CONFDIR)
	mkdir -p $(PIDDIR)
	mkdir -p $(MANDIR)
	mkdir -p $(DOCDIR)
	make -C vnc install
	make -C libxrdp install
	make -C xrdp install
	make -C rdp install
	make -C sesman install
	make -C docs install
#	install instfiles/pam.d/sesman /etc/pam.d/sesman
