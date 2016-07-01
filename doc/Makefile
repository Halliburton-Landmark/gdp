#
#  ----- BEGIN LICENSE BLOCK -----
#  ----- END LICENSE BLOCK -----
#

# files that should be made public (installed in /usr/local/share/doc)
#   should include gdplogd-vizstats.html once pandoc is installed
PUBALL= \
		gdp-gcl-metadata.html \
		gdp-programmatic-api.html \
		gdp-protocol.html \
		gdp-rest-interface.html \
		Libep.dbk \
		Libep.pdf \

# files that are auto-generated (should be cleaned)
CLEANALL= \
		gdplogd-vizstats.html \

ALL=		${CLEANALL} ${PUBALL}

DESTDIR=
LOCALROOT=	/usr
INSTALLROOT=	${DESTDIR}${LOCALDIR}
DOCDIR=		${INSTALLROOT}/share/doc/gdp

all: ${ALL}

clean:
	-rm ${CLEANALL}

${DOCDIR}:
	mkdir -p $@

install: ${DOCDIR} ${PUBALL}
	cp ${PUBALL} ${DOCDIR}

PANDOC=		pandoc
PANFLAGS=	-sS

.SUFFIXES: .md .html

.md.html:
	${PANDOC} ${PANFLAGS} -o $@ $<
