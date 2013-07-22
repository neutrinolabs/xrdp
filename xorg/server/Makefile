
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
