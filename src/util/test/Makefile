
include ../../config/Makefile.${SYS_NAME}

CFLAGS = -I. -I.. -I${SRCDIR}include ${XCFLAGS} -g

tests: test_ktime ktest dirpath_test

ktests.o: ktests.c ../ktime.h
	${CC} ${CFLAGS} -c ktests.c

ktest: ktest.o ../util.a
	${CC} ${CFLAGS} -o ktest ktest.o ../util.a ${XLIBS}

test_ktime.o: test_ktime.c ../ktime.h
	${CC} ${CFLAGS} -c test_ktime.c

test_ktime: test_ktime.o ../util.a
	${CC} ${CFLAGS} -o test_ktime test_ktime.o ../util.a ${XLIBS}

dirpath_test: dirpath_test.o ../util.a 
	${CC} dirpath_test.o  ${CFLAGS} -o dirpath_test ../util.a ${XLIBS}

b64: b64.o ../util.a
	${CC} b64.o ${CFLAGS} -o b64 ../util.a ${XLIBS}

fb64: fb64.o ../util.a
	${CC} fb64.o ${CFLAGS} -o fb64 ../util.a ${XLIBS}
