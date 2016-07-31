#
#  Makefile for libgdpjs  (based on gdp/libep/Makefile)

#	Copyright (c) 2008-2015, Eric P. Allman.  All rights reserved.
#	Copyright (c) 2014-2016, Regents of the University of California.
#	All rights reserved.
#
#	Permission is hereby granted, without written agreement and without
#	license or royalty fees, to use, copy, modify, and distribute this
#	software and its documentation for any purpose, provided that the above
#	copyright notice and the following two paragraphs appear in all copies
#	of this software.
#
#	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
#	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
#	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
#	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
#	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
#	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
#	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
#	OR MODIFICATIONS.#
#
# Author: Alec Dara-Abrams, Christopher Brooks
# 2014-10-24
#
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
		if [ -f ../../../libs/libgdp.${GDPLIBMAJVER}.${GDPLIBMINVER}.dylib ]; then \
			echo "Copying ../../../libs/libgdp.${GDPLIBMAJVER}.${GDPLIBMINVER}.dylib to ../gdp"; \
			cp ../../../libs/libgdp.${GDPLIBMAJVER}.${GDPLIBMINVER}.dylib ../gdp/libgdp.so.${GDPLIBMAJVER}.${GDPLIBMINVER}; \
		else \
			cp ../../../libs/libgdp* ../gdp/; \
		fi \
	fi


../ep:
	if [ ! -d ../ep ]; then \
		echo "Create a copy of libep* so that we can bundle it up"; \
		mkdir ../ep; \
		if [ -f ../../../libs/libep.${EPLIBMAJVER}.${EPLIBMINVER}.dylib ]; then \
			echo "Copying ../../../libs/libep.${EPLIBMAJVER}.${EPLIBMINVER}.dylib to ../ep"; \
			cp ../../../libs/libep.${EPLIBMAJVER}.${EPLIBMINVER}.dylib ../ep/libep.so.${EPLIBMAJVER}.${EPLIBMINVER}; \
		else \
			cp ../../../libs/libep* ../ep/; \
		fi \
	fi
FORCE:


# If we are running under Darwin, then copy the .so files from $PTII/lib.
# If we are running under Linux, then copy the .dylib files from $PTII/lib.
LIBEPDYLIB=libep.${EPLIBMAJVER}.${EPLIBMINVER}.dylib
../libs/${LIBEPDYLIB}:
	@echo "Creating $@"
	if [ ! -f ../ep/${LIBEPDYLIB} ]; then \
		cp ${PTII}/lib/${LIBEPDYLIB} $@; \
	fi

LIBGDPDYLIB=libgdp.${GDPLIBMAJVER}.${GDPLIBMINVER}.dylib
../libs/${LIBGDPDYLIB}:
	if [ ! -f ../ep/${LIBGDPDYLIB} ]; then \
		cp ${PTII}/lib/${LIBGDPDYLIB} $@; \
	fi

LIBGDPJSDYLIB=libgdpjs.${GDPJSLIBMAJVER}.${GDPJSLIBMINVER}.dylib
../libs/${LIBGDPJSDYLIB}:
	if [ ! -f ../ep/${LIBGDPSJDYLIB} ]; then \
		cp ${PTII}/lib/${LIBGDPJSDYLIB} $@; \
	fi


LIBEPSO=libep-${EPLIBMAJVER}.${EPLIBMINVER}.so
../libs/${LIBEPSO}:
	if [ ! -f ../ep/${LIBEPSO} ]; then \
		cp ${PTII}/lib/${LIBEPSO} $@; \
	fi

LIBGDPSO=libgdp-${GDPLIBMAJVER}.${GDPLIBMINVER}.so
../libs/${LIBGDPSO}:
	if [ ! -f ../ep/${LIBGDPSO} ]; then \
		cp ${PTII}/lib/${LIBGDPSO} $@; \
	fi

LIBGDPJSSO=libgdpjs.${GDPJSLIBMAJVER}.${GDPJSLIBMINVER}.so
../libs/${LIBGDPJSSO}:
	if [ ! -f ../ep/${LIBGDPSJSO} ]; then \
		cp ${PTII}/lib/${LIBGDPJSSO} $@; \
	fi

LIBS=../libs/${LIBEPDYLIB} ../libs/${LIBGDPDYLIB} ../libs/${LIBGDPJSDYLIB} ../libs/${LIBEPSO} ../libs/${LIBGDPSO} ../libs/${LIBGDPJSSO}

npmPublish: $(LIBS)
	(cd ..; npm publish -access public)


#
#  Administrative stuff
#

clean:
	-${RM} -f ${ALL} *.o *.core
	rm -rf ../ep ../gdp
${OBJS}: ${HFILES}
