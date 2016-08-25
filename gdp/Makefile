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

# Version number of library.  All others are derived from here.
GDP_VERSION_MAJOR=	0
GDP_VERSION_MINOR=	7
GDP_VERSION_PATCH=	1

# These are generally changed on make command line
DESTDIR=
LOCALROOT=	/usr
LOCAL1=		/usr/local
LOCAL2=		/opt/local

# These generally do not need to be changed
INSTALLROOT=	${DESTDIR}${LOCALROOT}
INCDIR=		${INSTALLROOT}/include
LIBDIR=		${INSTALLROOT}/lib
BINDIR=		${INSTALLROOT}/bin
SBINDIR=	${INSTALLROOT}/sbin
MANDIR=		${INSTALLROOT}/share/man
ALLDIRS=	${BINDIR} \
		${SBINDIR} \
		${LIBDIR} \
		${INCDIR}/gdp \
		${MANDIR}/man7 \
		${MANDIR}/man8
ADM=		../adm
LOCALLIBSDIR=	../libs
LIBNAME=	libgdp

BINALL=

SBINALL=

MAN1ALL=

MAN3ALL=

MAN5ALL=

MAN7ALL=	gdp.7 \

MAN8ALL=

MANALL=		${MAN1ALL} ${MAN3ALL} ${MAN5ALL} ${MAN7ALL} ${MAN8ALL}

OBJS_ZC = \
	gdp_zc_client.o \
	gdp_zc_server.o

OBJS_MOST=	\
	gdp_api.o \
	gdp_buf.o \
	gdp_chan.o \
	gdp_crypto.o \
	gdp_event.o \
	gdp_gcl_cache.o \
	gdp_gcl_ops.o \
	gdp_gclmd.o \
	gdp_datum.o \
	gdp_main.o \
	gdp_pdu.o \
	gdp_proto.o \
	gdp_req.o \
	gdp_stat.o \
	gdp_subscr.o \
	gdp_util.o \
	$(OBJS_ZC)

OBJS=	\
	${OBJS_MOST} \
	gdp_version.o

PUBHFILES=	\
	gdp.h \
	gdp_buf.h \
	gdp_stat.h \
	gdp_version.h \
	gdp_zc_client.h \
	gdp_zc_server.h \

PRIVHFILES=	\
	gdp_event.h \
	gdp_gclmd.h \
	gdp_pdu.h \
	gdp_priv.h \

HFILES=		${PUBHFILES} ${PRIVHFILES}

GDPLIBVER=	${GDP_VERSION_MAJOR}.${GDP_VERSION_MINOR}
INCALL=		${PUBHFILES}
LIBALL=		libgdp.a libgdp.so.${GDPLIBVER}
# files that should be removed on "make clean"
CLEANALL=	${LIBALL} ${BINALL} ${SBINALL} gdp_version.h
# files that should be publicly installed
PUBALL=		${SCRIPTS} ${MANALL} ${INCALL} ${LIBALL}
# "make all" targets (may be overlap)
ALL=		${CLEANALL} ${PUBALL}

LOCAL1=		/usr/local
LOCAL2=		/opt/local
INCROOT=	..
INCSEARCH=	${INCLOCAL} \
		-I${INCROOT} \
		-I${LOCAL1}/include \
		-I${LOCAL2}/include
LIBROOT=	${INCROOT}
LIBCRYPTO=	-lcrypto
LIBEVENT2=	-levent -levent_pthreads
LIBAVAHI=	-lavahi-client -lavahi-common
INCEP=		-I${INCROOT}
LIBEP=		-lep
INCS=		${INCSEARCH} ${INCEP} ${INCEVENT2} ${INCAVAHI}
LIBSEARCH=	${LIBLOCAL} \
		-L${LIBROOT}/ep \
		-L${LOCAL1}/lib \
		-L${LOCAL2}/lib
LDFLAGS= 	${LIBSEARCH}
LDLIBS=		${LIBEP} \
		${LIBEVENT2} \
		${LIBCRYPTO} \
		${LIBAVAHI}
