moduledir := $(shell pkg-config xorg-server --variable moduledir)

all: allmake

allmake:
	cd module; $(MAKE) $(MFLAGS)
	cd xrdpdev; $(MAKE) $(MFLAGS)
	cd xrdpkeyb; $(MAKE) $(MFLAGS)
	cd xrdpmouse; $(MAKE) $(MFLAGS)

clean: allclean

allclean:
	cd module; $(MAKE) clean
	cd xrdpdev; $(MAKE) clean
	cd xrdpkeyb; $(MAKE) clean
	cd xrdpmouse; $(MAKE) clean

xinstall:
	strip module/libxorgxrdp.so
	strip xrdpdev/xrdpdev_drv.so
	strip xrdpmouse/xrdpmouse_drv.so
	strip xrdpkeyb/xrdpkeyb_drv.so

	cp module/libxorgxrdp.so      $(HOME)/xorg-modules/
	cp xrdpdev/xrdpdev_drv.so     $(HOME)/xorg-modules/drivers/
	cp xrdpmouse/xrdpmouse_drv.so $(HOME)/xorg-modules/input/
	cp xrdpkeyb/xrdpkeyb_drv.so   $(HOME)/xorg-modules/input/

install:
	install --directory $(DESTDIR)$(moduledir)
	install --mode=0644 --strip module/libxorgxrdp.so $(DESTDIR)$(moduledir)
	install --mode=0644 --strip xrdpdev/xrdpdev_drv.so $(DESTDIR)$(moduledir)/drivers
	install --mode=0644 --strip xrdpmouse/xrdpmouse_drv.so $(DESTDIR)$(moduledir)/input
	install --mode=0644 --strip xrdpkeyb/xrdpkeyb_drv.so $(DESTDIR)$(moduledir)/input
