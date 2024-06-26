
#
# (C) COPYRIGHT IBM CORPORATION 1987
# LICENSED MATERIALS - PROPERTY OF IBM
#
SHELL=/bin/sh
INSTALL = ${SRCDIR}bin/install
KERNELDIR = ../libafs/
UKERNELDIR = ../libuafs/

MKDIR_IF_NEEDED=[ -d $$1 ] || mkdir -p $$1

COMPONENT=afs
include ../config/Makefile.${SYS_NAME}
include ../config/Makefile.version

system: install

# NOTE: linux case uses --new as well to work around bug in some versions of
# gencat.
kinstall: AFS_component_version_number.c
	set ${KERNELDIR}afs; $(MKDIR_IF_NEEDED)
	-chmod 666 ${KERNELDIR}afs/*
	${DESTDIR}bin/compile_et -v 2 afs_trace.et
	${INSTALL} *.[csh] ${KERNELDIR}afs
	${INSTALL} VNOPS/*.[csh] ${KERNELDIR}afs
	${INSTALL} ${AFS_OSTYPE}/*.[csh] ${KERNELDIR}afs
	set ${DESTDIR}root.client/usr/vice/etc/C; $(MKDIR_IF_NEEDED)
	-rm -f afszcm.cat
	case ${SYS_NAME} in \
		sgi_* ) \
			gencat -m afszcm.cat afs_trace.msf ;; \
		i386_linux* ) \
			gencat --new afszcm.cat afs_trace.msf ;; \
		* ) \
			gencat afszcm.cat afs_trace.msf ;; \
	esac
	
	

ukinstall webinstall: AFS_component_version_number.c
	set ${UKERNELDIR}afs; $(MKDIR_IF_NEEDED)
	-chmod 666 ${UKERNELDIR}afs/*
	${DESTDIR}bin/compile_et -v 2 afs_trace.et
	${INSTALL} *.[csh] ${UKERNELDIR}afs
	${INSTALL} VNOPS/*.[csh] ${UKERNELDIR}afs
	${INSTALL} UKERNEL/*.[csh] ${UKERNELDIR}afs

noversion: install.noversion
install install.noversion: kinstall ukinstall
	${INSTALL} afs.h ${DESTDIR}include/afs
	${INSTALL} ${AFS_OSTYPE}/osi_inode.h ${DESTDIR}include/afs
	${INSTALL} afs_stats.h exporter.h nfsclient.h  ${DESTDIR}include/afs
	case ${SYS_NAME} in \
		pmax_ul43 | pmax_ul43a) \
			${INSTALL} longc_procs.h ${DESTDIR}include/afs ;; \
	esac
	case ${SYS_NAME} in \
		next_mach30 | vax_ul43) \
			echo skipping afszcm.cat install for ${SYS_NAME} ;; \
		* ) \
			${INSTALL} afszcm.cat ${DESTDIR}root.client/usr/vice/etc/C ;; \
	esac
	case ${SYS_NAME} in \
		*linux* ) \
			${INSTALL} ${AFS_OSTYPE}/osi_vfs.h ${DESTDIR}include/afs ;;\
		* ) \
			echo No vfs headers to install for ${SYS_NAME};; \
	esac

clean:
	echo Nothing to delete.
