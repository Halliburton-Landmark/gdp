
#
#  Makefile for libhs
#

# These are generally changed on make command line
DESTDIR=
LOCALROOT=	/usr
LOCAL1=		/usr/local
LOCAL2=		/opt/local
LOCAL3=		/usr/local/opt/openssl


# These generally do not need to be changed
INSTALLROOT=	${DESTDIR}${LOCALROOT}
LIBDIR=		${INSTALLROOT}/lib
INCDIR=		${INSTALLROOT}/include
LOCALLIBSDIR=	../libs
ALLDIRS=	${LIBDIR} \
			${INCDIR}/hs



LIBNAME=	libhs

CURDIR=	.
INCROOT=	..


INCS= -I${INCROOT} \
		-I${CURDIR} \
		-I${INCDIR} \
		-I${LOCAL1}/include \
		-I${LOCAL2}/include \
		-I${LOCAL3}/include


LIBROOT= ${INCROOT}
LIBSEARCH=	-L${CURDIR} \
		-L${LIBROOT}/libs \
		-L${LIBROOT}/ep \
		-L${LIBROOT}/gdp \
		-L${LOCALROOT}/lib \
		-L${LOCAL1}/lib \
		-L${LOCAL2}/lib \
		-L${LOCAL3}/lib


LDFLAGS=	${LIBSEARCH}
LIBCRYPTO=	-lcrypto
LIBEP=	-lep
LIBGDP=	-lgdp
LIBEVENT2= -levent -levent_pthreads
LDLIBS=	${LIBCRYPTO} \
		${LIBEP} \
		${LIBGDP} \
		${LIBEVENT2}
#LATER : check avahi?? 



CC=		cc
PG=
WALL=		-Wall
G=		-g
O=		-O
STD=
COPTS=		${PG} ${WALL} $G $O ${STD}
CFLAGS=		${COPTS} ${INCS} -fPIC


AR=		ar
CP=		cp
RM=		rm
LD=		ld
INSTALL=	install
MKDIR=		mkdir -p

RANLIB=		ranlib
#SHARED=		-shared 
SHARED=	-shared ${CRYPTOLFLAGS}


HSLIBMAJVER=	1
HSLIBMINVER=	1
HSLIBVER=		${HSLIBMAJVER}.${HSLIBMINVER}
LIBALL=			${LIBNAME}.a ${LIBNAME}.so.${HSLIBVER}
#LIBALL=			${LIBNAME}.so.${HSLIBVER}
#LIBALL=			${LIBNAME}.a 


HS_HDRS= \
		hs_errno.h \
		hs_list.h \
		hs_file.h \
		hs_util.h \
		hs_hashchain.h \
		hs_symkey_gen.h \
		hs_stdin_helper.h \
		gdp_extension.h \
		gcl_helper.h


HS_OBJS= hs_list.o \
		 hs_err.o \
		 hs_file.o \
		 hs_util.o \
		hs_hashchain.o \
		 hs_symkey_gen.o \
		 hs_stdin_helper.o \
		 gcl_helper.o


CLEANALL=	${LIBALL}
INCALL=		${HS_HDRS}
PUBALL=		${CLEANALL} ${INCALL}
ALL=		${CLEANALL} ${PUBALL}

all: ${ALL} 

${LIBNAME}.a: ${HS_OBJS}
	${AR} -r $@ ${HS_OBJS}
	${RANLIB} $@
	${RM} -f  ${LOCALLIBSDIR}/$@
	${INSTALL} $@ ${LOCALLIBSDIR}


${LIBNAME}.so.${HSLIBVER}: ${HS_OBJS}
	${CC} ${SHARED} -o $@ ${LDFLAGS} ${HS_OBJS} ${LDLIBS}
	${INSTALL} $@ ${LOCALLIBSDIR}
	sh ${LOCALLIBSDIR}/makelinks.sh hs ${HSLIBMAJVER} ${HSLIBMINVER} ${LOCALLIBSDIR}

# HSMOON START
clean:
	-rm  -f ${LIBNAME}.* *.o *.core  


%.o:	%.c
	${CC} -o $@ -c ${CFLAGS} $<



install: ${ALLDIRS} ${PUBALL}
	${CP} ${INCALL} ${INCDIR}/hs 
	${INSTALL} ${LIBALL} ${LIBDIR}
	sh ${LOCALLIBSDIR}/makelinks.sh hs ${HSLIBMAJVER} ${HSLIBMINVER} ${LIBDIR}


${ALLDIRS}: 
	${MKDIR} $@


