all sayaka sixelv clean depend:
	(cd csrc; ${MAKE} $@)

distclean:	clean
	rm -f config.status config.log
	(cd csrc; ${MAKE} $@)
