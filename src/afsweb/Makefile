
SHELL=/bin/sh
COMPONENT=nsafs

include ../config/Makefile.${SYS_NAME}

UKERNELDIR=../libuafs/
INSTALL=${SRCDIR}bin/install
COMPILE_ET=${SRCDIR}bin/compile_et

MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

UKSRCS=nsafs.h nsafs.c securehash.c

#
# Targets for Apache AFS Web Secure
#
APACHE_AFS_LIB=libapacheafs.a
DCE_ADK_LIB=libadk.a
APACHE_AFS_COMMON_LIB=libcommon.a

BINARIES=weblog weblog_starter
SRC_MODULE=afs_module.c
## This module is made compatible with Apache 1.3.1 by including a file called
## ap_compat.h provided by Apache (basically all they do is #define all the old API calls
## as the new ones with ap_ prepended to the old API calls. The makefile takes care of this
## while building afs_module.c and adding the AFS_component_version_number.c file's contents 
## in comments. 

APACHEAFS_DOC=README
APACHE_INSTALL_SCRIPT=afswsApache_config.sh 
NETSCAPE_INSTALL_SCRIPT=afswsNetscape_config.sh



system noversion apache_afs_web_secure:
	case "${SYS_NAME}" in \
		rs_aix* ) \
			echo Clean, make, and install the lib for 1.2 ; \
			$(MAKE) libclean ; \
			$(MAKE) AUX_CFLAGS=-DAIX $(APACHE_AFS_LIB) ; \
			${INSTALL} ${APACHE_AFS_LIB} ${DESTDIR}root.afsweb/lib/apache_1.2 ; \
			echo Make and install source for 1.2 ; \
			/bin/rm -f $(SRC_MODULE) ; \
			$(MAKE) $(SRC_MODULE) VERSION_CFLAG=-DAPACHE_1_2 ; \
			${INSTALL} $(SRC_MODULE) ${DESTDIR}root.afsweb/src/apache_1.2 ; \
			echo Make and install the lib for 1.3.1 ; \
			$(MAKE) libclean ; \
			$(MAKE) AUX_CFLAGS=-DAIX $(APACHE_AFS_LIB) VERSION_CFLAG="-DAPACHE_1_3 -DAPACHE_1_3_1" ; \
			${INSTALL} $(APACHE_AFS_LIB) ${DESTDIR}root.afsweb/lib/apache_1.3.1 ; \
			echo Make and install source for 1.3.1 ; \
			/bin/rm -f $(SRC_MODULE) ; \
			$(MAKE) $(SRC_MODULE) VERSION_CFLAG="-DAPACHE_1_3 -DAPACHE_1_3_1" ; \
			${INSTALL} $(SRC_MODULE) ${DESTDIR}root.afsweb/src/apache_1.3.1 ; \
			echo Clean, make, and install the lib for 1.3.6 ; \
			$(MAKE) libclean ; \
			$(MAKE) AUX_CFLAGS=-DAIX $(APACHE_AFS_LIB) VERSION_CFLAG="-DAPACHE_1_3 -DAPACHE_1_3_6"; \
			${INSTALL} $(APACHE_AFS_LIB) ${DESTDIR}root.afsweb/lib/apache_1.3.6 ; \
			/bin/rm -f $(SRC_MODULE) ; \
			echo Make and install source for 1.3.1 ; \
			$(MAKE) $(SRC_MODULE) VERSION_CFLAG="-DAPACHE_1_3 -DAPACHE_1_3_6" ; \
			${INSTALL} $(SRC_MODULE) ${DESTDIR}root.afsweb/src/apache_1.3.6 ; \
			echo Do the binaries ; \
			$(MAKE) AUX_CFLAGS=-DAIX $(BINARIES) ;; \
		sun*) \
			$(MAKE) libclean ; \
			$(MAKE) AUX_FLAGS=-DSOLARIS $(APACHE_AFS_LIB) ; \
			${INSTALL} $(APACHE_AFS_LIB) ${DESTDIR}root.afsweb/lib/apache_1.2 ; \
			$(MAKE) libclean ; \
			$(MAKE) AUX_FLAGS=-DSOLARIS $(APACHE_AFS_LIB) VERSION_CFLAG="-DAPACHE_1_3 -DAPACHE_1_3_1" ; \
			${INSTALL} $(APACHE_AFS_LIB) ${DESTDIR}root.afsweb/lib/apache_1.3.1 ; \
			$(MAKE) libclean ; \
			$(MAKE) AUX_FLAGS=-DSOLARIS $(APACHE_AFS_LIB) VERSION_CFLAG="-DAPACHE_1_3 -DAPACHE_1_3_6" ; \
			${INSTALL} $(APACHE_AFS_LIB) ${DESTDIR}root.afsweb/lib/apache_1.3.6 ; \
			/bin/rm -f $(SRC_MODULE) ; \
			$(MAKE) $(SRC_MODULE) VERSION_CFLAG=-DAPACHE_1_2 ; \
			${INSTALL} $(SRC_MODULE) ${DESTDIR}root.afsweb/src/apache_1.2 ; \
			/bin/rm -f $(SRC_MODULE) ; \
			$(MAKE) $(SRC_MODULE) VERSION_CFLAG="-DAPACHE_1_3 -DAPACHE_1_3_1" ; \
			${INSTALL} $(SRC_MODULE) ${DESTDIR}root.afsweb/src/apache_1.3.1 ; \
			/bin/rm -f $(SRC_MODULE) ; \
			$(MAKE) $(SRC_MODULE) VERSION_CFLAG="-DAPACHE_1_3 -DAPACHE_1_3_6" ; \
			${INSTALL} $(SRC_MODULE) ${DESTDIR}root.afsweb/src/apache_1.3.6 ; \
			$(MAKE) AUX_CFLAGS=-DSOLARIS "XLIBS=-lsocket -lnsl" $(BINARIES) ;; \
		*linux*) \
			$(MAKE) libclean ; \
			$(MAKE) AUX_FLAGS=-DLINUX $(APACHE_AFS_LIB) ; \
			${INSTALL} $(APACHE_AFS_LIB) ${DESTDIR}root.afsweb/lib/apache_1.2 ; \
			$(MAKE) libclean ; \
			$(MAKE) AUX_FLAGS=-DLINUX $(APACHE_AFS_LIB) VERSION_CFLAG="-DAPACHE_1_3 -DAPACHE_1_3_1" ; \
			${INSTALL} $(APACHE_AFS_LIB) ${DESTDIR}root.afsweb/lib/apache_1.3.1 ; \
			$(MAKE) libclean ; \
			$(MAKE) AUX_FLAGS=-DLINUX $(APACHE_AFS_LIB) VERSION_CFLAG="-DAPACHE_1_3 -DAPACHE_1_3_6" ; \
			${INSTALL} $(APACHE_AFS_LIB) ${DESTDIR}root.afsweb/lib/apache_1.3.6 ; \
			/bin/rm -f $(SRC_MODULE) ; \
			$(MAKE) $(SRC_MODULE) VERSION_CFLAG=-DAPACHE_1_2 ; \
			${INSTALL} $(SRC_MODULE) ${DESTDIR}root.afsweb/src/apache_1.2 ; \
			/bin/rm -f $(SRC_MODULE) ; \
			$(MAKE) $(SRC_MODULE) VERSION_CFLAG="-DAPACHE_1_3 -DAPACHE_1_3_1" ; \
			${INSTALL} $(SRC_MODULE) ${DESTDIR}root.afsweb/src/apache_1.3.1 ; \
			/bin/rm -f $(SRC_MODULE) ; \
			$(MAKE) $(SRC_MODULE) VERSION_CFLAG="-DAPACHE_1_3 -DAPACHE_1_3_6" ; \
			${INSTALL} $(SRC_MODULE) ${DESTDIR}root.afsweb/src/apache_1.3.6 ; \
			$(MAKE) AUX_CFLAGS=-DLINUX "XLIBS=-lnsl" $(BINARIES) ;; \
		* ) \
			 echo "NOT BUILT FOR ${SYS_NAME}"; ;; \
	esac
	${INSTALL} ${APACHEAFS_DOC} ${DESTDIR}root.afsweb/doc
	${INSTALL} ${APACHE_INSTALL_SCRIPT} ${DESTDIR}root.afsweb/install
	chmod a+x ${DESTDIR}root.afsweb/install/${APACHE_INSTALL_SCRIPT}
	chmod a+w ${DESTDIR}root.afsweb/install/${APACHE_INSTALL_SCRIPT}
	${INSTALL} $(BINARIES) ${DESTDIR}root.afsweb/bin

