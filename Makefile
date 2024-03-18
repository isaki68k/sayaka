all clean depend sayaka sixelv test:
	(cd src; $(MAKE) $@)

distclean:	clean
	rm -f config.status Makefile.cfg config.h config.log
