
all: allmake

allmake:
	cd src; $(MAKE) $(MFLAGS)
	cd tests; $(MAKE) $(MFLAGS)

clean: allclean

allclean:
	cd src; $(MAKE) clean
	cd tests; $(MAKE) clean
