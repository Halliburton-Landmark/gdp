#
#  Install MariaDB container
#

GDP_ROOT=	../..
GDP_ADM=	${GDP_ROOT}/adm

DESTDIR=
LOCALROOT=	/usr
INSTALLROOT=	${DESTDIR}${LOCALROOT}
BINDIR=		${INSTALLROOT}/bin
SBINDIR=	${INSTALLROOT}/sbin

INSTALLFLAGS=
INSTALL=	install ${INSTALLFLAGS}

install: gdp-mariadb-start.sh gdp-mariadb.service.template
	${INSTALL} gdp-mariadb-start.sh ${SBINDIR}
	${GDP_ADM}/customize.sh gdp-mariadb.service /etc/systemd/system
