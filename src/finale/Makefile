#/* Copyright (C) 1990 Transarc Corporation - All rights reserved */
#/*
# * (C) COPYRIGHT IBM CORPORATION 1989
# * LICENSED MATERIALS - PROPERTY OF IBM
# */


SHELL = /bin/sh
include ../config/Makefile.${SYS_NAME}

CFLAGS = -g -I${SRCDIR}include ${XCFLAGS}
LDFLAGS = -g ${XLDFLAGS}
INCLS=${SRCDIR}include/ubik.h \
	${SRCDIR}include/lwp.h \
	${SRCDIR}include/lock.h  \
	${SRCDIR}include/rx/rx.h \
	${SRCDIR}include/rx/xdr.h \
	${SRCDIR}include/rx/rxkad.h \
	${SRCDIR}include/afs/com_err.h \
	${SRCDIR}include/afs/auth.h \
	${SRCDIR}include/afs/cellconfig.h \
	${SRCDIR}include/afs/cmd.h \
	${SRCDIR}include/afs/pterror.h \
	${SRCDIR}include/afs/kautils.h \
	${SRCDIR}include/afs/bnode.h \
	${SRCDIR}include/afs/vlserver.h

rxkadlib=${SRCDIR}lib/librxkad.a
authlib=${SRCDIR}lib/afs/libauth.a
cmdlib=${SRCDIR}lib/afs/libcmd.a
vllib=${SRCDIR}lib/afs/libvldb.a
boslib=${SRCDIR}lib/afs/libbos.a
volserlib=${SRCDIR}lib/afs/libvolser.a
utilib=${SRCDIR}lib/afs/util.a

LIBS=${SRCDIR}lib/libubik.a ${SRCDIR}lib/librx.a ${SRCDIR}lib/liblwp.a ${authlib} ${SRCDIR}lib/afs/libsys.a ${deslib} ${rxkadlib} ${cmdlib} ${vllib} ${boslib} ${SRCDIR}lib/afs/libcom_err.a ${volserlib} ${utilib} ${SRCDIR}lib/afs/libkauth.a ${SRCDIR}lib/afs/libprot.a ${SRCDIR}lib/libdes.a ${XLIBS}

COMPONENT=finale
include ../config/Makefile.version

all:  translate_et

clean:
	rm -f *.o translate_et core AFS_component_version_number.c

translate_et: translate_et.o ${INCLS}
	${CC} ${LDFLAGS} -o translate_et translate_et.o ${LIBS}

translate_et.o: AFS_component_version_number.c

test: translate_et
	echo -n > /tmp/translate_et.output
	-./translate_et >> /tmp/translate_et.output 2>&1
	./translate_et 55 >> /tmp/translate_et.output
	./translate_et 110 >> /tmp/translate_et.output
	./translate_et 255 >> /tmp/translate_et.output
	./translate_et 256 >> /tmp/translate_et.output
	./translate_et -1 -451 >> /tmp/translate_et.output
	./translate_et 1613147 >> /tmp/translate_et.output
	./translate_et 19270407 >> /tmp/translate_et.output
	./translate_et 180511 >> /tmp/translate_et.output
	diff test.output /tmp/translate_et.output
	rm /tmp/translate_et.output

system: install

noversion install.noversion install: all
	${INSTALL} translate_et ${DESTDIR}bin/translate_et
