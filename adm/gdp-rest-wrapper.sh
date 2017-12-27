#!/bin/sh
{ test -r /usr/local/etc/gdp.conf.sh && . /usr/local/etc/gdp.conf.sh; } ||
	{ test -r /etc/gdp.conf.sh && . /etc/gdp.conf.sh; }

#
#  Wrapper for starting up gdp-rest
#

# configure defaults
: ${GDP_ROOT:=/usr}
: ${GDP_LOG_DIR:=/var/log/gdp}
: ${GDP_REST_ARGS:="-D*=10"}
: ${GDP_REST_BIN:=$GDP_ROOT/sbin/gdp-rest}
: ${GDP_REST_LOG:=$GDP_LOG_DIR/gdp-rest.log}
: ${LLOGGER:=llogger}

{
	echo `date +"%F %T %z"` Running $GDP_REST_BIN $GDP_REST_ARGS
	exec $GDP_REST_BIN $GDP_REST_ARGS
} 2>& 1 | ${LLOGGER} -a $GDP_REST_LOG
