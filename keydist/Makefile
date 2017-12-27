#
# Temporary Makefile 
#

# These are generally changed on make command line
DESTDIR=
LOCALROOT=	/usr


# These generally do not need to be changed
INSTALLROOT=	${DESTDIR}${LOCALROOT}
LIBDIR=			${INSTALLROOT}/lib
INCDIR=			${INSTALLROOT}/include
LOCALLIBSDIR=	../libs
ALLDIRS=		${LIBDIR}	\
				${INCDIR}/ks


CURDIR=	.
INCROOT=	..

INCS= -I${INCROOT}\
		-I${CURDIR} \
		-I${INCDIR} \
		-I${INCROOT}/hs \
		-I${INCROOT}/ep \
		-I${INCROOT}/gdp \
		-I${INCROOT}/acrule \


LIBROOT= ${INCROOT}
LIBSEARCH0=	 \
		-L${LOCALROOT}/lib \
		-L${LIBROOT}/libs \
		-L${LIBROOT}/ep \
		-L${LIBROOT}/gdp \
		-L${LIBROOT}/hs \

LIBSEARCH1=	 ${LIBSEARCH0} \
		-L${LIBROOT}/acrule \

LDFLAGS0=	${LIBSEARCH0}
LDFLAGS1=	${LIBSEARCH1}
LDFLAGS2=	 \
		-L${LIBROOT}/gdp \
		-L${LIBROOT}/ep \
		-L${LIBROOT}/acrule \
		-L${LIBROOT}/hs \
		-L${LOCALROOT}/lib \


LIBCRYPTO=	-lcrypto
LIBEP=	-lep
LIBAC=	-lac
LIBHS=	-lhs
LIBGDP=	-lgdp
LIBEVENT2= -levent -levent_pthreads -pthread
LDLIBS_COM=	\
		${LIBHS} \
		${LIBGDP} \
		${LIBEP} \

LDLIBS=	${LDLIBS_COM}	\
		${LIBCRYPTO} \
		${LIBAC} \
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
CP=		cp 

INSTALL=	install
MKDIR=		mkdir -p 
RANLIB=		ranlib
#SHARED=		-shared 
SHARED=	-shared ${CRYPTOLFLAGS}

LIBNAME=	libks

KSLIBMAJVER=	1
KSLIBMINVER=	1
KSLIBVER=		${KSLIBMAJVER}.${KSLIBMINVER}
LIBALL=			${LIBNAME}.a ${LIBNAME}.so.${KSLIBVER}


KS_HDRS= \
		kds_priv.h	\
		kdc_api.h


# current only gdp_util2 compile test version 
COM_OBJS= ksd_key_data.o session_manager.o
OBJS= ${COM_OBJS}	kds_pubsub.o \
		ksd_key_manager.o ksd_key_gen.o \
		ksd_ac_client.o ksd_data_manager.o \
		kds_proto.o kds_gcl.o kds_adv.o 
OBJS1= ${OBJS} keydistd.o 
OBJS2= ${COM_OBJS} kdc_gcl_ops.o kdc_api.o gdp_devicekey.o gdp_logdatakey.o 
OBJS3= ${OBJS2} gdp-writer-enc.o
OBJS4= ${OBJS2} gdp-reader-dec.o
OBJS5= ${OBJS2} kds_manager.o 


#ALL= test
ALL= keydistd  gdp-writer gdp-reader manager_kds ${LIBALL}
CLEANALL=	${ALL} 
#CLEANALL=	${ALL} ${OBJS1} ${OBJS2} ${OBJS3} ${OBJS4} ${OBJS5}

all: ${ALL}

keydistd: ${OBJS1} 
	${CC} -o $@ ${OBJS1} ${LDLIBS} ${LDFLAGS1}


gdp-writer: ${OBJS3} 
	${CC} -o $@ ${OBJS3} ${LDLIBS} ${LDFLAGS1}
#	${CC} -o $@ ${OBJS3} ${LDLIBS} ${LDFLAGS2}
#	${CC} -o $@ ${LDFLAGS1} ${OBJS3} ${LDLIB} 

gdp-reader: ${OBJS4} 
	${CC} -o $@ ${OBJS4} ${LDLIBS} ${LDFLAGS1}

manager_kds: ${OBJS5} 
	${CC} -o $@ ${OBJS5} ${LDLIBS} ${LDFLAGS1}



${LIBNAME}.a: ${OBJS2}
	${AR} -r $@ ${OBJS2}
	${RANLIB} $@
	${RM} -f  ${LOCALLIBSDIR}/$@
	${INSTALL} $@ ${LOCALLIBSDIR}


${LIBNAME}.so.${KSLIBVER}: ${OBJS2}
	${CC} ${SHARED} -o $@ ${LDFLAGS0} ${OBJS2} ${LDLIBS_COM}
	${INSTALL} $@ ${LOCALLIBSDIR}
	sh ${LOCALLIBSDIR}/makelinks.sh ac ${KSLIBMAJVER} ${KSLIBMINVER} ${LOCALLIBSDIR}



clean:
	-rm  -f ${CLEANALL} *.o *.core  
#	-rm  -f ${LIBNAME}.* *.o *.core  


%.o:	%.c
	${CC} -o $@ -c ${CFLAGS} $<


# later : install service program 
PUBALL=	${LIBALL} ${KS_HDRS}  

install: ${ALLDIRS} ${PUBALL}
	${CP} ${KS_HDRS} ${INCDIR}/ks 
	${INSTALL} ${LIBALL} ${LIBDIR}
	sh ${LOCALLIBSDIR}/makelinks.sh ac ${KSLIBMAJVER} ${KSLIBMINVER} ${LIBDIR}


${ALLDIRS}:
	${MKDIR} $@


