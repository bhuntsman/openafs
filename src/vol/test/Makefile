SHELL=/bin/sh
COMPONENT=vol
include ../../config/Makefile.${SYS_NAME}

INCDIRS= -I. -I.. -I${DESTDIR}include ${FSINCLUDES}
INSTALL = ${SRCDIR}bin/install
LDFLAGS = ${OPTMZ} ${PROF} ${LDIRS} ${XLDFLAGS}

LIBS= ${DESTDIR}lib/afs/libcmd.a ${DESTDIR}lib/afs/vlib.a \
	${SRCDIR}lib/afs/util.a \
	${DESTDIR}lib/afs/libsys.a ${DESTDIR}lib/afs/libdir.a \
	${DESTDIR}lib/liblwp.a  ${DESTDIR}lib/afs/libacl.a ${XLIBS}

NAMEI_LIBS = ../vlib.a ${SRCDIR}lib/afs/util.a ${DESTDIR}lib/liblwp.a

CFLAGS = -D${SYS_NAME} ${OPTMZ} ${INCDIRS} ${XCFLAGS} -g

PROGS = listVicepx

PUBLICHEADERS=../nfs.h ../vnode.h ../viceinode.h ../volume.h ../vldb.h \
	../voldefs.h ../partition.h ../fssync.h ../inodeops.h

OBJECTS=listVicepx.o utilities.o  updateDirInode.o

all system: install

kinstall:
	@echo No kernel source here.

install:  listVicepx updateDirInode testpart

${OBJECTS}:	${PUBLICHEADERS} ${SRCDIR}include/lwp.h \
		${SRCDIR}include/lock.h  ${SRCDIR}include/afs/afsint.h \
		../filesignal.h ../vutils.h ../vldb.h ../salvage.h \
		../volinodes.h ../qiin.h 

listVicepx.o:	listVicepx.c  utilities.c
		${CC} ${CFLAGS} -c listVicepx.c
		${CC} ${CFLAGS} -c utilities.c

testpart:	testpart.c ../vlib.a
		${CC} ${CFLAGS} ${LDFLAGS} -o $@ testpart.c \
		${DESTDIR}lib/afs/vlib.a ${DESTDIR}lib/liblwp.a \
		${DESTDIR}lib/afs/util.a ${XLIBS}
		

listVicepx:	listVicepx.o utilities.o 
		${CC} ${LDFLAGS} -o listVicepx listVicepx.o utilities.o ${LIBS} 

updateDirInode.o:	updateDirInode.c  
		${CC} ${CFLAGS} -c updateDirInode.c

updateDirInode:	updateDirInode.o 
		${CC} ${LDFLAGS} -o updateDirInode updateDirInode.o ${LIBS} 


NAMEI_PROGS = nicreate nincdec nino nilist
namei: ${NAMEI_PROGS}

nicreate: nicreate.o nsubr.o
	${CC} ${CFLAGS} -o nicreate nicreate.o nsubr.o ${NAMEI_LIBS}

nincdec: nincdec.o nsubr.o
	${CC} ${CFLAGS} -o nincdec nincdec.o nsubr.o ${NAMEI_LIBS}

nilist: nilist.o nsubr.o
	${CC} ${CFLAGS} -o nilist nilist.o nsubr.o ${NAMEI_LIBS}

nino: nino.o
	${CC} ${CFLAGS} -o nino nino.o ${DESTDIR}lib/afs/util.a

clean:
	rm -f *.o *.a 
	rm -f ${SCMPROGS} ${STAGEPROGS} core listVicepx updateDirInode

beancount:
	wc -l *.c *.h
