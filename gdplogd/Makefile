ALL=		gdplogd

LOCAL1=		/usr/local
LOCAL2=		/opt/local
INCROOT=	..
LIBROOT=	${INCROOT}
#LIBSEARCH=	-L${INCROOT}/libs
LIBSEARCH=	-L${LIBROOT}/ep \
		-L${LIBROOT}/gdp \
		-L${LOCAL1}/lib \
		-L${LOCAL2}/lib
INCGDP=		-I${INCROOT}
LIBGDP=		-lgdp
INCEP=
LIBEP=		-lep
INCEVENT2=	-I${LOCAL1}/include -I${LOCAL2}/include
LIBEVENT2=	-levent -levent_pthreads -pthread
INCCRYPTO=
LIBCRYPTO=	-lcrypto
INCS=		${INCGDP} ${INCEP} ${INCEVENT2} ${INCCRYPTO}
LDFLAGS=	${LIBSEARCH} ${LIBGDP} ${LIBEP} ${LIBEVENT2} ${LIBCRYPTO}
PG=
O=		-O
WALL=		-Wall
STD=
COPTS=		${PG} -g ${WALL} $O ${STD}
CFLAGS=		${COPTS} ${INCS}

OBJS=	\
		logd.o \
		logd_gcl.o \
		logd_physlog.o \
		logd_proto.o \
		logd_pubsub.o \
		logd_circular_buffer.o \

HDEPS=	\
		logd.h \
		logd_physlog.h \
		logd_pubsub.h \
		logd_circular_buffer.h \
		../gdp/gdp.h \
		../gdp/gdp_pdu.h \

LIBDEPS= \
	 	../gdp/libgdp.a \

all: ${ALL}

gdplogd: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

${OBJS}: ${HDEPS} ${LIBDEPS}

clean:
	-rm -f ${ALL} *.o *.core