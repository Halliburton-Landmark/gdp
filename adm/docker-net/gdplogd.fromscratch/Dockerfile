## To create an image from this Dockerfile, rename this file to 'Dockerfile', and do 
## $ docker build -t <imagename> .

FROM ubuntu:16.04

MAINTAINER Nitesh Mor <mor@eecs.berkeley.edu>

# stop stupid debconf errors
ENV DEBIAN_FRONTEND noninteractive

# Thee are the configuration parameters for GDP runtime
ENV GDP_ROUTER gdp-01.eecs.berkeley.edu:8009
ENV GDPLOGD_NAME docker.gdplogd

ADD . /src
WORKDIR /src

RUN ./install-gdplogd.sh
RUN ./config-gdplogd.sh

ENTRYPOINT ["/bin/sh", "/src/start-gdplogd.sh"]
