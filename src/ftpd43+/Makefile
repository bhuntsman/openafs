#
# Copyright (c) 1988 Regents of the University of California.
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
#	@(#)Makefile	5.8 (Berkeley) 9/22/88


COMPONENT=ftpd43+
include ../config/Makefile.version
include ../config/Makefile.${SYS_NAME}

LIBDIR = ${DESTDIR}lib/
AFSLIBS = ${LIBDIR}afs/libkauth.a ${LIBDIR}afs/libprot.a ${LIBDIR}libubik.a \
	  ${LIBDIR}afs/libauth.a ${DESTDIR}lib/librxkad.a ${LIBDIR}afs/libsys.a \
	  ${DESTDIR}lib/libdes.a ${LIBDIR}librx.a ${LIBDIR}liblwp.a \
          ${LIBDIR}afs/libcmd.a ${LIBDIR}afs/libcom_err.a ${LIBDIR}afs/util.a \
          ${LIBDIR}afs/libaudit.a

LIBS = ${AFSLIBS} 
CFLAGS=	${OPTMZ} -I${DESTDIR}include ${XCFLAGS}
LIBC=	/lib/libc.a 
SRCS=	ftpd.c ftpcmd.c getusershell.c glob.c logwtmp.c popen.c vers.c
OBJS=	ftpd.o ftpcmd.o getusershell.o glob.o logwtmp.o popen.o vers.o
MAN=	ftpd.8


noversion: install

all: ftpd

system: install

ftpd: ${OBJS} ${LIBS}
	case ${SYS_NAME} in \
	sun4c_53 | sun4m_53 | sun4_53 | sun4_54 | sun4c_54 | sun4m_54 | sun4x_5? | sunx86_54) \
		${CC} -o $@ ${OBJS} ${LIBS} ${XLIBS} -lsocket -lnsl -lauth -ldl ;; \
	rs_aix*) \
		${CC} -o $@ ${OBJS} ${LIBS} ${XLIBS} -ls -lbsd  ;; \
	  * ) 	${CC} -o $@ ${OBJS} ${LIBS} ${XLIBS} ;; \
	esac

ftpd.o: ftpd.c AFS_component_version_number.c
	case ${SYS_NAME} in \
	rs_aix* ) \
		${CC} ${CFLAGS} -D_BSD -c $< ;; \
	* ) \
		${CC} ${CFLAGS} -c $< ;; \
	esac

vers.o: ftpd.c ftpcmd.y
#	sh newvers.sh
	${CC} ${CFLAGS} -c vers.c

clean:
	rm -f ${OBJS} ftpd core ftpcmd.c AFS_component_version_number.c

cleandir: clean
	rm -f ${MAN} tags .depend

depend: ${SRCS}
	mkdep ${CFLAGS} ${SRCS}

install: ${MAN} all
	${INSTALL} -s -m 755 ftpd ${DESTDIR}/etc/ftpd

lint: ${SRCS}
	lint ${CFLAGS} ${SRCS}

tags: ${SRCS}
	ctags ${SRCS}
