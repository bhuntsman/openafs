SRCDIR=/usr/andy/
LIBRX=${SRCDIR}lib/librx.a
DESTDIR=/usr/andy/
INSTALL=${SRCDIR}bin/install
LIBS=${LIBRX} ${SRCDIR}lib/liblwp.a
CFLAGS=-g -I.. -I. -I${SRCDIR}include -DDEBUG ${XCFLAGS}

all: bulk_client bulk_server

bulk_client: bulk_client.o bulk_io.o ${LIBS} bulk.cs.o
	${CC} ${CFLAGS} -o bulk_client bulk_client.o bulk_io.o bulk.cs.o ${LIBS}

bulk_server: bulk_server.o bulk_io.o bulk.ss.o ${LIBS}
	${CC} ${CFLAGS} -o bulk_server bulk_server.o bulk_io.o bulk.ss.o ${LIBS}

bulk_client.o bulk_server.o bulk_io.o:	bulk.h

bulk.cs.c bulk.ss.c bulk.er.c bulk.h: bulk.xg
	rxgen bulk.xg
