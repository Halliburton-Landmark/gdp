#
#  ----- BEGIN LICENSE BLOCK -----
#  ----- END LICENSE BLOCK -----
#

ALL=		machine-mon

DESTDIR=
LOCALROOT=	/usr
INSTALLROOT=	${DESTDIR}${LOCALROOT}
BINDIR=		${INSTALLROOT}/bin
SBINDIR=	${INSTALLROOT}/sbin
ALLDIRS=	${BINDIR} \
		${SBINDIR} \
		${LIBDIR} \
		${INCDIR}/ep \
		${MANDIR}/man1 \
		${MANDIR}/man3 \
		${MANDIR}/man5 \
		${MANDIR}/man7 \
		${MANDIR}/man8

INSTALLFLAGS=
INSTALL=	install ${INSTALLFLAGS}

LOCAL1=		/usr/local
LOCAL2=		/opt/local
LOCAL3=		/usr/local/opt/openssl
SANITIZE=
#SANITIZE+=	-fsanitize=address
#SANITIZE+=	-fsanitize=thread
#SANITIZE+=	-fsanitize=memory
#SANITIZE+=	-fsanitize=undefined

INCROOT=	..
INCSEARCH=	${INCLOCAL} \
		-I${INCROOT} \
		-I${LOCAL1}/include \
		-I${LOCAL2}/include \
		-I${LOCAL3}/include \
		`mariadb_config --cflags`
LIBROOT=	${INCROOT}
LIBSEARCH=	${LIBLOCAL} \
		-L${LIBROOT}/gdp \
		-L${LIBROOT}/ep \
		-I${LOCAL1}/lib \
		-L${LOCAL2}/lib \
		-L${LOCAL3}/lib
LIBGDP=		-lgdp
LIBEP=		-lep
LIBJANSSON=	-L${LOCAL1}/lib -L${LOCAL2}/lib -ljansson
LIBEVENT2=	-levent -levent_pthreads -pthread
LIBCRYPTO=	-lcrypto
LIBPROTO_C=	-lprotobuf-c
LIBAVAHI=	-lavahi-client -lavahi-common
LIBADD=		`sh ../adm/add-libs.sh`
LDLIBS=		${LIBGDP} \
		${LIBEP} \
		${LIBEVENT2} \
		${LIBCRYPTO} \
		${LIBPROTO_C} \
		${LIBAVAHI} \
		${LIBJANSSON} \
		${LIBADD}
INCS=		${INCSEARCH} ${INCGDP} ${INCEP}
LDFLAGS+=	${LIBSEARCH} ${LDLIBS} ${SANITIZE}
CC=		cc
PG=
WALL=		-Wall
G=		-g
O=		-O
STD=
COPTS=		${PG} ${WALL} $G $O ${STD} ${SANITIZE}
CFLAGS+=	${COPTS} ${INCS}
MKDIR=		mkdir -p

all:	${ALL}

# Build without avahi, the zero-conf facility that
# can be tricky to compile under Mac OS X.
all_noavahi:
	${MAKE}	STD=-DGDP_OSCF_USE_ZEROCONF=0 LIBAVAHI=  all

machine-mon:	machine-mon.o
	${CC} -o $@ machine-mon.o ${LDFLAGS}

clean:
	-rm -f ${ALL} *.o *.core

${ALLDIRS}:
	${MKDIR} $@
