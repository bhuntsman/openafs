#
# Makefile for the wsadmin src directory, the place where package files
# are generated.
#
#	(C) Copyright Transarc Corporation 1989
#	Licensed Materials - Property of Transarc
#	All Rights Reserved.
#
CONFIG = \
	staff.aix_22 \
	staff.pmax_31 \
	staff.rt_r4 \
	staff.dkload.rt_r4 \
	staff.sun3_35 \
	staff.sun3_40 \
	staff.sun3_40x \
	staff.sun4_40 \
	staff.sun4_40c \
	staff.vax_30

BASE_LIBS = \
	${wsadmin}/src/admin \
	${wsadmin}/lib/devel \
	${wsadmin}/lib/base.generic

MACHINE_LIBS = \
	${wsadmin}/lib/aix_22.generic \
	${wsadmin}/lib/aix_22.generic.dev \
	${wsadmin}/lib/aix_22.readonly \
	${wsadmin}/lib/aix_22.readwrite \
 \
	${wsadmin}/lib/rt_r4.generic \
	${wsadmin}/lib/rt_r4.generic.dev \
	${wsadmin}/lib/rt_r4.generic.dkload \
	${wsadmin}/lib/rt_r4.generic.printer \
	${wsadmin}/lib/rt_r4.readonly \
	${wsadmin}/lib/rt_r4.readwrite \
 \
	${wsadmin}/lib/pmax_31.generic \
	${wsadmin}/lib/pmax_31.generic.dev \
	${wsadmin}/lib/pmax_31.readonly \
	${wsadmin}/lib/pmax_31.readwrite \
 \
	${wsadmin}/lib/sun3_35.generic \
	${wsadmin}/lib/sun3_35.generic.dev \
	${wsadmin}/lib/sun3_35.readonly \
	${wsadmin}/lib/sun3_35.readwrite \
 \
	${wsadmin}/lib/sun3_40.generic \
	${wsadmin}/lib/sun3_40.generic.dev \
	${wsadmin}/lib/sun3_40.readonly \
	${wsadmin}/lib/sun3_40.readwrite \
 \
	${wsadmin}/lib/sun3_40x.generic \
	${wsadmin}/lib/sun3_40x.generic.dev \
	${wsadmin}/lib/sun3_40x.readonly \
	${wsadmin}/lib/sun3_40x.readwrite \
 \
	${wsadmin}/lib/sun4_40.generic \
	${wsadmin}/lib/sun4_40.generic.dev \
	${wsadmin}/lib/sun4_40.readonly \
	${wsadmin}/lib/sun4_40.readwrite \
 \
	${wsadmin}/lib/sun4_40c.generic \
	${wsadmin}/lib/sun4_40c.generic.dev \
	${wsadmin}/lib/sun4_40c.readonly \
	${wsadmin}/lib/sun4_40c.readwrite \
 \
	${wsadmin}/lib/vax_30.generic \
	${wsadmin}/lib/vax_30.generic.dev \
	${wsadmin}/lib/vax_30.readonly \
	${wsadmin}/lib/vax_30.readwrite

LIBS = ${MACHINE_LIBS} ${BASE_LIBS}

.SUFFIXES: .aix_22 \
		.pmax_31 \
		.rt_r4 \
		.proto \
		.sun3_35 \
		.sun3_40 \
		.sun3_40x \
		.sun4_40 \
		.sun4_40c \
		.vax_30

.proto.aix_22:	; mpp -Dwsadmin=${wsadmin} -Dsys=aix_22  -Dname=$* $*.proto > $@
.proto.pmax_31:	; mpp -Dwsadmin=${wsadmin} -Dsys=pmax_31 -Dname=$* $*.proto > $@
.proto.rt_r4:	; mpp -Dwsadmin=${wsadmin} -Dsys=rt_r4   -Dname=$* $*.proto > $@
.proto.sun3_35: ; mpp -Dwsadmin=${wsadmin} -Dsys=sun3_35 -Dname=$* $*.proto > $@
.proto.sun3_40: ; mpp -Dwsadmin=${wsadmin} -Dsys=sun3_40 -Dname=$* $*.proto > $@
.proto.sun3_40x: ; mpp -Dwsadmin=${wsadmin} -Dsys=sun3_40x -Dname=$* $*.proto > $@
.proto.sun4_40: ; mpp -Dwsadmin=${wsadmin} -Dsys=sun4_40 -Dname=$* $*.proto > $@
.proto.sun4_40c: ; mpp -Dwsadmin=${wsadmin} -Dsys=sun4_40c -Dname=$* $*.proto > $@
.proto.vax_30:	; mpp -Dwsadmin=${wsadmin} -Dsys=vax_30  -Dname=$* $*.proto > $@

all: ${CONFIG}

${CONFIG}: ${LIBS}

system: install

install: ${CONFIG}
	cp ${CONFIG} ${wsadmin}/etc
	
clean:
	rm -f ${CONFIG} *.BAK *.CKP
