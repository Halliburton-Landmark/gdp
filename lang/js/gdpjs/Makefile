#
#  Makefile for libgdpjs  (based on gdp/libep/Makefile)
#
#	$Id$
#
#	Copyright (c) 2014, <TBD>.  All rights reserved.
#

# Alec Dara-Abrams
# 2014-10-24
#
# TBD: Copyrights, one-button tests, README files
#
# Caveats:
#
#    This Makefile assumes that it is located in gdp/lang/js/gdpjs/ ,
#    where gdp/ is a GDP Git local repository.  See GDPROOT and
#    the (unstructured) -I..'s below.
#
#    It will not force a re-build of the system up in gdp/ .
#    gdp/ must be re-built and kept up to date separately.
#
#    Products of the build:
#      1) *.o and *.a are left in the current directory
#      2) the dynamic library, libgdpjs.so.<<major>>.<<minor>> is moved
#      to gdpjs/libs/ and variously named copies are sym linked to it there.
#    Note, some of the various dynamic library sym linked files have
#    version information within their file names.
#
#    Yes, this Makefile is not as clean, structured, and parameterized
#    as it could be!


# External makefile targets below: all, clean
# TBD: provide a test/ sub-directory.


# Internal makefile variables

CURDIR=	.

OBJS=	\
	gdpjs_supt.o \

HFILES=	\
	gdpjs_supt.h \

CC=	cc
PG=
O=	-O
WALL=	-Wall
STD=
COPTS=	${PG} -g ${WALL} $O ${STD}
# /opt/local/include is for Mac OS X.
CFLAGS=	${COPTS} ${INCLS} -I${CURDIR} -I.. -I../.. -I../../.. -I/opt/local/include -fPIC
RM=	rm
LD=	ld
GDPROOT= ../../..
LIBROOT= ${GDPROOT}
# /opt/local/lib is for Mac OS X.
LDCRYPTO= -L/opt/local/lib -lcrypto -lavahi-common -lavahi-client
LDEVENT2= -levent -levent_pthreads
LDEP=   -L${LIBROOT}/ep -lep
LDGDP=  -L${LIBROOT}/gdp -lgdp
#LDFLAGS=-L.
LDFLAGS= ${LDGDP} ${LDEP} ${LDEVENT2} ${LDCRYPTO}
LDLIBS=	libgdpjs.a
LIBDIR= ../libs
LN=	ln
MV=	mv
RANLIB=	ranlib
SHARED=	-shared
#SHARED=	-shared -Wl,-soname,$@
TIME=

GDPJS_LIBMAJVER=	1
GDPJS_LIBMINVER=	0
GDPJS_LIBVER=	${GDPJS_LIBMAJVER}.${GDPJS_LIBMINVER}
ALL=		libgdpjs.a libgdpjs.so.${GDPJS_LIBVER}

all: ${ALL}

libgdpjs.a: ${OBJS}
	${AR} -r $@ ${OBJS}
	${RANLIB} $@

libgdpjs.so.${GDPJS_LIBVER}: ${OBJS}
	${CC} ${SHARED} -o $@ ${OBJS} ${LDFLAGS}
	${MV} $@ ${LIBDIR}
	(cd ${LIBDIR}; rm -f libgdpjs.so.${GDPJS_LIBMAJVER}; ln -s libgdpjs.so.${GDPJS_LIBVER} libgdpjs.so.${GDPJS_LIBMAJVER})
	(cd ${LIBDIR}; rm -f libgdpjs.so; ln -s libgdpjs.so.${GDPJS_LIBMAJVER} libgdpjs.so)
	(cd ${LIBDIR}; rm -f libgdpjs.${GDPJS_LIBVER}.dylib; ln -s libgdpjs.so.${GDPJS_LIBVER} libgdpjs.${GDPJS_LIBVER}.dylib)

gdpjs_version.o: gdpjs_version.c FORCE
	${CC} ${CFLAGS} -D_CURRENT_DATE_=\"`date +'%Y-%m-%d_%H:%M'`\" -c gdpjs_version.c

FORCE:

#
#  Administrative stuff
#

clean:
	-${RM} -f ${ALL} *.o *.core

${OBJS}: ${HFILES}
