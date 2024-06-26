#
# COPYRIGHT (C) IBM CORPORATION 1988
# LICENSED MATERIALS - PROPERTY OF IBM
#

SHELL = /bin/sh
include ../config/Makefile.${SYS_NAME}
include ../config/Makefile.version

KERNELDIR = ../libafs/
UKERNELDIR = ../libuafs/
INSTALL=${SRCDIR}bin/install
RXGEN=${SRCDIR}bin/rxgen
MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

KSRCS=rxstat.c rxstat.h Krxstat.ss.c Krxstat.xdr.c

CFLAGS = $(DBUG) -I${SRCDIR}include ${XCFLAGS}

LIBOBJS = rxstat.cs.o rxstat.ss.o rxstat.xdr.o rxstat.o

rxstat.cs.c rxstat.ss.c rxstat.xdr.c rxstat.h: rxstat.xg
	${RXGEN} -x rxstat.xg

librxstat.a: ${LIBOBJS} AFS_component_version_number.o
	-rm -f $@
	ar rv $@ AFS_component_version_number.o ${LIBOBJS}
	$(RANLIB) $@

kinstall: ${KSRCS} 
	set ${KERNELDIR}afsint; $(MKDIR_IF_NEEDED)
	${INSTALL} ${KSRCS} ${KERNELDIR}afsint

ukinstall webinstall:  ${KSRCS}
	set ${UKERNELDIR}afsint; $(MKDIR_IF_NEEDED)
	${INSTALL} ${KSRCS} ${UKERNELDIR}afsint

Krxstat.cs.c: rxstat.xg
	${RXGEN} -x -k -C -o $@ rxstat.xg

Krxstat.ss.c: rxstat.xg
	${RXGEN} -x -k -S -o $@ rxstat.xg

Krxstat.xdr.c: rxstat.xg
	${RXGEN} -x -k -c -o $@ rxstat.xg

install: librxstat.a includes kinstall ukinstall
	${INSTALL} librxstat.a ${DESTDIR}lib

includes: rxstat.h
	${INSTALL} rxstat.h ${DESTDIR}include/rx

clean:
	rm -f rxstat.h *.xdr.c *.cs.c *.ss.c *.o *.a core *_component_version_number.c
