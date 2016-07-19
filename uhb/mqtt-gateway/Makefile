#
#  ----- BEGIN LICENSE BLOCK -----
#	MTQQ Gateway to the Global Data Plane
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

BINALL=		mqtt-gdp-gateway \

SBINALL=	

SCRIPTS=	start-mqtt-gdp-gateway.sh \
		../../adm/common-support.sh \

MAN1ALL=	mqtt-gdp-gateway.1 \

MAN3ALL=

MAN5ALL=

MAN7ALL=

MAN8ALL=

MANALL=		${MAN1ALL} ${MAN3ALL} ${MAN5ALL} ${MAN7ALL} ${MAN8ALL}

# files that should be removed on "make clean"
CLEANALL=	${BINALL} ${SBINALL}
# files that should be made public
PUBALL=		${SCRIPTS} ${MANALL}
# "make all" targets (may be overlaps)
ALL=		${CLEANALL} ${PUBALL}

DESTDIR=
LOCALROOT=	/usr
INSTALLROOT=	${DESTDIR}${LOCALROOT}
BINDIR=		${INSTALLROOT}/bin
SBINDIR=	${INSTALLROOT}/sbin
MANDIR=		${INSTALLROOT}/share/man
ALLDIRS=	${BINDIR} \
		${SBINDIR} \
		${LIBDIR} \
		${INCDIR}/ep \
		${MANDIR}/man1 \

INSTALL=	install

LOCAL1=		/usr/local
LOCAL2=		/opt/local
INCROOT=	../..
INCSEARCH=	-I${INCROOT} -I${LOCAL1}/include -I${LOCAL2}/include
LIBROOT=	${INCROOT}
LIBSEARCH=	-L${LIBROOT}/gdp \
		-L${LIBROOT}/ep \
		-L${LOCAL1}/lib \
		-L${LOCAL2}/lib
INCGDP=		-I${INCROOT}
LIBGDP=		-lgdp
INCEP=
LIBEP=		-lep
LIBJANSSON=	-L${LOCAL1}/lib -L${LOCAL2}/lib -ljansson
LIBEVENT2=	-levent -levent_pthreads -pthread
INCCRYPTO=	-I${CRYPTOROOT}/include
LIBCRYPTO=	-lcrypto
LIBAVAHI=	-lavahi-client -lavahi-common
LIBMOSQ=	-lmosquitto
INCS=		${INCSEARCH} ${INCGDP} ${INCEP} \
		${INCJANSSON} ${INCEVENT2} ${INCCRYPTO} ${INCAVAHI} \
		${INCMOSQ}
LDFLAGS=	${LIBSEARCH} ${LIBGDP} ${LIBEP} \
		${LIBJANSSON} ${LIBEVENT2} ${LIBCRYPTO} ${LIBAVAHI} \
		${LIBMOSQ}
PG=
O=		-O
WALL=		-Wall
STD=
COPTS=		${PG} -g ${WALL} $O ${STD}
CFLAGS=		${COPTS} ${INCS}
MKDIR=		mkdir -p

all:	${ALL}

mqtt-gdp-gateway:	mqtt-gdp-gateway.o
	${CC} -o $@ mqtt-gdp-gateway.o ${LDFLAGS}

clean:
	-rm -f ${CLEANALL} *.o *.core

install:	${BINDIR} ${SBINDIR} ${ALL} \
		${SCRIPTS} ${MANALL} FORCE
	test -z "${BINALL}"  || ${INSTALL} ${BINALL}  ${BINDIR}
	test -z "${SBINALL}" || ${INSTALL} ${SBINALL} ${SBINDIR}
	test -z "${MAN1ALL}" || ${INSTALL} ${MAN1ALL} ${MANDIR}/man1
	test -z "${MAN3ALL}" || ${INSTALL} ${MAN3ALL} ${MANDIR}/man3
	test -z "${MAN5ALL}" || ${INSTALL} ${MAN5ALL} ${MANDIR}/man5
	test -z "${MAN7ALL}" || ${INSTALL} ${MAN7ALL} ${MANDIR}/man7
	test -z "${MAN8ALL}" || ${INSTALL} ${MAN8ALL} ${MANDIR}/man8
	INSTALLROOT=${INSTALLROOT} sh install-mqtt-gdp-gateway.sh

${BINDIR} ${SBINDIR}:
	${MKDIR} $@

ADM=		../../adm
UPDATE_LICENSE=	${ADM}/update-license.sh

update-license:
	${UPDATE_LICENSE} Makefile *.[ch]

FORCE:
