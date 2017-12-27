#
# Utility related access control Makefile 
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

CURDIR=	.
INCROOT=	..


INCS= -I${INCROOT} \
		-I${INCROOT}/hs \
		-I${INCROOT}/acrule \
		-I${CURDIR} \
		-I${INCDIR} \
		-I${LOCAL1}/include \
		-I${LOCAL2}/include \
		-I${LOCAL3}/include


LIBROOT= ${INCROOT}
LIBSEARCH=	-L${CURDIR} \
		-L${LIBROOT}/libs \
		-L${LIBROOT}/hs \
		-L${LIBROOT}/acrule \
		-L${LIBROOT}/ep \
		-L${LIBROOT}/gdp \
		-L${LOCALROOT}/lib \
		-L${LOCAL1}/lib \
		-L${LOCAL2}/lib \
		-L${LOCAL3}/lib
LDFLAGS=	${LIBSEARCH}
LIBCRYPTO=	-lcrypto
LIBEP=	-lep
LIBHS=	-lhs
LIBAC=	-lac
LIBGDP=	-lgdp
LIBEVENT2= -levent -levent_pthreads
LDLIBS=	${LIBCRYPTO} \
		${LIBEP} \
		${LIBAC} \
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


RM=		rm
LD=		ld


# current only gdp_util2 compile test version 
OBJS=  acinfo_helper.o  app_helper.o
OBJS1= ${OBJS} ac_manager.o 
OBJS2=  token_generator.o  
#OBJS3= ${OBJS_O1} test.o 
#OBJS4= ${OBJS_O1} gdp-reader2.o 
#OBJS5= ${OBJS_O1} gdp-writer2.o 

ALL= ac_manager gen_token
#ALL= create_symkey encdec_symkey test enc-writer dec-reader
CLEANALL=	${ALL} 
#CLEANALL=	${ALL} ${OBJS1} ${OBJS2} ${OBJS3} ${OBJS4} ${OBJS5}

all: ${ALL}

ac_manager: ${OBJS1} 
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ ${OBJS1} ${LDLIBS}

gen_token: ${OBJS2} 
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ ${OBJS2} ${LDLIBS}

#dec-reader: ${OBJS4} 
#	${CC} ${CFLAGS} ${LDFLAGS} -o $@ ${OBJS4} ${LDLIBS}


clean:
	-rm  -f ${CLEANALL} *.o *.core  


%.o:	%.c
	${CC} -o $@ -c ${CFLAGS} $<

#LATER: INSTALL
