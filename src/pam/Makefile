SHELL=/bin/sh
INSTALL = ${SRCDIR}bin/install
KERNELDIR = ../libafs/

COMPONENT=afs
include ../config/Makefile.${SYS_NAME}

LIBDIR = ${DESTDIR}lib/
INSTALL=${SRCDIR}bin/install
  LIBSA = ${LIBDIR}afs/libprot.a ${LIBDIR}libubik.a
AFSLIBS = ${DESTDIR}lib/librxkad.a 	    \
	  ${LIBDIR}afs/libsys.a ${DESTDIR}lib/libdes.a 	    \
	  ${LIBDIR}librx.a ${LIBDIR}liblwp.a ${LIBDIR}afs/libaudit.a \
          ${LIBDIR}afs/libcmd.a ${LIBDIR}afs/libcom_err.a ${LIBDIR}afs/util.a

LDFLAGS = ${SHARE_LDFLAGS}
   LIBS = ${LIBDIR}afs/libkauth.a ${LIBSA} ${LIBDIR}afs/libauth.a \
	  ${AFSLIBS} ${PAMLIBS}
  KLIBS = ${LIBDIR}afs/libkauth.krb.a ${LIBSA} ${LIBDIR}afs/libauth.krb.a \
	  ${AFSLIBS} ${PAMLIBS}
 SHOBJS = afs_auth.o afs_account.o afs_session.o afs_password.o \
	  afs_pam_msg.o afs_message.o afs_util.o AFS_component_version_number.o
   OBJS = $(SHOBJS) test_pam.o
INCLUDES = -I${DESTDIR}include -I/usr/include -I/usr/include/sys
CFLAGS =  ${DEBUG}  ${INCLUDES} ${PAM_CFLAGS}

TARGETS = pam_afs.so.1 test_pam pam_afs.krb.so.1

include ../config/Makefile.version

system all: $(TARGETS)

afs_setcred.o: afs_setcred.c afs_pam_msg.h afs_message.h afs_util.h
	${CC} ${CFLAGS} -c afs_setcred.c -o afs_setcred.o

afs_setcred_krb.o: afs_setcred.c afs_pam_msg.h afs_message.h afs_util.h
	${CC} ${CFLAGS} -DAFS_KERBEROS_ENV -c afs_setcred.c -o afs_setcred_krb.o

pam_afs.so.1: $(SHOBJS) afs_setcred.o
	set -x; \
	case "$(SYS_NAME)" in \
	hp_ux*) \
		$(LD) $(LDFLAGS) -c mapfile.hp -o $@ afs_setcred.o \
			$(SHOBJS) $(LIBS) ;; \
	sun*_5*) \
		$(LD) $(LDFLAGS) -M mapfile -o $@ afs_setcred.o \
			$(SHOBJS) $(LIBS) ;; \
	*linux*) \
		$(CC) $(LDFLAGS) -o $@ afs_setcred.o $(SHOBJS) $(LIBS) ;;\
	* ) \
		echo No link line for system $(SYS_NAME). ;; \
	esac

pam_afs.krb.so.1: $(SHOBJS) afs_setcred_krb.o
	set -x; \
	case "$(SYS_NAME)" in \
	hp_ux*) \
		$(LD) $(LDFLAGS) -c mapfile.hp -o $@ \
			afs_setcred_krb.o $(SHOBJS) $(LDFLAGS) $(KLIBS) ;; \
	sun*_5*) \
		$(LD) $(LDFLAGS) -M mapfile -o $@ \
			afs_setcred_krb.o $(SHOBJS) $(LDFLAGS) $(KLIBS) ;; \
	*linux*) \
		$(CC) $(LDFLAGS) -o $@ afs_setcred_krb.o $(SHOBJS) $(KLIBS) ;;\
	* ) \
		echo No link line for system $(SYS_NAME). ;; \
	esac

test_pam: test_pam.o
	set -x; \
	case "$(SYS_NAME)" in \
	hp_ux*) \
		$(CC) $(CFLAGS) -o $@ test_pam.o ${PAMLIBS};; \
	sun*_5*) \
		$(CC) $(CFLAGS) -o $@ test_pam.o ${PAMLIBS};; \
	*linux*) \
		$(CC) $(CFLAGS) -rdynamic -o $@ test_pam.o -lpam -ldl;; \
	*) \
		echo No link line for system $(SYS_NAME). ;; \
	esac

clean:
	/bin/rm -f $(TARGETS) $(OBJS) afs_setcred.o afs_setcred_krb.o core *~

install:	all
	${INSTALL} -s pam_afs.so.1 ${DESTDIR}lib/pam_afs.so.1
	${INSTALL} -s pam_afs.krb.so.1 ${DESTDIR}lib/pam_afs.krb.so.1

afs_auth.o: afs_auth.c afs_pam_msg.h afs_message.h afs_util.h
afs_pam_msg.o: afs_pam_msg.c afs_pam_msg.h afs_message.h
afs_message.o: afs_message.c afs_message.h
afs_util.o: afs_util.c afs_util.h