CC=		cc
PG=
WALL=		-Wall
G=		-g
O=		-O
STD=
COPTS=		${PG} ${WALL} $G $O ${STD}
CFLAGS=		${COPTS} ${INCS} -fPIC

LN=		ln
MV=		mv
CP=		cp
RM=		rm
MKDIR=		mkdir -p
RANLIB=		ranlib
SHARED=		-shared ${CRYPTOLFLAGS}
#SHARED=		-shared ${CRYPTOLFLAGS} -Wl,-soname,$@
INSTALL=	install

all: ${ALL}

# Build without avahi, the zero-conf facility that
# can be tricky to compile under Mac OS X.
all_noavahi:
	${MAKE}	STD=-DGDP_OSCF_USE_ZEROCONF=0 LIBAVAHI= OBJS_ZC= all

${LIBNAME}.a: ${OBJS}
	${AR} -r $@ ${OBJS}
	${RANLIB} $@
	${RM} -f ${LOCALLIBSDIR}/$@
	${INSTALL} $@ ${LOCALLIBSDIR}

${LIBNAME}.so.${GDPLIBVER}: ${OBJS}
	${CC} ${SHARED} -o $@ ${LDFLAGS} ${OBJS} ${LDLIBS}
	${RM} -f ${LOCALLIBSDIR}/$@
	${INSTALL} $@ ${LOCALLIBSDIR}
	sh ${LOCALLIBSDIR}/makelinks.sh gdp ${GDP_VERSION_MAJOR} ${GDP_VERSION_MINOR} ${LOCALLIBSDIR}

${OBJS}: ${HFILES}


gdp_version.o: gdp_version.c ${OBJS_MOST} Makefile gdp_version.h
	${CC} ${CFLAGS} \
		-D_VERSION_INFO_="\"`sh ../adm/get-version-info.sh`\"" \
		-c gdp_version.c

gdp_version.h: gdp_version.template gdp_version.sh Makefile
	sh gdp_version.sh ${GDP_VERSION_MAJOR} ${GDP_VERSION_MINOR} ${GDP_VERSION_PATCH}

#
#  Administrative stuff
#

# cleanup
clean:
	-rm -f ${CLEANALL} *.o *.core

# system installation
install:	${ALLDIRS} ${PUBALL}
	test -z "${BINALL}"  || ${INSTALL} ${BINALL}  ${BINDIR}
	test -z "${SBINALL}" || ${INSTALL} ${SBINALL} ${SBINDIR}
	test -z "${MAN1ALL}" || ${INSTALL} ${MAN1ALL} ${MANDIR}/man1
	test -z "${MAN3ALL}" || ${INSTALL} ${MAN3ALL} ${MANDIR}/man3
	test -z "${MAN7ALL}" || ${INSTALL} ${MAN7ALL} ${MANDIR}/man7
	test -z "${MAN8ALL}" || ${INSTALL} ${MAN8ALL} ${MANDIR}/man8
	${INSTALL} ${INCALL} ${INCDIR}/gdp
	${INSTALL} ${LIBALL} ${LIBDIR}
	sh ${LOCALLIBSDIR}/makelinks.sh gdp ${GDP_VERSION_MAJOR} ${GDP_VERSION_MINOR} ${LIBDIR}

${ALLDIRS}:
	${MKDIR} $@

# local installation (into source tree, needed for Java & Debian package build)
# XXX Why is this needed?  It's done when the libraries are built.
install-local:	${LOCALLIBSDIR} ${LIBALL}
	${INSTALL} ${LIBALL} ${LOCALLIBSDIR}
	sh ${LOCALLIBSDIR}/makelinks.sh gdp ${GDP_VERSION_MAJOR} ${GDP_VERSION_MINOR} ${LOCALLIBSDIR}

UPDATE_LICENSE=	${ADM}/update-license.sh

update-license:
	${UPDATE_LICENSE} Makefile *.[ch]

FORCE:
