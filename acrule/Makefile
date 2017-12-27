#
# AC Rule library (libac) Makefile 
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
			${INCDIR}/ac



LIBNAME=	libac

CURDIR=	.
INCROOT=	..


INCS= -I${INCROOT} \
		-I${CURDIR} \
		-I${INCDIR} \


LIBROOT= ${INCROOT}
LIBSEARCH=	-L${CURDIR} \
		-L${LIBROOT}/libs \
		-L${LIBROOT}/ep \
		-L${LIBROOT}/gdp \
		-L${LIBROOT}/hs \
		-L${LOCALROOT}/lib \


LDFLAGS=	${LIBSEARCH}
LIBCRYPTO=	-lcrypto
LIBEP=	-lep
LIBHS=	-lhs
LIBGDP=	-lgdp
LIBEVENT2= -levent -levent_pthreads
LDLIBS=	${LIBCRYPTO} \
		${LIBEP} \
		${LIBHS} \
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


ACLIBMAJVER=	1
ACLIBMINVER=	1
ACLIBVER=		${ACLIBMAJVER}.${ACLIBMINVER}
LIBALL=			${LIBNAME}.a ${LIBNAME}.so.${ACLIBVER}
#LIBALL=			${LIBNAME}.so.${HSLIBVER}
#LIBALL=			${LIBNAME}.a 


AC_HDRS= \
		ac_token.h \
		acrule.h \
		DAC_UID_1.h


AC_OBJS=  \
		ac_token.o \
		acrule_api.o \
		DAC_UID_1.o


CLEANALL=	${LIBALL}
INCALL=		${AC_HDRS}
PUBALL=		${CLEANALL} ${INCALL}
ALL=		${CLEANALL} ${PUBALL}

all: ${ALL} 

${LIBNAME}.a: ${AC_OBJS}
	${AR} -r $@ ${AC_OBJS}
	${RANLIB} $@
	${RM} -f  ${LOCALLIBSDIR}/$@
	${INSTALL} $@ ${LOCALLIBSDIR}


${LIBNAME}.so.${ACLIBVER}: ${AC_OBJS}
	${CC} ${SHARED} -o $@ ${LDFLAGS} ${AC_OBJS} ${LDLIBS}
	${INSTALL} $@ ${LOCALLIBSDIR}
	sh ${LOCALLIBSDIR}/makelinks.sh ac ${ACLIBMAJVER} ${ACLIBMINVER} ${LOCALLIBSDIR}

# HSMOON START
clean:
	-rm  -f ${LIBNAME}.* *.o *.core  


%.o:	%.c
	${CC} -o $@ -c ${CFLAGS} $<



install: ${ALLDIRS} ${PUBALL}
	${CP} ${INCALL} ${INCDIR}/ac 
	${INSTALL} ${LIBALL} ${LIBDIR}
	sh ${LOCALLIBSDIR}/makelinks.sh ac ${ACLIBMAJVER} ${ACLIBMINVER} ${LIBDIR}


${ALLDIRS}: 
	${MKDIR} $@


