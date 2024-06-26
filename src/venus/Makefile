SHELL = /bin/sh
COMPONENT=venus
include ../config/Makefile.${SYS_NAME}

INSTALL = ${DESTDIR}bin/install
INSTALLex = ${DESTDIR}bin/install -m 755
CCXPG2= /usr/xpg2bin/cc

CFLAGS = ${OPTMZ} -I${DESTDIR}include -I${SRCDIR}include ${XCFLAGS}

INCLS = ${DESTDIR}include/afs/afsint.h \
	${DESTDIR}include/afs/cmd.h \
	${DESTDIR}include/afs/afsutil.h

VLIB = ${DESTDIR}lib/afs/

LIB = ${DESTDIR}lib/

FSLIBS = ${VLIB}libsys.a \
	 ${VLIB}libvldb.a \
	 ${LIB}libubik.a \
	 ${VLIB}vlib.a \
	 ${VLIB}libauth.a \
	 ${DESTDIR}lib/librxkad.a \
	 ${VLIB}libcom_err.a \
	 ${VLIB}libcmd.a \
	 ${VLIB}libkauth.a \
	 ${DESTDIR}lib/libdes.a \
	 ${LIB}librx.a \
	 ${VLIB}libsys.a \
	 $(VLIB)util.a \
	 ${LIB}liblwp.a \
	 ${VLIB}libaudit.a

CMLIBS = ${VLIB}libsys.a \
	 ${VLIB}libafsint.a \
	 ${DESTDIR}lib/librxkad.a \
	 ${VLIB}libauth.a \
	 ${VLIB}libcom_err.a \
	 ${VLIB}libcmd.a \
	 ${LIB}librx.a \
	 ${VLIB}libsys.a \
	 $(VLIB)util.a \
	 ${LIB}liblwp.a 

LIBS = ${FSLIBS} 

include ../config/Makefile.version

cacheout: cacheout.o
	$(CC) ${CFLAGS} -o cacheout cacheout.o ${LIBS} ${XLIBS} ${CMLIBS}

cacheout.o: cacheout.c

all: fs up cmdebug fstrace kdump gcpags kdump64

${DESTDIR}bin/fs ${DESTDIR}root.server/usr/afs/bin/fs: fs
	${INSTALL} -s $? $@

${DESTDIR}bin/up: up
	${INSTALL} -s $? $@

${DESTDIR}etc/fstrace: fstrace
	${INSTALL} -s $? $@

${DESTDIR}bin/cmdebug: cmdebug
	${INSTALL} -s $? $@

${DESTDIR}etc/kdump: kdump
	set -x; \
	case ${SYS_NAME} in \
	sgi_6? ) \
		${INSTALLex} -f kdump.sh.sgi_ipnos $@; \
		ln -fs kdump ${DESTDIR}etc/kdump32; \
		ln -fs kdump.IP20 ${DESTDIR}etc/kdump.IP22; \
		ln -fs kdump.IP20 ${DESTDIR}etc/kdump.IP32; \
		for f in kdump.IP??; \
		do ${INSTALL} -s $$f ${DESTDIR}etc/$$f || exit $$? ; \
		done ;; \
	sun4x_57 ) \
		${INSTALLex} -f kdump.sh.solaris7 $@; \
		${INSTALL} -s -f $? ${DESTDIR}etc/kdump32;; \
	hp_ux11* ) \
		${INSTALLex} -f kdump.sh.hp_ux11 $@; \
		${INSTALL} -s -f $? ${DESTDIR}etc/kdump32;; \
	*) \
		${INSTALL} -s $? $@ ;; \
	esac

${DESTDIR}etc/kdump64: kdump64
	set -x; \
	case ${SYS_NAME} in \
	sun4x_57 | hp_ux11* ) \
		${INSTALL} -s $? $@ ;;\
	* ) \
		echo skipping kdump64 for ${SYS_NAME} ;; \
	esac


