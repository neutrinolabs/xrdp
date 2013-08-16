
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
	cp module/libxorgxrdp.so      $(HOME)/xorg-modules/
	cp xrdpdev/xrdpdev_drv.so     $(HOME)/xorg-modules/drivers/
	cp xrdpmouse/xrdpmouse_drv.so $(HOME)/xorg-modules/input/
	cp xrdpkeyb/xrdpkeyb_drv.so   $(HOME)/xorg-modules/input/
