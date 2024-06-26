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
# @(#)Makefile	5.1 (Berkeley) 10/18/88
#

COMPONENT=login
include ../config/Makefile.${SYS_NAME}

LIBDIR = ${DESTDIR}lib/
INSTALL=${SRCDIR}bin/install
AFSLIBS = ${LIBDIR}afs/libkauth.a ${LIBDIR}afs/libprot.a ${LIBDIR}libubik.a \
	  ${LIBDIR}afs/libauth.a ${DESTDIR}lib/librxkad.a ${LIBDIR}afs/libsys.a \
	  ${DESTDIR}lib/libdes.a ${LIBDIR}librx.a ${LIBDIR}liblwp.a \
          ${LIBDIR}afs/libcmd.a ${LIBDIR}afs/libcom_err.a ${LIBDIR}afs/util.a
KAFSLIBS = ${LIBDIR}afs/libkauth.krb.a ${LIBDIR}afs/libprot.a ${LIBDIR}libubik.a \
	  ${LIBDIR}afs/libauth.krb.a ${DESTDIR}lib/librxkad.a ${LIBDIR}afs/libsys.a \
	  ${DESTDIR}lib/libdes.a ${LIBDIR}librx.a ${LIBDIR}liblwp.a \
          ${LIBDIR}afs/libcmd.a ${LIBDIR}afs/libcom_err.a ${LIBDIR}afs/util.a
INCLUDES = -I${DESTDIR}include -I/usr/include -I/usr/include/sys
CFLAGS=	-g ${XCFLAGS} ${INCLUDES}
LIBC=	/lib/libc.a
SRCS=	login.c 
OBJS=   login.o setenv.o getenv.o libutil.a
KOBJS=  login.krb.o setenv.o getenv.o libutil.a
LIBS=   ${AFSLIBS} 
KLIBS=   ${KAFSLIBS}
SLIBS=   ${AFSLIBS} ${LIBC}
SKLIBS=   ${KAFSLIBS} ${LIBC}
MAN=
UTILOBJS = util_login.o util_logout.o util_logwtmp.o

include ../config/Makefile.version

all: login login.krb

login:	${OBJS} ${LIBS}
	case ${SYS_NAME} in \
	sun4x_5? ) ${CC} -g -o $@ ${CFLAGS} ${OBJS} -lc ${LIBS} ${XLIBS} -lcmd;;\
	*) ${CC} -g -o $@ ${CFLAGS} ${OBJS} ${LIBS} ${XLIBS} ;;\
	esac ;

login.krb: ${KOBJS} ${KLIBS}
	case ${SYS_NAME} in \
	sun4x_5? ) ${CC} -g -o $@ ${CFLAGS} ${KOBJS} -lc ${KLIBS} ${XLIBS} -lcmd;;\
	*) ${CC} -g -o $@ ${CFLAGS} ${KOBJS} ${KLIBS} ${XLIBS} ;;\
	esac ;

libutil.a : ${UTILOBJS}
	@rm -f $@
	ar cru $@ ${UTILOBJS}
	${RANLIB} $@

login.o: login.c AFS_component_version_number.c
	${CC} ${CFLAGS} -c login.c

login.krb.o: login.c
	${CC} ${CFLAGS} -DAFS_KERBEROS_ENV -c login.c
	mv login.o login.krb.o

clean:
	rm -f ${OBJS} core login AFS_component_version_number.c

cleandir: clean
	rm -f ${MAN} tags .depend

depend: ${SRCS}
	mkdep -p ${CFLAGS} ${SRCS}

install: ${MAN} all
	${INSTALL} -s login ${DESTDIR}bin/login
	${INSTALL} -s login.krb ${DESTDIR}bin/login.krb

lint: ${SRCS}
	lint ${CFLAGS} ${SRCS}

tags: ${SRCS}
	ctags ${SRCS}

system: install
