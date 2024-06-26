#
# Makefile for the module providing the client side of the File Server
# and Cache Manager extended statistics gathering.
#
#	(C) Copyright Transarc Corporation 1990
#	Licensed Materials - Property of Transarc
#	All Rights Reserved.
#
SHELL = /bin/sh
COMPONENT=xstat
include ../config/Makefile.${SYS_NAME}
INSTALL=${SRCDIR}bin/install

CFLAGS= ${DBUG} -I. \
	-I${SRCDIR}include \
	-I${SRCDIR}include/afs \
	-I${SRCDIR} \
	-I/usr/include ${XCFLAGS}

RPCINCLS=${SRCDIR}include/lwp.h ${SRCDIR}include/rx/rx.h
FSINCLS=xstat_fs.h ${RPCINCLS}
CMINCLS=xstat_cm.h ${RPCINCLS}

AFSLIBS=${SRCDIR}lib/afs/libafsint.a \
	${SRCDIR}lib/afs/libcmd.a \
	${SRCDIR}lib/librx.a \
	${SRCDIR}lib/liblwp.a \
	${SRCDIR}lib/afs/libsys.a \
	${SRCDIR}lib/afs/util.a

LIBS = ${AFSLIBS} 

FSOBJS=xstat_fs.o \
	xstat_fs_callback.o

CMOBJS=xstat_cm.o

include ../config/Makefile.version

all: libxstat_fs.a xstat_fs_test \
     libxstat_cm.a xstat_cm_test

#
# ------------------------------------------ File Server data collection
#
libxstat_fs.a: ${FSOBJS} AFS_component_version_number.o
	-rm -f libxstat_fs.a
	${AR} rv libxstat_fs.a ${FSOBJS} AFS_component_version_number.o
	$(RANLIB) libxstat_fs.a

xstat_fs.o: xstat_fs.c ${FSINCLS}

xstat_fs_callback.o: xstat_fs_callback.c ${FSINCLS}

xstat_fs_test.: xstat_fs_test.c ${FSINCLS}

xstat_fs_test: xstat_fs_test.o libxstat_fs.a ${LIBS}
	${CC} ${CFLAGS} -o xstat_fs_test xstat_fs_test.o libxstat_fs.a ${LIBS} ${XLIBS}

#
# ---------------------------------------- Cache Manager data collection
#
libxstat_cm.a: ${CMOBJS} AFS_component_version_number.o
	-rm -f libxstat_cm.a
	${AR} rv libxstat_cm.a ${CMOBJS} AFS_component_version_number.o
	$(RANLIB) libxstat_cm.a

xstat_cm.o: xstat_cm.c ${CMINCLS}

xstat_cm_test.o: xstat_cm_test.c ${CMINCLS}

xstat_cm_test: xstat_cm_test.o libxstat_cm.a ${LIBS}
	${CC} ${CFLAGS} -o xstat_cm_test xstat_cm_test.o libxstat_cm.a ${LIBS} ${XLIBS}

#
# Misc others

#
system: install

install: all
	${INSTALL} xstat_fs.h    ${DESTDIR}include/afs
	${INSTALL} xstat_cm.h    ${DESTDIR}include/afs
	${INSTALL} libxstat_fs.a ${DESTDIR}lib/afs
	${INSTALL} libxstat_cm.a ${DESTDIR}lib/afs
	${INSTALL} xstat_fs_test ${DESTDIR}bin
	${INSTALL} xstat_cm_test ${DESTDIR}bin

clean:
	rm -f *.o \
		xstat_fs_test \
		xstat_cm_test \
		libxstat_fs.a \
		libxstat_cm.a \
		core\
		AFS_component_version_number.c
