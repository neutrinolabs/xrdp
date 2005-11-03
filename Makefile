
DESTDIR = /usr/local/xrdp

all: world

world:
	make -C vnc
	make -C libxrdp
	make -C xrdp
	make -C rdp
	make -C sesman

clean:
	make -C vnc clean
	make -C libxrdp clean
	make -C xrdp clean
	make -C rdp clean
	make -C sesman clean

install:
	mkdir -p $(DESTDIR)
	install xrdp/xrdp $(DESTDIR)/xrdp
	install libxrdp/libxrdp.so $(DESTDIR)/libxrdp.so
	install xrdp/ad256.bmp $(DESTDIR)/ad256.bmp
	install xrdp/xrdp256.bmp $(DESTDIR)/xrdp256.bmp
	install xrdp/cursor0.cur $(DESTDIR)/cursor0.cur
	install xrdp/cursor1.cur $(DESTDIR)/cursor1.cur
	install xrdp/Tahoma-10.fv1 $(DESTDIR)/Tahoma-10.fv1
	install vnc/libvnc.so $(DESTDIR)/libvnc.so
	install sesman/sesman $(DESTDIR)/sesman
	install sesman/sesrun $(DESTDIR)/sesrun
	install instfiles/sesman.ini $(DESTDIR)/sesman.ini
	install instfiles/startwm.sh $(DESTDIR)/startwm.sh
	install instfiles/xrdp.ini $(DESTDIR)/xrdp.ini
	install instfiles/xrdpstart.sh $(DESTDIR)/xrdpstart.sh
	install instfiles/pam.d/sesman /etc/pam.d/sesman
	install xrdp/rsakeys.ini $(DESTDIR)/rsakeys.ini
	install rdp/librdp.so $(DESTDIR)/librdp.so
