#
#  ----- BEGIN LICENSE BLOCK -----
#  ----- END LICENSE BLOCK -----
#

# files that should be made public (installed in /usr/local/share/doc)
# 	It would be nice to make .html versions of the .md files, but
# 	pandoc is not installed on the Jenkins machine.
PUBALL= \
		gdp-gcl-metadata.html \
		gdp-programmatic-api.html \
		gdp-proto-v4-proposal.md \
		gdp-protocol-v3.html \
		gdp-rest-interface.html \
		gdplogd-vizstats.md \
		Libep.dbk \
		Libep.pdf \

# files that are auto-generated (should be cleaned)
CLEANALL= \
	  	gdp-proto-v4-proposal.html \
		gdplogd-vizstats.html \

ALL=		${CLEANALL} ${PUBALL}

DESTDIR=
LOCALROOT=	/usr
INSTALLROOT=	${DESTDIR}${LOCALROOT}
DOCDIR=		${INSTALLROOT}/share/doc/gdp

# remove this if pandoc gets installed on the Jenkins machine
jenkins-all: ${PUBALL}

all: ${ALL}

clean:
	rm -f ${CLEANALL}

${DOCDIR}:
	mkdir -p $@

install: ${DOCDIR} ${PUBALL}
	cp ${PUBALL} ${DOCDIR}

ADM=		../adm
UPDATE_LICENSE=	${ADM}/update-license.sh

update-license:
	${UPDATE_LICENSE} Makefile ${ALL}

PANDOC=		pandoc
PANFLAGS=	-sS -c gdpdoc.css --self-contained

.SUFFIXES: .md .html

.md.html:
	${PANDOC} ${PANFLAGS} -o $@ $<
