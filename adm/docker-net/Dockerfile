##
##  Master Dockerfile for GDP core.  Has multiple targets.  You can
##  build intermediate targets using:
##	docker build --target <target> -t <imagename> .
##
##  For example, to create just the C development environment:
##	docker build --target gdp-dev-c -t gdp-dev-c .
##
##  There are also parameters you can set using --build-arg:
##	GDP_ROUTER		The default router for clients (IP address).
##	GDP_HONGD_SERVER	The Human to GDP name directory (IP address).
##	GDP_CREATION_SERVICE	The default creation service name (GDPname).
##	GDPLOGD_NAME		The default name for gdplogd (GDPname).
##				Can be changed at run time using
##				  docker run -e GDPLOGD_NAME=xxx gdplogd.
##


############################################################
#
#  The source code.  This isn't intended for distribution, but is
#  just a build step for other containers.
#	Installs prerequisite packages.
#	Installs the source code (from git).
#	Compiles (but does not install) the code.
#

FROM ubuntu:16.04 as gdp-src-base
ARG GDP_ROUTER=gdp-01.eecs.berkeley.edu
ENV GDP_ROUTER=$GDP_ROUTER
ARG GDP_CREATION_SERVICE
ENV GDP_CREATION_SERVCE=$GDP_CREATION_SERVICE
ARG GDP_HONGD_SERVER
ENV GDP_HONGD_SERVER=$GDP_HONGD_SERVER
ARG VER=latest
ENV VER=$VER
ARG BRANCH=master
ENV BRANCH=$BRANCH
COPY setup-src.sh /src/
WORKDIR /src
RUN VER=$VER /src/setup-src.sh && cd / && rm /src/setup-src.sh


############################################################
#
#  The C-based development environment.
#	Compiles and installs the libraries (but no clients).
#  The intent is that this will be used as a base image for C-based
#	application development.
#

FROM gdp-src-base AS gdp-dev-c

# stop stupid debconf errors
#ENV DEBIAN_FRONTEND noninteractive

WORKDIR /src/gdp
RUN make install-client && cd / && rm -r src


############################################################
#
#  The Python-based development environment.
#  The intent is that this will be used as a base image for Python-based
#	application development.
#

#FROM gdp-src-base AS gdp-dev-python
#COPY setup-python.sh /src/
#WORKDIR /src
#
#RUN /src/setup-python.sh && rm /src/setup-python.sh


############################################################
#
#  The base instance for a minimal execution environment.
#  Only to be used as a base for other containers.
#	Should be based on alpine, but I couldn't figure out how
#	to install the libraries needed to make the code work.
#	Static linking failed, and `apk` is inscrutable.
#

#FROM alpine:latest AS gdp-run-base
FROM gdp-src-base AS gdp-run-base
#RUN addgroup -g 321 gdp && adduser -S -u 321 -G gdp gdp
RUN adduser --system --uid 123 --group gdp
#RUN apk install \
#	avahi-libs-0.7-r1 \
#	XXX more XXX


############################################################
#
#  A gdplogd instance.
#
#	The data files are on an external volume.
#	Each container needs a known and unique id.
#	It is possible to override the default router.
#
#	You have to run this with:
#		glogs=/var/swarm/gdp/glogs	# set as appropriate
#		uid=<unique name>		# set as appropriate
#		router=<router name>		# optional
#		gdplogd_name=<name root>	# optional
#		docker run \
#			-v $glogs:/var/swarm/gdp/glogs \
#			-p 8009:8009 \
#			-e GDP_ROUTER=$router \
#			-e GDPLOGD_NAME=$gdplogd_name \
#			gdplogd
#
#	If GDP_ROUTER isn't set it defaults to gdp-01.eecs.berkeley.edu;
#		it would be better if this pointed to a local router.
#	If GDPLOGD_NAME isn't set it defaults to the container name,
#		which is essentially a random number.  This means that
#		attempts to create logs on this log server will need to
#		be customized each time the container starts up.
#

FROM gdp-run-base AS gdplogd
ARG GDPLOGD_NAME
ENV GDPLOGD_NAME=$GDPLOGD_NAME
ARG GDP_ROUTER=gdp-01.eecs.berkeley.edu
ENV GDP_ROUTER=$GDP_ROUTER
VOLUME /var/swarm/gdp/glogs

COPY setup-gdplogd.sh start-gdplogd.sh /src/
COPY --from=gdp-src-base \
	/src/gdp/gdplogd/gdplogd2 \
	/src/gdp/adm/gdplogd-wrapper.sh \
		/usr/sbin/
COPY --from=gdp-src-base \
	/src/gdp/util/llogger \
		/usr/bin/

RUN /src/setup-gdplogd.sh && rm /src/setup-gdplogd.sh
USER gdp:gdp

CMD ["/bin/sh", "/src/start-gdplogd.sh"]
