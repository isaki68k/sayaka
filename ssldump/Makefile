ssldump:	ssldump.vala
	valac -X -w --pkg=gio-2.0 --pkg=gio-unix-2.0 ssldump.vala

readdump:	readdump.o
	${CXX} -o $@ $>

readdump.o:	readdump.cpp