include ../config/Makefile.version

all:

install: kinstall ukinstall

kinstall:
	@echo No $@ source here.


webinstall:${UKSRCS}
	set ${UKERNELDIR}afs; $(MKDIR_IF_NEEDED)
	${INSTALL} ${UKSRCS} ${UKERNELDIR}afs

ukinstall:
	@echo No $@ source here.

##
## For Apache Afs Web Secure libraries and binaries
## NOTE: APACHE_INCLUDES should point to the Apache src directory where 
## httpd.h and all it's included files reside which right now is src/afsweb/include
## and we have Apache versions as subdirectories but these are taken care of by the
## code. Be sure to compile with the VERSION_CFLAG=-DAPACHE_1_3 option for Apache 1.3
##
APACHE_INCLUDES=-Iapache_includes
EXTRA_CFLAGS=-DENABLE_DCE_DLOG -I../dauth/
ADKOBJS=../dauth/adkint.cs.o ../dauth/adkint.xdr.o

XLIBS=
DEFINES=-DSTATUS
OPTF=
DEBUG_FLAG=
AUX_CFLAGS=
VERSION_CFLAG=
AFS_INCL=-I${SRCDIR}include -I.. -I../config
CFLAGS=$(OPTF) $(DEFINES) -I. -I.. $(EXTRA_CFLAGS) $(VERSION_CFLAG) $(DEBUG_FLAG)
INCLS=${SRCDIR}include/ubik.h \
             ${SRCDIR}include/lwp.h \
             ${SRCDIR}include/lock.h  \
             ${SRCDIR}include/rx/rx.h \
             ${SRCDIR}include/rx/xdr.h \
             ${SRCDIR}include/rx/rxkad.h \
             ${SRCDIR}include/afs/com_err.h \
    	    weblog_errors.h $(DCE_DLOG_INCLUDES)