install: all ${DESTDIR}bin/fs ${DESTDIR}root.server/usr/afs/bin/fs \
	${DESTDIR}bin/up ${DESTDIR}etc/fstrace ${DESTDIR}bin/cmdebug \
	${DESTDIR}etc/kdump ${DESTDIR}etc/kdump64

system: install

up.o: up.c AFS_component_version_number.c

up: up.o
	${CC} ${CFLAGS} -g -o up up.o ${LIBS} ${XLIBS}

fs.o: fs.c ${INCLS} AFS_component_version_number.c

fs: fs.o $(LIBS)
	${CC} ${CFLAGS} -g -o fs fs.o ${DESTDIR}lib/afs/libprot.a $(LIBS) ${XLIBS}

twiddle: twiddle.c $(LIBS)
	${CC} -g -o twiddle $(CFLAGS) twiddle.c $(LIBS) ${XLIBS}

gcpags: gcpags.c $(LIBS)
	${CC} -g -o gcpags $(CFLAGS) gcpags.c $(LIBS) ${XLIBS}

whatfid.o: whatfid.c ${INCLS} AFS_component_version_number.c

whatfid: whatfid.o ${LIBS}
	${CC} ${CFLAGS} -g -o whatfid whatfid.o ${LIBS} ${XLIBS}

fstrace.o: fstrace.c AFS_component_version_number.c
	case ${SYS_NAME} in \
		sun4_411 | sun4c_411 | sun4m_412 ) \
			${CCXPG2} -g -I${DESTDIR}include -I${SRCDIR}include -c fstrace.c ;; \
		* ) \
			${CC} ${CFLAGS} -g -I${DESTDIR}include -I${SRCDIR}include -c fstrace.c ;; \
	esac

fstrace: fstrace.o
	case ${SYS_NAME} in \
		pmax_ul43 | pmax_ul43a ) \
			${CC} ${CFLAGS} -o fstrace fstrace.o ${VLIB}/libsys.a ${VLIB}libcmd.a ${VLIB}util.a /usr/lib/libi.a ;; \
		sun4_411 | sun4c_411 | sun4m_412 ) \
			${CCXPG2} ${CFLAGS} -g -o fstrace fstrace.o ${VLIB}/libsys.a ${VLIB}libcmd.a ${VLIB}util.a ;; \
		hp700_ux100 | hp800_ux100 | hp?00_ux10? | hp_ux10? | hp_ux11?) \
			${CC}  -o fstrace fstrace.o ${VLIB}/libsys.a ${VLIB}libcmd.a ${VLIB}util.a ;; \
		* ) \
			${CC} ${CFLAGS} -o fstrace fstrace.o ${VLIB}/libsys.a ${VLIB}libcmd.a ${VLIB}util.a ;; \
	esac

cmdebug.o: cmdebug.c ${INCLS} AFS_component_version_number.c

cmdebug: cmdebug.o ${CMLIBS}
	$(CC) -o cmdebug cmdebug.o ${CFLAGS} ${CMLIBS} ${XLIBS}

kdump.o: kdump.c ${INCLS} AFS_component_version_number.c
	set -x; \
	case ${SYS_NAME} in \
		alpha_osf1 | alpha_osf20 |  alpha_osf30 | alpha_osf32 | alpha_osf32c| alpha_dux?? ) \
			${CC} -g ${CFLAGS} -I/usr/sys/include -I/usr/sys/BINARY -I/usr/sys/AFS -DDEBUGGER -c kdump.c ;;\
		sgi_6? ) \
			for f in ../libafs/STATIC.IP*/CPU_KDEFS; \
			do	IP=`expr "$$f" : '../libafs/STATIC.\(IP..\)'`; \
				CPU_KDEFS=`sed 's/-mips.//' $$f`; \
				echo IP = $$IP; \
				echo CPU_KDEFS = $$CPU_KDEFS; \
				case $$CPU_KDEFS in \
				*-64*)	${CC} -D_KMEMUSER -woff 1178 \
						-g -I${DESTDIR}include \
						$$CPU_KDEFS \
						${XCFLAGS64} \
						-c kdump.c -o kdump.$$IP.o \
					;; \
				*)	${CC} -D_KMEMUSER -woff 1178 \
						-g -I${DESTDIR}include \
						$$CPU_KDEFS \
						${XCFLAGS} -DAFS_32BIT_KERNEL_ENV \
						-c kdump.c -o kdump.$$IP.o \
					;; \
				esac || exit $$?; \
			done \
			;; \
		*) \
			${CC} -g -I${DESTDIR}include -I${SRCDIR}include ${XCFLAGS} -c kdump.c ;; \
	esac ;

