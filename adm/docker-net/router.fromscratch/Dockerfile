## To create an image from this Dockerfile, rename this file to 'Dockerfile', and do 
## $ docker build -t <imagename> .

FROM ubuntu:16.04

MAINTAINER Nitesh Mor <mor@eecs.berkeley.edu>

# stop stupid debconf errors
ENV DEBIAN_FRONTEND noninteractive

ADD . /src
WORKDIR /src

# To be able to compile the gdp router, we need the gdp library to be installed.
RUN ./install-gdp.sh
RUN ./install-router.sh

ENTRYPOINT ["/bin/sh", "/src/start-router.sh"]
