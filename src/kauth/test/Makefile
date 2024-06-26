# Copyright (C) 1992  Transarc Corporation - All rights reserved

SRCDIR=DEST/
DESTDIR=DEST/
BACKDIR=../../../../PARENT/dest/
OPTIMIZE=-O

INCDIRS= -I${DESTDIR}include -I.. -I${BACKDIR}include
LDIRS= -L${DESTDIR}lib -L${DESTDIR}lib/afs -L.. -L${BACKDIR}lib -L${BACKDIR}lib/afs
LIBS= -llwp -ldes -lrxkad -lauth -lcmd -lubik -lprot -lsys -lrx -lcom_err -lkauth

utilib=${SRCDIR}lib/afs/util.a

CFLAGS = ${OPTIMIZE} ${INCDIRS} ${LDIRS} ${LIBS} ${XCFLAGS}

system: all

# the only tests _known_ to work are multiklog and the tcl scripts.

all test tests: multiklog 

clean:
	rm -f *.o core

multiklog: multiklog.o
	cc -o  multiklog ${CFLAGS} multiklog.o ${utilib}

test_date: ../kautils.o test_date.o ${utilib}
	cc -o test_date ${CFLAGS} test_date.o ../kautils.o ${utilib}

test_badtix: test_badtix.o
	cc -o test_badtix ${CFLAGS} test_badtix.o

decode_ticket: decode_ticket.o
	cc -o decode_ticket ${CFLAGS} decode_ticket.o 

test_interim_ktc: test_interim_ktc.o
	cc -o test_interim_ktc ${CFLAGS} test_interim_ktc.o

test_rxkad_free: test_rxkad_free.o 
	cc -o test_rxkad_free  ${CFLAGS} test_rxkad_free.o 

test_getticket: test_getticket.o 
	cc -o test_getticket  ${CFLAGS} test_getticket.o 

background: background.c
	cc -o background background.c

# test_interim_ktc depends on normally running ka & pr servers.  Also on
# the current user having write access to the working directory.  Also
# on the user "tester" being defined with ViceId=1031.  The password
# must be specified on the make line the others can be overridden by
# specifying a value for TESTERVALUES.  To test remote authentication, a
# test account in another cell must be available.  The default is the
# cell "cellname" and the user tester with Vice Id 510, and the
# same password as in the local cell.  These can be over-ridden by
# specifying a difference value for REMOTETESTER.  An empty value for
# REMOTETESTER will disable testing of remote authentication.

TESTERNAME=xxx
TESTERPASSWORD=xxx
#TESTERVALUES=-tester tester -testId 1031
TESTERVALUES=
REMOTEPASSWORD=${TESTERPASSWORD}
REMOTETESTER=-remoteTester $(TESTERNAME) -remoteTestPassword ${REMOTEPASSWORD} -remoteTestId 510 -remoteCell cellname

# test_kaserver starts a server in the background and tests it.

# test_rxkad_free checks for rxkad object/connection usage and checks for
#   core leaks.  Normally it makes 100 calls to the kaserver, doing a GetEntry
#   call.  This requests an admin ticket 100 times and creates a ubik
#   connection 100 times.  With the -doauth switch specified it calls
#   ka_UserAuthenticate instead, which uses rxnull connections, but also
#   exercises the ptserver code, particularly, pr_Initialize, 100 times.

runtest: background multiklog test_badtix test_interim_ktc test_rxkad_free
	test_interim_ktc ${TESTERVALUES} -testPassword ${TESTERPASSWORD} ${REMOTETESTER}
	test_kaserver
	test_rxkad_free -waitforreap -a $(TESTERNAME) -p $(TESTERPASSWORD)
	test_rxkad_free -doauth -waitforreap -a $(TESTERNAME) -p $(TESTERPASSWORD)
