#
# Makefile for fsprobe and its test program, all part of the AFS
# navigation and monitoring tool (gator).
#
#	Transarc Corporation
#
#	(C) Copyright Transarc Corporation 1989
#	Licensed Materials - Property of Transarc
#	All Rights Reserved.
#
SHELL = /bin/sh
COMPONENT=fsprobe
include ../config/Makefile.${SYS_NAME}

INSTALL=${SRCDIR}bin/install

CFLAGS=-g -I. \
	-I${SRCDIR}include \
	-I${SRCDIR}include/afs \
	-I${SRCDIR} \
	-I/usr/include ${XCFLAGS}

RPCINCLS=${SRCDIR}include/lwp.h ${SRCDIR}include/rx/rx.h

INCLS=fsprobe.h ${RPCINCLS}

LIBS= 	${SRCDIR}lib/afs/libvolser.a ${SRCDIR}lib/afs/vlib.a ${SRCDIR}/lib/afs/libacl.a \
        ${SRCDIR}lib/afs/libsys.a ${SRCDIR}lib/afs/libvldb.a \
        ${SRCDIR}lib/libubik.a \
        ${SRCDIR}lib/afs/libauth.a ${SRCDIR}lib/afs/libsys.a \
        ${SRCDIR}lib/afs/libcmd.a ${SRCDIR}lib/librxkad.a ${SRCDIR}lib/libdes.a \
        ${SRCDIR}lib/afs/libcom_err.a ${SRCDIR}lib/afs/libkauth.a \
	${SRCDIR}lib/afs/libaudit.a \
	${SRCDIR}lib/afs/libafsint.a \
	${SRCDIR}lib/librx.a \
	${SRCDIR}lib/liblwp.a \
	${SRCDIR}lib/afs/libsys.a \
	${SRCDIR}lib/afs/util.a 

OBJS=fsprobe.o \
	fsprobe_callback.o

include ../config/Makefile.version

noversion: install

all: libfsprobe.a fsprobe_test

libfsprobe.a: ${OBJS} AFS_component_version_number.o
	-rm -f libfsprobe.a
	${AR} rv libfsprobe.a ${OBJS} AFS_component_version_number.o
	${RANLIB} libfsprobe.a

fsprobe.o: fsprobe.c ${INCLS} AFS_component_version_number.c

fsprobe_callback.o: fsprobe_callback.c ${INCLS}

fsprobe_test: fsprobe_test.o libfsprobe.a ${LIBS}
	${CC} ${CFLAGS} -o fsprobe_test fsprobe_test.o libfsprobe.a \
		${LIBS} ${XLIBS}

#
# Misc others
#
system: install

install: all
	${INSTALL} fsprobe.h    ${DESTDIR}include/afs
	${INSTALL} libfsprobe.a ${DESTDIR}lib/afs

clean:
	rm -f *.o fsprobe_test libfsprobe.a core AFS_component_version_number.c
