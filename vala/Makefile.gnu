#
# GNU Make 用の Makefile です
#

all:	sayaka

sayaka:	${SRCS.sayaka:.vala=.o} ${SRCS_NATIVE:.c=.o}
	${CC} -o $@ $^ ${LIBS}

${SRCS.sayaka:.vala=.o} ${SRCS_NATIVE:.c=.o}: %.o : %.c
	${CC} ${COPTS} ${CFLAGS} -c $^ -o $@

${SRCS.sayaka:.vala=.c}: %.c : %.vala
	if [ -e $@ ] && [ $@ = `ls -dt $@ $+ | head -1` ]; then :; \
	else \
		${VALAC} -C ${SRCS.sayaka}; \
		for Y in ${SRCS.sayaka:.vala=} ; \
		do \
			if [ $$Y.vala = `ls -dt $$Y.vala $$Y.c | head -1` ]; then touch $$Y.c; fi; \
		done; \
	fi;