deslib=${SRCDIR}lib/libdes.a
rxkadlib=${SRCDIR}lib/librxkad.a
authlib=${SRCDIR}lib/afs/libauth.a
cmdlib=${SRCDIR}lib/afs/libcmd.a
utilib=${SRCDIR}lib/afs/util.a
auditlib=${SRCDIR}lib/afs/libaudit.a
kauthlib=${SRCDIR}lib/afs/libkauth.a

LIBS = ${kauthlib} ${SRCDIR}lib/libubik.a ${SRCDIR}lib/afs/libprot.a \
       ${authlib} ${rxkadlib} ${SRCDIR}lib/afs/libsys.a ${SRCDIR}lib/librx.a \
       ${SRCDIR}lib/afs/libsys.a  \
       ${SRCDIR}lib/liblwp.a ${deslib} ${cmdlib} \
       ${SRCDIR}lib/afs/libcom_err.a ${utilib} \
       $(DBM) $(XLIBS)

AR=ar
ARFLAGS=rv
LIBCOMMON_OBJS=apache_afs_utils.o apache_afs_cache.o
LIBAPACHE_OBJS=apache_afs_client.o apache_afs_plugin.o
LIBOBJS=$(LIBCOMMON_OBJS) $(LIBAPACHE_OBJS)
OBJS=$(LIBOBJS) apache_afs_weblog.o weblog.o

.c.o:
	$(CC) -c $(CFLAGS) $(AFS_INCL) $(AUX_CFLAGS) $<


weblog: weblog.o $(APACHE_AFS_COMMON_LIB) $(DCE_ADK_LIB)
	$(CC) -o $@ weblog.o $(APACHE_AFS_COMMON_LIB) $(LIBS) $(DCE_ADK_LIB) $(XLIBS)

weblog_starter: apache_afs_weblog.o weblog_errors.h 
	$(CC) -o $@ apache_afs_weblog.o 

$(DCE_ADK_LIB): 
	- /bin/rm -f $@ 
	$(AR) $(ARFLAGS) $@ $(ADKOBJS)  	

$(APACHE_AFS_COMMON_LIB):$(LIBCOMMON_OBJS)
	- /bin/rm -f $@
	$(AR) $(ARFLAGS) $@ $(LIBCOMMON_OBJS)

$(APACHE_AFS_LIB): $(LIBOBJS) AFS_component_version_number.o
	- /bin/rm -f $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS) 

afs_module.c: apache_afs_module.c 
	-/bin/rm -f afs_module.c \
	; echo "/*" > afs_module.c \
	; cat AFS_component_version_number.c >> afs_module.c \
	; echo "*/" >> afs_module.c \
	; case "${VERSION_CFLAG}" in \
		-DAPACHE_1_3*)	echo '#include "ap_compat.h"' >> afs_module.c ;; \
	  esac \
	; cat apache_afs_module.c >> afs_module.c
	 
##
##Dependencies
##

$(OBJS): Makefile

apache_afs_weblog.o: apache_afs_weblog.c weblog_errors.h AFS_component_version_number.o
apache_afs_utils.o: apache_afs_utils.c apache_afs_utils.h
apache_afs_cache.o: apache_afs_cache.c apache_afs_cache.h
weblog.o: weblog.c weblog_errors.h $(APACHE_AFS_COMMON_LIB)
apache_afs_client.o: apache_api.h apache_afs_client.c apache_afs_utils.o apache_afs_cache.o AFS_component_version_number.o
	$(CC) -c $(CFLAGS) $(AFS_INCL) $(APACHE_INCLUDES) $(AUX_CFLAGS) apache_afs_client.c
apache_afs_plugin.o: apache_api.h apache_afs_plugin.c apache_afs_client.o apache_afs_utils.o apache_afs_cache.o
	$(CC) -c $(CFLAGS) $(APACHE_INCLUDES) $(AUX_CFLAGS) apache_afs_plugin.c


clean:
	-/bin/rm -f *.o $(BINARIES) $(APACHE_AFS_LIB) $(APACHE_AFS_COMMON_LIB) $(DCE_ADK_LIB)

libclean:
	-/bin/rm -f $(APACHE_AFS_LIB) $(LIBOBJS)

