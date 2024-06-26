SHELL = /bin/sh
COMPONENT=butc
include ../config/Makefile.${SYS_NAME}

INSTALL=${SRCDIR}bin/install

CFLAGS=-g -w ${INCDIRS} ${XCFLAGS}

INCDIRS= -I${SRCDIR}include/afs -I${SRCDIR}include/rx -I${SRCDIR}include

INCLS=${SRCDIR}include/afs/partition.h ${SRCDIR}include/afs/volume.h \
	${SRCDIR}include/afs/vlserver.h ${SRCDIR}include/rx/rx.h \
	${SRCDIR}include/rx/xdr.h ${SRCDIR}include/afs/keys.h \
	${SRCDIR}include/afs/cellconfig.h ${SRCDIR}include/ubik.h \
	${SRCDIR}include/afs/cmd.h ${SRCDIR}include/afs/butc.h \
	${SRCDIR}include/afs/tcdata.h ${SRCDIR}include/afs/bubasics.h \
	${SRCDIR}include/afs/butm.h

HACKS=${SRCDIR}lib/afs/libdir.a

INCLIBS=-L${SRCDIR}lib/afs -L${SRCDIR}lib

LIBS=${SRCDIR}lib/afs/libbudb.a \
	$(SRCDIR)/lib/afs/libbxdb.a \
	${SRCDIR}lib/afs/libbubasics.a \
        ${SRCDIR}lib/afs/libbutm.a \
	${SRCDIR}lib/afs/libvolser.a \
	${SRCDIR}lib/afs/libvldb.a \
        ${SRCDIR}lib/afs/vlib.a \
	${SRCDIR}/lib/afs/libacl.a \
        ${SRCDIR}lib/afs/libprot.a      \
        ${SRCDIR}lib/afs/libkauth.a \
        ${SRCDIR}lib/libubik.a \
        ${SRCDIR}lib/afs/libauth.a \
	${SRCDIR}lib/librxkad.a \
        ${SRCDIR}lib/afs/libsys.a  \
        ${SRCDIR}lib/libdes.a \
	${SRCDIR}lib/librx.a \
        ${SRCDIR}lib/afs/libsys.a  \
	${SRCDIR}lib/liblwp.a \
        ${SRCDIR}lib/afs/libcmd.a \
	${SRCDIR}lib/afs/libcom_err.a \
	${SRCDIR}lib/afs/util.a \
	${SRCDIR}lib/afs/libusd.a \
	${SRCDIR}lib/afs/libprocmgmt.a

TESTOBJS=test.o

SOBJS=dbentries.o tcprocs.o lwps.o tcmain.o list.o recoverDb.o tcudbprocs.o \
	dump.o tcstatus.o

include ../config/Makefile.version

noversion: install

all: butc read_tape

butc_test: ${TESTOBJS} ${LIBS} ${INCLS} ${HACKS}
	${CC}  ${CFLAGS} ${TESTOBJS} ${LIBS} ${XLIBS} -o butc_test

tdump: tdump.c AFS_component_version_number.c
	${CC} ${CFLAGS} tdump.c -o tdump

butc: ${SOBJS} ${LIBS} ${INCLS} ${HACKS}
	@case ${SYS_NAME} in \
	  rs_aix4*) ${CC} ${CFLAGS} ${SOBJS} ${LIBS} ${XLIBS} /usr/lib/libc_r.a -o butc;; \
	  *)        ${CC} ${CFLAGS} ${SOBJS} ${LIBS} ${XLIBS} -o butc;; \
	esac

tcmain.o: tcmain.c ${INCLS} AFS_component_version_number.c
dbentries.o: dbentries.c ${INCLS}
tcprocs.o: tcprocs.c ${INCLS}
test.o: test.c ${INCLS} AFS_component_version_number.c
lwps.o: lwps.c ${INCLS}
list.o: list.c ${INCLS}
recoverDb.o: recoverDb.c ${INCLS}
tcudbprocs.o: tcudbprocs.c ${INCLS}
dump.o: dump.c ${INCLS}
tcstatus.o: tcstatus.c ${INCLS}

read_tape: read_tape.c
	${CC} ${CFLAGS} -o read_tape read_tape.c \
		${SRCDIR}lib/afs/libcmd.a ${SRCDIR}lib/afs/util.a \
		${SRCDIR}lib/afs/libusd.a
	${INSTALL} read_tape ${SRCDIR}etc

clean:
	rm -f butc *.o butc_test core tdump read_tape AFS_component_version_number.c

system: install
install install.noversion: all
	@case ${SYS_NAME} in \
	alpha_dux*|sgi_*|sun4x_*|rs_aix*|*linux*|hp_ux*) \
		echo "Don't install butc for ${SYS_NAME} (will install from tbutc)" ;; \
	*) \
		echo ${INSTALL} butc ${DESTDIR}etc/butc ; \
		${INSTALL} butc ${DESTDIR}etc/butc ;; \
	esac

lint:	tcmain.c dbentries.c tcprocs.c test.c lwps.c list.c
	lint -hb ${INCDIRS} ${INCLIBS} tcmain.c
	lint -hb ${INCDIRS} ${INCLIBS} dbentries.c
	lint -hb ${INCDIRS} ${INCLIBS} tcprocs.c
	lint -hb ${INCDIRS} ${INCLIBS} lwps.c
	lint -hb ${INCDIRS} ${INCLIBS} list.c
	lint -hb ${INCDIRS} ${INCLIBS} test.c
	lint -hb ${INCDIRS} ${INCLIBS} recoverDb.c
	lint -hb ${INCDIRS} ${INCLIBS} tcudbprocs.c
 
