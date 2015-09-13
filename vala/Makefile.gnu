#
# GNU Make 用の Makefile です
#

all:	sayaka

sayaka:	${SRCS_sayaka:.vala=.o} ${SRCS_NATIVE:.c=.o}
	${CC} -o $@ $^ ${LIBS}

${SRCS_sayaka:.vala=.o} ${SRCS_NATIVE:.c=.o}: %.o : %.c
	${CC} ${COPTS} ${CFLAGS} -c $^ -o $@

${SRCS_sayaka:.vala=.c}: %.c : %.vala
	if [ -e $@ ] && [ $@ = `ls -dt $@ $+ | head -1` ]; then :; \
	else \
		${VALAC} -C ${SRCS_sayaka}; \
		for Y in ${SRCS_sayaka:.vala=} ; \
		do \
			if [ $$Y.vala = `ls -dt $$Y.vala $$Y.c | head -1` ]; then touch $$Y.c; fi; \
		done; \
	fi;