kdump64.o : kdump.c ${INCLS} AFS_component_version_number.c
	set -x; \
	case ${SYS_NAME} in \
	sun4x_57 | hp_ux11* ) \
		${CC} -g -I${DESTDIR}include -I${SRCDIR}include ${XCFLAGS64} -o kdump64.o -c kdump.c ;; \
	esac

kdump:  kdump.o
	set -x; \
	case ${SYS_NAME} in \
	sun4c_51 | sun4c_52 | sun4m_51 | sun4m_52 | sun4c_53 | sun4m_53 | sun4_53 | sun4_54 | sun4c_54 | sun4m_54 | sunx86_54) \
		${CC} -g -o kdump kdump.o ${DESTDIR}lib/afs/libcmd.a  ${DESTDIR}lib/afs/util.a /usr/lib/libkvm.a -lelf ${XLIBS} ;; \
	sun4x_5? ) \
		${CC} -g -o kdump kdump.o ${DESTDIR}lib/afs/libcmd.a  ${DESTDIR}lib/afs/util.a  ${XLIBELFA}  ${XLIBKVM} ${XLIBS} ;; \
	sgi_6? ) \
		for f in ../libafs/STATIC.IP*/CPU_KDEFS; \
		do	IP=`expr "$$f" : '../libafs/STATIC.\(IP..\)'`; \
			CPU_KDEFS=`sed 's/-mips.//' $$f`; \
			echo IP = $$IP; \
			echo CPU_KDEFS = $$CPU_KDEFS; \
			case $$CPU_KDEFS in \
			*-64*)	${CC} ${XCFLAGS64} -g \
					$$CPU_KDEFS \
					-o kdump.$$IP kdump.$$IP.o \
					${DESTDIR}lib/afs/libcmd64.a -lelf \
				;; \
			*) 	${CC} ${XCFLAGS} -g \
					$$CPU_KDEFS \
					-o kdump.$$IP kdump.$$IP.o \
					${DESTDIR}lib/afs/libcmd.a -lelf \
				;; \
			esac || exit $$? ; \
		done ;; \
	alpha_osf1 | alpha_osf20 |  alpha_osf30 | alpha_osf32 | alpha_osf32c | alpha_dux??) \
	    ${CC} -g -o kdump kdump.o ${DESTDIR}lib/afs/libcmd.a  ${DESTDIR}lib/afs/util.a ${XLIBS} -ll -lmld;; \
	ncrx86_* ) ${CC} -g -o kdump kdump.o ${DESTDIR}lib/afs/libcmd.a  ${DESTDIR}lib/afs/util.a -lelf ${XLIBS} ;; \
	* )     ${CC} -g -o kdump kdump.o ${DESTDIR}lib/afs/libcmd.a  ${DESTDIR}lib/afs/util.a ${XLIBS} ;; \
	esac

## kdump to read from 64 bit kernel dumps

kdump64 : kdump64.o
	set -x; \
	case ${SYS_NAME} in \
	sun4x_57 | hp_ux11* )  \
		${CC} -g ${XCFLAGS64} -o kdump64 kdump64.o ${DESTDIR}lib/afs/libcmd64.a ${XLIBELFA}  ${XLIBKVM} ${XLIBS} ;; \
	esac

clean:
	rm -f *.o *.a up fs core cmdebug AFS_component_version_number.c

beancount:
	wc -l *.c *.h

test:
	cd test; $(MAKE)

