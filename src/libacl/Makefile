
SHELL = /bin/sh
COMPONENT=libacl
include ../config/Makefile.${SYS_NAME}

INSTALL = ${SRCDIR}bin/install
INCDIRS= -I${DESTDIR}include -I.. -I${BACKDIR}include -I${DESTDIR}include/afs 
KERNELDIR = ../libafs/
UKERNELDIR = ../libuafs/

CFLAGS = ${OPTMZ} ${INCDIRS} ${LDIRS} ${LIBS} ${XCFLAGS}

LIBOBJS=aclprocs.o netprocs.o

include ../config/Makefile.version

all: libacl.a

libacl.a: $(LIBOBJS)  AFS_component_version_number.o
	rm -f libacl.a
	ar rv libacl.a $(LIBOBJS) AFS_component_version_number.o
	$(RANLIB) libacl.a
	
aclprocs.o: aclprocs.c acl.h

netprocs.o: netprocs.c acl.h

clean: 
	rm -f acltest *.o libacl.a core AFS_component_version_number.c

system: install

kinstall:
	${INSTALL} prs_fs.h ${KERNELDIR}afs/prs_fs.h

ukinstall webinstall:
	${INSTALL} prs_fs.h ${UKERNELDIR}afs/prs_fs.h

install: libacl.a kinstall ukinstall
	${INSTALL} libacl.a ${DESTDIR}lib/afs/libacl.a
	${INSTALL} acl.h ${DESTDIR}include/afs/acl.h
	${INSTALL} prs_fs.h ${DESTDIR}include/afs/prs_fs.h

test:
	cd test; $(MAKE)
