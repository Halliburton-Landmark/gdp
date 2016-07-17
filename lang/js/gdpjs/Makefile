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
LOCAL1=	/usr/local
LOCAL2=	/opt/local
LIBROOT= ${GDPROOT}
# /opt/local/lib is for Mac OS X.
LDCRYPTO= -L/opt/local/lib -lcrypto -lavahi-common -lavahi-client
LDEVENT2= -levent -levent_pthreads
LDEP=   -L${LIBROOT}/ep -lep
LDGDP=  -L${LIBROOT}/gdp -lgdp
#LDFLAGS=-L.
LDFLAGS= ${LDGDP} ${LDEP} ${LDEVENT2} ${LDCRYPTO} -L${LOCAL1}/lib -L${LOCAL2}/lib
LDLIBS=	libgdpjs.a
LIBDIR= ../libs
LN=	ln
MV=	mv
RANLIB=	ranlib
SHARED=	-shared
#SHARED=	-shared -Wl,-soname,$@
TIME=

GDPJSLIBMAJVER=	1
GDPJSLIBMINVER=	0
GDPJSLIBVER=	${GDPJSLIBMAJVER}.${GDPJSLIBMINVER}

# Version of the GDP Library, should match ../../../gdp/Makefile
GDPLIBMAJVER=	0
GDPLIBMINVER=	5

# Version of the EP Library, should match ../../../ep/Makefile
EPLIBMAJVER=	3
EPLIBMINVER=	0

ALL=		libgdpjs.a libgdpjs.so.${GDPJSLIBVER}


all: ${ALL}

libgdpjs.a: ${OBJS}
	${AR} -r $@ ${OBJS}
	${RANLIB} $@

libgdpjs.so.${GDPJSLIBVER}: ${OBJS} ../gdp ../ep
	${CC} ${SHARED} -o $@ ${OBJS} ${LDFLAGS}
	(cd ${LIBDIR} && sh ${GDPROOT}/libs/makelinks.sh gdpjs ${GDPJSLIBMAJVER} ${GDPJSLIBMINVER})
	(cd ${LIBDIR} && sh ${GDPROOT}/libs/makelinks.sh gdp ${GDPLIBMAJVER} ${GDPLIBMINVER})
	(cd ${LIBDIR} && sh ${GDPROOT}/libs/makelinks.sh ep ${EPLIBMAJVER} ${EPLIBMINVER})

gdpjs_version.o: gdpjs_version.c FORCE
	${CC} ${CFLAGS} -D_CURRENT_DATE_=\"`date +'%Y-%m-%d_%H:%M'`\" -c gdpjs_version.c

../gdp:
	if [ ! -d ../gdp ]; then \
		echo "Create a copy of libgdp* so that we can bundle it up"; \
		mkdir ../gdp; \
		cp ../../../gdp/libgdp* ../gdp; \
	fi

../ep:
	if [ ! -d ../ep ]; then \
		echo "Create a copy of libep* so that we can bundle it up"; \
		mkdir ../ep; \
		cp ../../../ep/libep* ../ep; \
	fi
FORCE:

#
#  Administrative stuff
#

clean:
	-${RM} -f ${ALL} *.o *.core

${OBJS}: ${HFILES}
