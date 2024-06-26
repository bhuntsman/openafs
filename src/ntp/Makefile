# 
# 
#
SHELL = /bin/sh
COMPONENT=ntp
include ../config/Makefile.${SYS_NAME}

INSTALL=${SRCDIR}bin/install
INCPATH= -I${DESTDIR}include
BINDIR=/usr/local/etc
LINKDIR=/etc
LIBS= ${DESTDIR}lib/afs/util.a  

VERS=3.4

#CC=gcc -g -W -Wall
#afs#CC=cc -g -W

#afs#INSTALL= install -c

#
# FEATURES include:
#	DEBUG	  - include DEBUG code
#	BROADCAST_NTP - experimental support for broadcast NTP
#	XADJTIME2 - experimental support for second-order clock adjustment
#		    system call.
#	SETTICKADJ - attempt to modify kernel's `tickadj' variable at run time.
#	REFCLOCK  - define if you have a reference clock attached to your
#		    machine.  (untested by UMD)
#	PSTI - define along with REFCLOCK if you have a PSTI clock attached
#		that you'd like to use a a reference clock.
#	XTAL=0 - for line freq clock, or
#	XTAL=1 	 for crystal controlled clock (default)
#	LOG_NTP=foo - to change the syslog facility.  You could specify
#		    something like -DLOG_NTP=LOG_LOCAL3 to log into the
#		    LOG_LOCAL3 syslog facility
#	NOSWAP - allow use of plock() to prevent swapping
#

#FEATURES= -DBROADCAST_NTP -DSETTICKADJ -DDEBUG
#FEATURES= -DSETTICKADJ -DDEBUG -DREFCLOCK -DPSTI
#afs#FEATURES= -DSETTICKADJ -DDEBUG -DREFCLOCK
#afs# # SETTICKADJ is set in ntpd.c on SUNs.
FEATURES= -DDEBUG -DREFCLOCK

# for afs
DEFINES= -DGENERIC_UNS_BUG -DSUN_FLT_BUG

# for 4.3 BSD
#afs#DEFINES=

# for Sun
#DEFINES= -DSUN_FLT_BUG

# for Ultrix 2.0/2.2
# don't forget to fix the broken definition of inet_addr in netdb.h
# it should be declared as a u_long not a in_addr  (the doc is wrong also)
# VAX_COMPILER_FLT_BUG is defined for pcc which doesn't know how to 
# convert an unsigned long into a float/double
#DEFINES= -DVAX_COMPILER_FLT_BUG -DNOSWAP

#
# for a NeXT system, define these pre-processor symbols.
#DEFINES=-DSUN_FLT_BUG -DGENERIC_UNS_BUG 

CFLAGS= -O ${DEFINES} ${FEATURES} ${INCPATH} ${XCFLAGS}
LDFLAGS= ${XLDFLAGS}
#
# Header files
#
HDRS=	ntp.h patchlevel.h

# Source files
#
NTPDSRC= ntpd.c ntpsubs.c ntp_proto.c ntp_sock.c ntp_adjust.c read_local.c \
	read_psti.c
SRCS=	ntp.c ntpdc.c test.c ${NTPDSRC}

# Object files
#
NTPDOBJ= ntpd.o ntpsubs.o ntp_proto.o ntp_sock.o ntp_adjust.o read_local.o \
	read_psti.o
OBJS=	ntp.o ntpdc.o test.o ${NTPDOBJ}


DIST= README Makefile man ${SRCS} ${HDRS} ntp.conf test.c extract.pl stat.pl
PROGS=	ntp ntpd ntpdc ntest

include ../config/Makefile.version

ntp.o: AFS_component_version_number.c
ntpd.o: AFS_component_version_number.c
ntpdc.o: AFS_component_version_number.c
read_psti.o: AFS_component_version_number.c

all:	${PROGS} runntp
	-@./ntest

ntp:	ntp.o ntpsubs.o
	${CC} ${LDFLAGS} -o ntp ntp.o ntpsubs.o ${LIBS} ${XLIBS} ${XLIBELFA}

ntpd:	${NTPDOBJ}
	case ${SYS_NAME} in \
		sgi_*) \
			${CC} ${LDFLAGS} -o ntpd ${NTPDOBJ} ${LIBS} -lelf ;; \
		*) \
			${CC} ${LDFLAGS} -o ntpd ${NTPDOBJ} ${LIBS} ${XLIBS} ${XLIBELFA} ;; \
	esac


ntpdc: ntpdc.o
	${CC} ${LDFLAGS} -o ntpdc ntpdc.o ${LIBS} ${XLIBS} ${XLIBELFA}

ntest: test.o ntpsubs.o
	${CC} ${LDFLAGS} -o ntest test.o ntpsubs.o ${LIBS} ${XLIBS} ${XLIBELFA}
	./ntest

sock_test: ntp_sock.c AFS_component_version_number.o
	${CC} ${LDFLAGS} -DTEST -o sock_test ntp_sock.c ${LIBS} ${XLIBS} ${XLIBELFA}

${OBJS}: ntp.h # Makefile
ntpd.o:	patchlevel.h

# for afs

system: install

AFSLIBS= ${DESTDIR}lib/afs/libauth.a ${DESTDIR}lib/afs/libcom_err.a ${DESTDIR}lib/afs/util.a 

runntp.o: runntp.c ${DESTDIR}/include/afs/cellconfig.h AFS_component_version_number.c
	${CC} ${CFLAGS} -I${DESTDIR}include -c runntp.c ${AFSLIBS} ${XLIBS}

runntp: runntp.o ${AFSLIBS}
	${CC} ${LDFLAGS} -o runntp runntp.o ${AFSLIBS} ${XLIBS}

install: AFS_component_version_number.c ntest ntpd ntp ntpdc runntp
	${INSTALL} ntpd ${DESTDIR}root.server/usr/afs/bin/ntpd
	${INSTALL} ntpdc ${DESTDIR}root.server/usr/afs/bin/ntpdc
	${INSTALL} ntp ${DESTDIR}etc/ntp
	${INSTALL} ntpdc ${DESTDIR}etc/ntpdc
	${INSTALL} runntp ${DESTDIR}root.server/usr/afs/bin/runntp

#
# If you don't want a symlink to the daemon, comment out the next line
#	make ${MFLAGS} DESTDIR=${DESTDIR} install-link

install-man:
	cd man; make ${MFLAGS} DESTDIR=${DESTDIR} install

install-link:
	rm -f ${BINDIR}/${LINKDIR}/ntpd
	ln -s ${BINDIR}/ntpd ${DESTDIR}/${LINKDIR}/ntpd

print:
	enscript -2r -p - Makefile ${HDRS} ${SRCS} | qpr -q lps40

clean:
	@rm -f *.o *~ core ${PROGS} ntp.tar ntest sock_test AFS_component_version_number.c

lint:
	lint -bac ${DEFINES} ${NTPDSRC}

dist:	ntp.tar.Z
	mv ntp.tar.Z /usr/ftp/pub/ntp.${VERS}/ntp.tar.Z

test-dist:	ntp.tar.Z
	mv ntp.tar.Z /usr/ftp/pub/ntp.${VERS}/ntp-test.tar.Z

ntp.tar.Z:	${DIST}
	rm -f ntp.tar ntp.tar.Z
	tar cf ntp.tar ${DIST}
	compress  ntp.tar


depend:
	mkdep $(CFLAGS) $(SRCS)

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
