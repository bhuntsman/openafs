#
# Makefile for Scout, the AFS navigation and monitoring tool.
#
#	(C) Copyright Transarc Corporation 1989
#	Licensed Materials - Property of Transarc
#	All Rights Reserved.
#
SHELL = /bin/sh
COMPONENT=scout
include ../config/Makefile.${SYS_NAME}

INSTALL=${SRCDIR}bin/install

CFLAGS= ${DBUG} -I. \
	-I${SRCDIR}include \
	-I${SRCDIR}include/afs \
	-I${SRCDIR} ${XCFLAGS}
LDFLAGS = ${XLDFLAGS}

#RPCINCLS=${SRCDIR}include/lwp.h ${SRCDIR}include/rx/rx.h bosint.h
RPCINCLS=

INCLS=${SRCDIR}include/afs/gtxobjects.h \
	${SRCDIR}include/afs/gtxwindows.h \
	${SRCDIR}include/afs/gtxcurseswin.h \
	${SRCDIR}include/afs/gtxdumbwin.h \
	${SRCDIR}include/afs/gtxX11win.h \
	${SRCDIR}include/afs/gtxobjects.h \
	${SRCDIR}include/afs/gtxlightobj.h \
	${SRCDIR}include/afs/gtxtextobj.h \
	${SRCDIR}include/afs/gtxobjdict.h \
	${RPCINCLS} \
	${SRCDIR}include/afs/keys.h \
	${SRCDIR}include/afs/cellconfig.h \
	${SRCDIR}include/afs/cmd.h
 
LIBS= 	${SRCDIR}lib/afs/libgtx.a \
	${SRCDIR}lib/afs/libfsprobe.a \
	${SRCDIR}lib/afs/libvolser.a \
	${SRCDIR}lib/afs/libkauth.a \
	${SRCDIR}lib/afs/libauth.a \
	${SRCDIR}lib/librxkad.a \
	${SRCDIR}lib/libdes.a \
	${SRCDIR}lib/afs/libcmd.a \
	${SRCDIR}lib/afs/vlib.a ${SRCDIR}/lib/afs/libacl.a \
        ${SRCDIR}lib/afs/libvldb.a \
        ${SRCDIR}lib/libubik.a \
	${SRCDIR}lib/afs/libafsint.a \
	${SRCDIR}lib/afs/libsys.a \
	${SRCDIR}lib/librx.a \
	${SRCDIR}lib/afs/libsys.a \
	${SRCDIR}lib/afs/libaudit.a \
	${SRCDIR}lib/afs/libcom_err.a \
	${SRCDIR}lib/liblwp.a \
	${SRCDIR}lib/afs/util.a

EXTRA_LIBS= ${TXLIBS} ${SRCDIR}lib/afs/libtermlib.a  ${XLIBS}

include ../config/Makefile.version

all: scout

scout.o: scout.c ${INCLS} AFS_component_version_number.c

scout: scout.o $(LIBS)
	${CC} ${LDFLAGS} -o scout scout.o $(LIBS) ${EXTRA_LIBS}

#
# Misc others
#
system: install

install: all
	${INSTALL} scout ${DESTDIR}bin

clean:
	rm -f  *.o scout core AFS_component_version_number.c
