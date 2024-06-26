#
# Copyright (c) 1988 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation, advertising
# materials, and other materials related to such redistribution and
# use acknowledge that the software was developed by the University
# of California, Berkeley.  The name of the University may not be
# used to endorse or promote products derived from this software
# without specific prior written permission.  THIS SOFTWARE IS PROVIDED
# ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
# WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND
# FITNESS FOR A PARTICULAR PURPOSE.
#
# @(#)Makefile	5.1 (Berkeley) 9/20/88
#
SHELL=/bin/sh
COMPONENT=rsh
include ../config/Makefile.${SYS_NAME}

INSTALL = ${SRCDIR}bin/install
LIBDIR = ${DESTDIR}lib/
INCLUDES= -I${SRCDIR}include
CFLAGS=	${DBUG} ${INCLUDES} ${XCFLAGS}
LIBC=	/lib/libc.a
OBJS=   rcmd.o ../inetd/ta-rauth.o herror.o
MAN=	
SRCS=	rsh.c rcmd.c ../inetd/ta-rauth.c herror.c AFS_component_version_number.c
AFSLIBS =  ${LIBDIR}afs/libkauth.a ${LIBDIR}libubik.a\
           ${LIBDIR}afs/libauth.a ${LIBDIR}afs/libsys.a \
           ${LIBDIR}librx.a ${LIBDIR}liblwp.a ${DESTDIR}lib/librxkad.a\
	   ${LIBDIR}afs/libsys.a \
           ${DESTDIR}lib/libdes.a ${LIBDIR}afs/libcmd.a \
           ${LIBDIR}afs/libcom_err.a ${LIBDIR}afs/util.a
LIBS = ${AFSLIBS} 

include ../config/Makefile.version

all: rlogin rsh

system: install

rsh: rsh.o ${OBJS} ${LIBS}
	${CC} -o $@ ${CFLAGS}  rsh.o ${OBJS} ${LIBS} ${XLIBS}

rlogin: rlogin.o ${OBJS} ${LIBS}
	${CC} -o $@ ${CFLAGS} rlogin.o ${OBJS} ${LIBS} ${XLIBS}

rlogin.o: rlogin.c AFS_component_version_number.c
rsh.o: rsh.c AFS_component_version_number.c

../rlogind/herror.o: ../rlogind/herror.c
	(cd ../rlogind ; make herror.o )
../inetd/ta-rauth.o : ../inetd/ta-rauth.c
	(cd ../inetd ; make ta-rauth.o DESTDIR=${DESTDIR})

clean:
	rm -f ${OBJS} core rsh *.BAK AFS_component_version_number.c

cleandir: clean
	rm -f ${MAN} tags .depend

depend: ${SRCS}
	mkdep -p ${CFLAGS} ${SRCS}

install: ${MAN} all
	set -x; \
	case "${SYS_NAME}" in \
	hp* ) \
		${INSTALL} -m 4755 -f rsh ${DESTDIR}bin/remsh ;; \
	* ) \
		${INSTALL} -m 4755 rsh ${DESTDIR}bin/rsh ;; \
	esac

lint: ${SRCS}
	lint ${CFLAGS} ${SRCS}

tags: ${SRCS}
	ctags ${SRCS}
