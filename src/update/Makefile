
# /* Copyright (C) 1991, 1990 Transarc Corporation - All rights reserved */

SHELL = /bin/sh
COMPONENT=update
include ../config/Makefile.${SYS_NAME}

INSTALL=${SRCDIR}bin/install
LIBRX=${SRCDIR}lib/librx.a
LIBS = ${SRCDIR}lib/afs/libauth.a ${SRCDIR}lib/librxkad.a \
       ${SRCDIR}lib/libdes.a ${LIBRX} ${SRCDIR}lib/liblwp.a \
       ${SRCDIR}lib/afs/libcom_err.a ${SRCDIR}lib/afs/util.a \
       ${SRCDIR}lib/afs/libsys.a 
CFLAGS= ${DBUG}  -I${SRCDIR}include ${XCFLAGS}

PROGS=upclient upserver

include ../config/Makefile.version

all:  ${PROGS}

upclient: client.o update.cs.o utils.o ${LIBS} 
	${CC} ${CFLAGS} -o upclient client.o update.cs.o utils.o ${LIBS} ${XLIBS}

upserver: server.o utils.o update.ss.o ${LIBS}
	${CC} ${CFLAGS} -o upserver server.o utils.o update.ss.o ${LIBS} ${XLIBS}

utils.o: utils.c update.h global.h

client.o server.o: update.h global.h AFS_component_version_number.c

update.cs.c update.ss.c update.er.c update.h: update.xg
	${SRCDIR}bin/rxgen update.xg
clean: 
	rm -f upclient upserver *.o update.ss.c update.cs.c update.xdr.c update.h core\
	AFS_component_version_number.c

system: install
install : all
	${INSTALL} upserver ${DESTDIR}root.server/usr/afs/bin
	${INSTALL} upclient ${DESTDIR}root.server/usr/afs/bin
