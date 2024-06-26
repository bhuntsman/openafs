SHELL=/bin/sh
COMPONENT=bozo
include ../config/Makefile.${SYS_NAME}

INSTALL=${SRCDIR}bin/install
COMPILE_ET=$(SRCDIR)bin/compile_et
CFLAGS=-g -I${SRCDIR}include ${XCFLAGS}
RPCINCLS=${SRCDIR}include/lwp.h ${SRCDIR}include/rx/rx.h bosint.h
INCLS=bnode.h ${RPCINCLS} ${SRCDIR}include/afs/auth.h \
     ${SRCDIR}include/afs/keys.h  ${SRCDIR}include/afs/cellconfig.h \
     ${SRCDIR}include/afs/cmd.h ${SRCDIR}include/afs/ktime.h

auditlib=${SRCDIR}/lib/afs/libaudit.a

#have to search libauth.a twice to get error stuff done right.
# EH 12/18/90 - have to search librx.a twice on Ultrix 4.0
LIBS=${SRCDIR}lib/librx.a ${SRCDIR}lib/liblwp.a \
     ${SRCDIR}lib/afs/libcmd.a ${SRCDIR}lib/afs/libkauth.a \
     ${SRCDIR}lib/afs/libvolser.a \
     ${SRCDIR}lib/afs/libvldb.a \
     ${SRCDIR}lib/afs/libauth.a \
     ${SRCDIR}lib/librxstat.a \
     ${SRCDIR}lib/librxkad.a ${SRCDIR}lib/libdes.a \
     ${SRCDIR}lib/librx.a \
     ${SRCDIR}lib/libubik.a \
     ${SRCDIR}lib/afs/libcom_err.a ${SRCDIR}lib/afs/util.a \
     ${SRCDIR}lib/afs/libsys.a \
     ${SRCDIR}lib/afs/libprocmgmt.a

OBJS=bosserver.o bnode.o ezbnodeops.o fsbnodeops.o bosint.ss.o bosint.xdr.o \
bosoprocs.o cronbnodeops.o

LIBOBJS=bosint.xdr.o bosint.cs.o boserr.o

include ../config/Makefile.version

noversion: install

all: bosserver bos libbos.a

$(OBJS) $(LIBOBJS): $(INCLS)

bosint.ss.o: bosint.ss.c ${RPCINCLS}
bosint.xdr.o: bosint.xdr.c ${RPCINCLS}
bosint.cs.o: bosint.cs.c ${RPCINCLS}

bosint.xdr.c bosint.ss.c bosint.cs.c bosint.h: bosint.xg
	${SRCDIR}bin/rxgen -x bosint.xg

bnode.h boserr.c: bnode.p.h boserr.et
	rm -f boserr.c bnode.h; $(COMPILE_ET) boserr -h bnode

bosserver.o: bosserver.c ${INCLS} AFS_component_version_number.o

cronbnodeops.o: cronbnodeops.c ${INCLS}

bnode.o: bnode.c ${INCLS}

bosoprocs.o: bosoprocs.c ${INCLS}

bos.o: bos.c ${INCLS} AFS_component_version_number.o

bos: bos.o $(LIBS) libbos.a
	${CC} ${CFLAGS} -o bos bos.o libbos.a $(LIBS)  ${XLIBS}

ezbnodeops.o: ezbnodeops.c ${INCLS}

fsbnodeops.o: fsbnodeops.c ${INCLS}

libbos.a: $(LIBOBJS) AFS_component_version_number.o
	-rm -f libbos.a
	${AR} r libbos.a ${LIBOBJS} AFS_component_version_number.o
	$(RANLIB) libbos.a

bosserver: $(OBJS) $(LIBS)
	${CC} $(CFLAGS) -o bosserver $(OBJS) ${auditlib} $(LIBS)  ${XLIBS} 

system: install

install: all
	${INSTALL} bosserver ${DESTDIR}root.server/usr/afs/bin
	${INSTALL} bosint.h ${DESTDIR}include/afs/bosint.h
	${INSTALL} bos ${DESTDIR}bin
	${INSTALL} bos ${DESTDIR}root.server/usr/afs/bin
	${INSTALL} libbos.a ${DESTDIR}lib/afs/libbos.a
	${INSTALL} bnode.h ${DESTDIR}include/afs/bnode.h

clean:
	rm -f  *.a *.o bos bosserver testproc bosint.cs.c bosint.ss.c bosint.xdr.c bosint.h core\
	AFS_component_version_number.c

test: 
	cd test; $(MAKE) 
