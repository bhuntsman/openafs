
SRCDIR=DEST/
DESTDIR=DEST/
BACKDIR=../../../../PARENT/dest/
OPTIMIZE=-O
CC	= cc
#  Test programs
PROGS 	= \
	key_test \
	testit \
	verify

INCDIRS= -I${DESTDIR}include -I.. -I${BACKDIR}include
LDIRS= -L${DESTDIR}lib -L${DESTDIR}lib/afs -L.. -L${BACKDIR}lib -L${BACKDIR}lib/afs
utilib=${DESTDIR}lib/afs/util.a
LIBS= -ldes

CFLAGS = ${OPTIMIZE} ${INCDIRS} ${LDIRS} ${LIBS} ${XCFLAGS}

all system test: $(PROGS)

clean:
	rm -f *.o *.a testit verify key_test core

kinstall:
	@echo No kernel source here.

testit: testit.o 
	${CC} ${CFLAGS} testit.o -o testit
verify: verify.o libdes.a
	${CC} ${CFLAGS} verify.o -o verify
key_test: key_test.o libdes.a
	${CC}  ${CFLAGS} key_test.o -o key_test

