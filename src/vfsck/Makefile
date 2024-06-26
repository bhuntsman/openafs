#
# Copyright (c) 1987 The Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation,
# advertising materials, and other materials related to such
# distribution and use acknowledge that the software was developed
# by the University of California, Berkeley.  The name of the
# University may not be used to endorse or promote products derived
# from this software without specific prior written permission.
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
#	@(#)Makefile	5.12 (Berkeley) 6/26/89
#
COMPONENT=vfsck
include ../config/Makefile.${SYS_NAME}

INSTALL=${SRCDIR}bin/install
CFLAGS=	${DBUG} -w -I${SRCDIR}include ${XCFLAGS}
LIBC=	/lib/libc.a
SRCS=	dir.c inode.c main.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c setup.c utilities.c ufs_subr.c ufs_tables.c dirutils.c\
	vprintf.c proplist.c
LOCOBJS=dir.o inode.o main.o pass1.o pass1b.o pass2.o pass3.o pass4.o \
	pass5.o setup.o utilities.o dirutils.o vprintf.o proplist.o
SYSOBJS=ufs_subr.o ufs_tables.o
OBJS=	${LOCOBJS} ${SYSOBJS}
MAN =
#MAN=	fsck.0

include ../config/Makefile.version

all: vfsck

vfsck: main.o ${OBJS} 

${OBJS}:	${SRCS}
	case ${SYS_NAME} in 					\
	  hp_ux11? )                                            \
	    ${CC} -o vfsck -I/usr/old/usr/include ${DBUG} -w -I${SRCDIR}include ${SRCS} ${NON_SHARED} +DA1.0 -Wl,-a,archive -D_FILE64 \
			;;					\
	  hp?00_ux101 | hp_ux10? )				\
    	    ${CC} -o vfsck ${CFLAGS} ${SRCS} ${NON_SHARED} ${XLIBS} -D_FILE64\
			;;					\
	    * )							\
	    ${CC} -o vfsck ${CFLAGS} ${SRCS} ${NON_SHARED} ${XLIBS}\
			;;					\
	esac

main.o: main.c AFS_component_version_number.c

#${LOCOBJS}:
#	${CC} ${CFLAGS} -c $*.c

clean:
	rm -f ${OBJS} vfsck core AFS_component_version_number.c

cleandir: clean
	rm -f ${MAN} tags .depend

depend: ${SRCS}
	mkdep ${CFLAGS} ${SRCS}

system: install

install: all ${MAN}
	${INSTALL} -s -c vfsck ${DESTDIR}/root.server/etc
	${INSTALL} -s -c vfsck ${DESTDIR}/root.server/usr/afs/bin
	case ${SYS_NAME} in 				\
		hp_ux10? )				\
			${CP} bcheckrc ${DESTDIR}/root.server/etc; \
			;;				\
		hp_ux11? )				\
			${CP} bcheckrc-hp_ux110 ${DESTDIR}/root.server/etc/bcheckrc; \
			;;				\
	esac
	case ${SYS_NAME} in 				\
		hp_ux10? | hp_ux11? )			\
			 ${CP} mount ${DESTDIR}/root.server/etc ;\
			 ${CP} umount ${DESTDIR}/root.server/etc; \
			 chmod 755 ${DESTDIR}/root.server/etc/bcheckrc ${DESTDIR}/root.server/etc/mount ${DESTDIR}/root.server/etc/umount ;\
				;;					\
	esac
lint: ${SRCS}
	lint ${CFLAGS} ${SRCS}

tags: ${SRCS}
	ctags ${SRCS}
