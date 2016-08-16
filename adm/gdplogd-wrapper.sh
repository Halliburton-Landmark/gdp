#!/bin/sh
{ test -r /usr/local/etc/gdp.conf.sh && . /usr/local/etc/gdp.conf.sh; } ||
	{ test -r /etc/gdp.conf.sh && . /etc/gdp.conf.sh; }

#
#  Wrapper for starting up gdplogd
#

# configure defaults
: ${GDP_ROOT:=/usr}
: ${GDP_LOG_DIR:=/var/log/gdp}
: ${GDPLOGD_ARGS:="-D*=10"}
: ${GDPLOGD_BIN:=$GDP_ROOT/sbin/gdplogd}
: ${GDPLOGD_LOG:=$GDP_LOG_DIR/gdplogd.log}
: ${LLOGGER:=llogger}

{
	echo `date +"%F %T %z"` Running $GDPLOGD_BIN $GDPLOGD_ARGS
	exec $GDPLOGD_BIN $GDPLOGD_ARGS
} 2>& 1 | ${LLOGGER} -a $GDPLOGD_LOG
