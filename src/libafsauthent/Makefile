#/* Copyright (C) 1998 Transarc Corporation - All rights reserved */


# This is a pthread safe library containing ubikclient, auth, kauth.

SHELL = /bin/sh
COMPONENT=libafsauthent
include ../config/Makefile.${SYS_NAME}

CC=${MT_CC}
INSTALL = ${SRCDIR}bin/install
CFLAGS = ${OPTMZ} ${DBG} -I${SRCDIR}include ${MT_CFLAGS} 
CCRULE = ${CC} ${CFLAGS} -c $?

AUDIT= ../audit
AUTH = ../auth
KAUTH = ../kauth
UBIK = ../ubik
UTIL = ../util
RXKAD = ../rxkad
PTSERVER = ../ptserver
SYS = ../sys

AUDITOBJS = audit.o

AUTHOBJS = \
	cellconfig.o \
	ktc.o \
	userok.o \
	writeconfig.o \
	authcon.o \
	ktc_errors.o \
	acfg_errors.o

KAUTHOBJS = \
	kauth.xdr.o \
	kauth.cs.o \
	kaaux.o \
	client.o \
	authclient.o \
	token.o \
	kautils.o \
	kalocalcell.o \
	kaerrors.o \
	user.o \
	read_passwd.o

UBIKOBJS = \
	ubikclient.o \
	uerrors.o \
	ubik_int.cs.o \
	ubik_int.xdr.o

UTILOBJS = \
	pthread_glock.o \
	get_krbrlm.o \
	dirpath.o \
	fileutil.o \
	casestrcpy.o

RXKADOBJS = \
	rxkad_errs.o

SYSOBJS = \
	rmtsysc.o \
	rmtsys.xdr.o \
	rmtsys.cs.o \
	afssyscalls.o \
	rmtsysnet.o

PTSERVEROBJS = \
	ptclient.o \
	ptint.cs.o \
	ptint.xdr.o \
	ptuser.o \
	display.o \
	pterror.o

LIBOBJS = \
	${AUDITOBJS} \
	${AUTHOBJS} \
	${KAUTHOBJS} \
	${UBIKOBJS} \
	${UTILOBJS} \
	${RXKADOBJS} \
	${PTSERVEROBJS} \
	${SYSOBJS}

system install: ${DESTDIR}lib/libafsauthent.a

${DESTDIR}lib/libafsauthent.a: libafsauthent.a
	${INSTALL} libafsauthent.a ${DESTDIR}lib/libafsauthent.a

libafsauthent.a: ${LIBOBJS}
	rm -f libafsauthent.a
	${AR} -r libafsauthent.a ${LIBOBJS}
	${RANLIB} libafsauthent.a

audit.o: ${AUDIT}/audit.c
	${CCRULE};

cellconfig.o: ${AUTH}/cellconfig.c
	${CCRULE};

ktc.o: ${AUTH}/ktc.c
	${CCRULE};

userok.o: ${AUTH}/userok.c
	${CCRULE};

writeconfig.o: ${AUTH}/writeconfig.c
	${CCRULE};

authcon.o: ${AUTH}/authcon.c
	${CCRULE};

ktc_errors.o: ${AUTH}/ktc_errors.c
	${CCRULE};

acfg_errors.o: ${AUTH}/acfg_errors.c
	${CCRULE};

kauth.xdr.o: ${KAUTH}/kauth.xdr.c
	${CCRULE};

kauth.cs.o: ${KAUTH}/kauth.cs.c
	${CCRULE};

kaaux.o: ${KAUTH}/kaaux.c
	${CCRULE};

client.o: ${KAUTH}/client.c
	${CCRULE};

authclient.o: ${KAUTH}/authclient.c
	${CCRULE};

token.o: ${KAUTH}/token.c
	${CCRULE};

kautils.o: ${KAUTH}/kautils.c
	${CCRULE};

kalocalcell.o: ${KAUTH}/kalocalcell.c
	${CCRULE};

kaerrors.o: ${KAUTH}/kaerrors.c
	${CCRULE};

user.o: ${KAUTH}/user.c
	${CCRULE};

read_passwd.o: ${KAUTH}/read_passwd.c
	${CCRULE};

ubikclient.o: ${UBIK}/ubikclient.c
	${CCRULE};

uerrors.o: ${UBIK}/uerrors.c
	${CCRULE};

ubik_int.cs.o: ${UBIK}/ubik_int.cs.c
	${CCRULE};

ubik_int.xdr.o: ${UBIK}/ubik_int.xdr.c
	${CCRULE};

get_krbrlm.o: ${UTIL}/get_krbrlm.c
	${CCRULE};

dirpath.o: ${UTIL}/dirpath.c
	${CCRULE};

fileutil.o: ${UTIL}/fileutil.c
	${CCRULE};

casestrcpy.o: ${UTIL}/casestrcpy.c
	${CCRULE}

pthread_glock.o: ${UTIL}/pthread_glock.c
	${CCRULE};

rxkad_errs.o: ${RXKAD}/rxkad_errs.c
	${CCRULE};

ptclient.o: ${PTSERVER}/ptclient.c
	${CCRULE};

# The special treatment of this file for hp_ux110 is because of a bug
# in version A.11.01.00 of the HP C compiler.  This bug appears to be
# fixed in version A.11.01.02 of the HP C compiler, however this version
# of the compiler is not installed on all of our build machines.
# The symptom of the problem is an error when linking the pthread fileserver:
# /usr/ccs/bin/ld: TP override with DATA_ONE_SYM fixup for non thread local
# storage symbol pr_Initialize in file DEST/lib/libafsauthent.a(ptuser.o)
ptuser.o: ${PTSERVER}/ptuser.c
	set -x; \
	case ${SYS_NAME} in \
	hp_ux11*) \
		set X `echo ${CCRULE} | sed s/-g//`; shift; \
		"$$@" \
		;; \
	*) \
		${CCRULE} \
		;; \
	esac

display.o: ${PTSERVER}/display.c
	${CCRULE};

ptint.cs.o: ${PTSERVER}/ptint.cs.c
	${CCRULE};

ptint.xdr.o: ${PTSERVER}/ptint.xdr.c
	${CCRULE};

pterror.o: ${PTSERVER}/pterror.c
	${CCRULE};

rmtsysc.o: ${SYS}/rmtsysc.c
	${CCRULE};

rmtsys.xdr.o: ${SYS}/rmtsys.xdr.c
	${CCRULE};

rmtsys.cs.o: ${SYS}/rmtsys.cs.c
	${CCRULE};

afssyscalls.o: ${SYS}/afssyscalls.c
	${CCRULE};

rmtsysnet.o: ${SYS}/rmtsysnet.c
	${CCRULE};

clean:
	rm -f *.o libafsauthent*
