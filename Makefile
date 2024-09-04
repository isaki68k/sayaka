all sayaka sixelv clean depend:
	(cd src; ${MAKE} $@)

distclean:	clean
	rm -f config.status config.log
	(cd src; ${MAKE} $@)
