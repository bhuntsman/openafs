
#
# (C) COPYRIGHT IBM CORPORATION 1987
# LICENSED MATERIALS - PROPERTY OF IBM
#
SHELL = /bin/sh
COMPONENT=viced
include ../config/Makefile.${SYS_NAME}

DBUG = -g
INSTALL = ${SRCDIR}bin/install
CFLAGS =  -DNINTERFACE ${DBUG} -I. -I${SRCDIR}include -I${SRCDIR}include/afs \
	 ${XCFLAGS} ${DBG_DEFS} -DRXDEBUG
LDFLAGS = ${DBUG} ${XLDFLAGS}

UTILLIB=${SRCDIR}lib/afs/util.a
auditlib=${SRCDIR}lib/afs/libaudit.a

LIBS=	${SRCDIR}lib/afs/libacl.a	\
	${SRCDIR}lib/afs/libvldb.a	\
	${SRCDIR}lib/afs/libprot.a	\
	${SRCDIR}lib/afs/libauth.a	\
	${SRCDIR}lib/librxkad.a		\
	${SRCDIR}lib/libubik.a 		\
	${SRCDIR}lib/afs/vlib.a		\
	${SRCDIR}lib/afs/libdir.a	\
	${UTILLIB}			\
	${SRCDIR}lib/afs/libsys.a	\
	${SRCDIR}lib/afs/libafsint.a	\
	${SRCDIR}lib/librxstat.a	\
	${SRCDIR}lib/librx.a		\
	${SRCDIR}lib/afs/libsys.a	\
	${SRCDIR}lib/libdes.a		\
	${SRCDIR}lib/afs/libcom_err.a	\
	${SRCDIR}lib/afs/util.a		\
	${SRCDIR}lib/liblwp.a 

headers=${SRCDIR}include/lwp.h		\
	${SRCDIR}include/afs/acl.h	\
	${SRCDIR}include/afs/errors.h	\
	${SRCDIR}include/afs/ptclient.h \
	${SRCDIR}include/lock.h		\
	${SRCDIR}include/afs/volume.h	\
	${SRCDIR}include/afs/vnode.h	\
	${SRCDIR}include/rx/rx.h	\
	${SRCDIR}include/rx/xdr.h	\
	${SRCDIR}include/afs/auth.h	\
	${SRCDIR}include/afs/afsint.h	\
	viced.h				\
	host.h				\
	fs_stats.h

objects=viced.o		\
	afsfileprocs.o	\
	host.o		\
	physio.o	\
	callback.o	

include ../config/Makefile.version

noversion: install

all: cbd fsprobe check_sysid

${objects}:${headers}
viced.o: AFS_component_version_number.o

check_sysid: check_sysid.c
	${CC} ${CFLAGS} -IDEST/include -o check_sysid check_sysid.c

fileserver: callback_clean1 viced.o ${objects} ${headers} ${LIBS}
	set -x; \
	case ${SYS_NAME} in \
	*linux*) \
	    ${CC} ${LDFLAGS} -o fileserver ${objects} \
	    ${auditlib} ${LIBS} ${XLIBS} ;; \
	rs_aix*) \
	    ${CC} -K ${LDFLAGS} -o fileserver ${objects} \
	    ${auditlib} ${LIBS} ${XLIBS} ;; \
	*) \
	    ${CC} ${LDFLAGS} -o fileserver ${objects} \
	    ${auditlib} ${LIBS} ${XLIBS} ;; \
	esac

fsprobe: fsprobe.c  AFS_component_version_number.c
	${CC} ${CFLAGS} -DINTERPRET_DUMP -o fsprobe fsprobe.c ${LIBS} ${XLIBS}  
cbd: callback_clean2 callback.c AFS_component_version_number.c
	${CC} ${CFLAGS} -DINTERPRET_DUMP -o cbd callback.c ${LIBS} ${XLIBS}  

lint:
	lint -uvn  -I${SRCDIR}include viced.c afsfileprocs.c host.c physio.c callback.c ${SRCDIR}lib/afs/llib-lutil.ln

${DESTDIR}root.server/usr/afs/bin/fileserver : fileserver
	@case ${SYS_NAME} in \
	alpha_dux4*|*linux*|rs_aix*|sgi_6*|sun4x*) \
	echo "Don't install fileserver for ${SYS_NAME}" ;; \
	*) \
	echo ${INSTALL} -ns fileserver ${DESTDIR}root.server/usr/afs/bin/fileserver ; \
	${INSTALL} -ns fileserver ${DESTDIR}root.server/usr/afs/bin/fileserver ;; \
	esac

install: all  ${DESTDIR}root.server/usr/afs/bin/fileserver
	${INSTALL} fs_stats.h ${DESTDIR}/include/afs

system: install

callback_clean1 callback_clean2: 
	case ${SYS_NAME} in \
	ncrx86_*) \
		${RM}  -f callback.o;\
		echo ${RM} callback.o;;\
	esac;

clean:
	rm -f *.o llib-lvice.ln fileserver core AFS_component_version_number.c

beancount:
	wc -l *.c *.h
