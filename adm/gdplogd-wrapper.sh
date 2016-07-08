#!/bin/sh
(test -r /usr/local/etc/gdp.conf.sh && . /usr/local/etc/gdp.conf.sh) ||
		(test -r /etc/gdp.conf.sh && . /etc/gdp.conf.sh)
: ${GDP_ROOT:=/usr}
: ${GDPLOGD_ARGS:=}
: ${GDPLOGD_BIN:=$GDP_ROOT/sbin/gdplogd}
: ${GDPLOGD_LOG:=/var/log/gdp/gdplogd.log}

{
	echo "Sleeping for 5s"
	sleep 5
	echo `date +"%F %T %z"` Running $GDPLOGD_BIN $GDPLOGD_ARGS
	exec $GDPLOGD_BIN $GDPLOGD_ARGS
} >> $GDPLOGD_LOG 2>& 1
