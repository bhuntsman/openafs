#/* Copyright (C) 1998 Transarc Corporation - All rights reserved */


SHELL = /bin/sh
include ../../config/Makefile.${SYS_NAME}

CC = ${MT_CC}
CFLAGS = ${OPTMZ} ${DBG} -I${SRCDIR}include ${MT_CFLAGS}
CCRULE = ${CC} ${CFLAGS} -c $?

PTSERVER = ../../ptserver/

ADMINOBJS = \
	afs_ptsAdmin.o

PTSERVEROBJS = \
	ptint.xdr.o \
	ptint.cs.o

LIBOBJS = ${ADMINOBJS} ${PTSERVEROBJS}

LIBPTSADMIN = ${DESTDIR}/lib/afs/libptsadmin.a

system install: ${LIBPTSADMIN}

${LIBPTSADMIN}: libptsadmin.a
	${INSTALL} afs_ptsAdmin.h ${DESTDIR}include/afs
	${INSTALL} libptsadmin.a ${LIBPTSADMIN}

libptsadmin.a: ${LIBOBJS}
	-rm -f $@
	ar rv $@ ${LIBOBJS}
	${RANLIB} $@

afs_ptsAdmin.o: afs_ptsAdmin.h

ptint.xdr.o: ${PTSERVER}/ptint.xdr.c
	${CCRULE};

ptint.cs.o: ${PTSERVER}/ptint.cs.c
	${CCRULE};

clean:
	rm -f *.o libptsadmin*
