#/* Copyright (C) 1998 Transarc Corporation - All rights reserved */


SHELL = /bin/sh
include ../../config/Makefile.${SYS_NAME}

CC = ${MT_CC}
CFLAGS = ${OPTMZ} ${DBG} -I${SRCDIR}include ${MT_CFLAGS}
CCRULE = ${CC} ${CFLAGS} -c $?

VLSERVER = ../../vlserver/
VOLSER = ../../volser/
FSINT = ../../fsint/

ADMINOBJS =\
	afs_vosAdmin.o \
	vosutils.o \
	vsprocs.o \
	lockprocs.o

VLSERVEROBJS =\
	vldbint.cs.o \
	vldbint.xdr.o

VOLSEROBJS =\
	volint.cs.o \
	volint.xdr.o

FSINTOBJS =\
	afsint.xdr.o \
	afscbint.xdr.o

LIBOBJS = ${ADMINOBJS} ${VLSERVEROBJS} ${VOLSEROBJS} ${FSINTOBJS}

LIBVOSADMIN = ${DESTDIR}/lib/afs/libvosadmin.a

system install: ${LIBVOSADMIN}

${LIBVOSADMIN}: libvosadmin.a
	${INSTALL} afs_vosAdmin.h ${DESTDIR}include/afs
	${INSTALL} libvosadmin.a ${LIBVOSADMIN}

libvosadmin.a: ${LIBOBJS}
	-rm -f $@
	ar rv $@ ${LIBOBJS}
	${RANLIB} $@

afs_vosAdmin.o: afs_vosAdmin.h

vsprocs.o: vsprocs.c
	case ${SYS_NAME} in \
	rs_aix*) \
		${CC} ${CFLAGS} -D_BSD -c $? ;; \
	*) \
		${CCRULE} ;; \
	esac

vldbint.cs.o: ${VLSERVER}/vldbint.cs.c
	${CCRULE};

vldbint.xdr.o: ${VLSERVER}/vldbint.xdr.c
	${CCRULE};

volint.xdr.o: ${VOLSER}/volint.xdr.c
	${CCRULE};

volint.cs.o: ${VOLSER}/volint.cs.c
	${CCRULE};

afscbint.xdr.o: ${FSINT}/afscbint.xdr.c
	${CCRULE};

afsint.xdr.o: ${FSINT}/afsint.xdr.c
	${CCRULE};

clean:
	rm -f *.o libvosadmin*
