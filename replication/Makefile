#
#  ----- BEGIN LICENSE BLOCK -----
#	GDP: Global Data Plane Support Library
#	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
#
#	Copyright (c) 2015, Regents of the University of California.
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
#	OR MODIFICATIONS.
#  ----- END LICENSE BLOCK -----
#

DESTDIR=
LOCALROOT=	/usr
CRYPTOROOT=	/opt/local
INSTALLROOT=	${DESTDIR}${LOCALROOT}
INCDIR=		${INSTALLROOT}/include
LIBDIR=		${INSTALLROOT}/lib
INSTALL=	install

OBJS=	\
	replication_proto.o \


PUBHFILES=	\
	replication.h \

PRIVHFILES=	\
	../gdp/gdp.h \
	../gdp/gdp_priv.h \
	../gdp/gdp_pdu.h \
	../gdp/gdp_stat.h \

HFILES=		${PUBHFILES} ${PRIVHFILES}

GDPLIBMAJVER=	0
GDPLIBMINVER=	2
GDPLIBVER=	${GDPLIBMAJVER}.${GDPLIBMINVER}
INCALL=		${PUBHFILES}
LIBALL=		librep.a
ALL=		${INCALL} ${LIBALL}

LOCAL1=		/usr/local
LOCAL2=		/opt/local
INCROOT=	..
INCSEARCH=	-I${LOCAL1}/include -I${LOCAL2}/include
LIBROOT=	${INCROOT}
INCCRYPTO=	-I${CRYPTOROOT}/include
LIBCRYPTO=	-lcrypto
INCEVENT2=
LIBEVENT2=	-levent -levent_pthreads
INCAVAHI=
LIBAVAHI=	-lavahi-client -lavahi-common
INCEP=		-I${INCROOT}
LIBEP=		-lep
INCS=		${INCSEARCH} ${INCEP} ${INCEVENT2} ${INCCRYPTO} ${INCAVAHI}
LIBSEARCH=	-L${CRYPTOROOT}/lib \
		-L${LIBROOT}/ep \
		-L${LOCAL1}/lib \
		-L${LOCAL2}/lib
LDFLAGS= 	${LIBSEARCH} ${LIBEP} ${LIBEVENT2} ${LIBCRYPTO} ${LIBAVAHI}
PG=
O=		-O
WALL=		-Wall
STD=
COPTS=		${PG} -g ${WALL} $O ${STD}
CFLAGS=		${COPTS} ${INCS} -fPIC

LN=		ln
MV=		mv
CP=		cp
RM=		rm
MKDIR=		mkdir -p
RANLIB=		ranlib
SHARED=		-shared ${CRYPTOLFLAGS}
#SHARED=		-shared ${CRYPTOLFLAGS} -Wl,-soname,$@

all: ${ALL}

librep.a: ${OBJS}
	${AR} -r $@ ${OBJS}
	${RANLIB} $@

${OBJS}: ${HFILES}


clean:
	-rm -f ${LIBALL} *.o *.core

install:


ADM=		../adm
UPDATE_LICENSE=	${ADM}/update-license.sh

update-license:
	${UPDATE_LICENSE} Makefile *.[ch]

FORCE:
