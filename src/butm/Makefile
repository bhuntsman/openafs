

#/*
# * (C) COPYRIGHT IBM CORPORATION 1989
# * LICENSED MATERIALS - PROPERTY OF IBM
# */

SHELL = /bin/sh
SRCDIR=DEST/
DESTDIR=DEST/
INSTALL=${SRCDIR}bin/install

CFLAGS = -g -w -I${SRCDIR}include ${XCFLAGS}
LDFLAGS = -g ${XLDFLAGS}
INCLS= ${SRCDIR}include/afs/com_err.h ${SRCDIR}include/afs/butm.h

LIBS=${SRCDIR}lib/afs/libbubasics.a \
${SRCDIR}lib/liblwp.a ${SRCDIR}lib/afs/libusd.a \
${SRCDIR}lib/afs/libcom_err.a ${SRCDIR}lib/afs/libbutm.a \
${SRCDIR}lib/afs/libcom_err.a ${SRCDIR}lib/afs/util.a ${XLIBS}

OBJS=file_tm.o 

COMPONENT=butm
include ../config/Makefile.${SYS_NAME}
include ../config/Makefile.version

noversion: install

all: libbutm.a 

clean:
	rm -f *.o *.a core test_ftm AFS_component_version_number.c

file_tm.o: file_tm.c ${INCLS}

test_ftm.o: test_ftm.c ${INCLS} AFS_component_version_number.c

test_ftm: test_ftm.o libbutm.a
	${CC} ${LDFLAGS} -o test_ftm test_ftm.o libbutm.a ${LIBS}

libbutm.a: ${OBJS} AFS_component_version_number.o
	-rm -f libbutm.a
	${AR} rv libbutm.a $(OBJS) AFS_component_version_number.o
	${RANLIB} libbutm.a

${DESTDIR}lib/afs/libbutm.a: libbutm.a
	${INSTALL}   $? $@

system: install
install install.noversion: all ${DESTDIR}lib/afs/libbutm.a

test: test_ftm
	echo 'Usage: ./test_ftm -conf config -tape xxx *'


