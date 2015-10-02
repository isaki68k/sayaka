CXXFLAGS+=	-std=gnu++11

ssldump:	ssldump.o stream.o
	${CXX} -o $@ $>

ssldump.o:	ssldump.cpp stream.h

stream.o:	stream.cpp stream.h
