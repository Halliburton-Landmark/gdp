#!/bin/sh

#
#  Customize scripts for various pathnames and other parameters.
#
#	Defaults are defined here.
#
#	Usage: adm/customize.sh source-file target-dir
#

source_file=$1
target_dir=$2

source_dir=`dirname $source_file`
source_root=`basename $source_file .template`
source_file="$source_dir/$source_root.template"
#echo source_file=$source_file, source_dir=$source_dir, source_root=$source_root, target_dir=$target_dir

(test -r /usr/local/etc/gdp.conf.sh && . /usr/local/etc/gdp.conf.sh) ||
	        (test -r /etc/gdp.conf.sh && . /etc/gdp.conf.sh)
: ${GDP_LOGDIR:=/var/log/gdp}
: ${GDP_ROOT:=/usr}
: ${GDP_ROUTER_CONF:=/etc/gdp-router-click.conf}
: ${GDP_SYSLOG_FACILITY:=local4}
: ${GDP_SYSLOG_LEVEL:=notice}
: ${GDP_USER:=gdp}
: ${GDPLOGD_ARGS:=}

(
	echo "# Generated" `date +"%F %T %z"` from $source_file
	sed \
		-e "s;@GDP_LOGDIR@;$GDP_LOGDIR;g" \
		-e "s;@GDP_ROOT@;$GDP_ROOT;g" \
		-e "s;@GDP_ROUTER_CONF@;$GDP_ROUTER_CONF;g" \
		-e "s;@GDP_SYSLOG_FACILITY@;$GDP_SYSLOG_FACILITY;g" \
		-e "s;@GDP_SYSLOG_LEVEL@;$GDP_SYSLOG_LEVEL;g" \
		-e "s;@GDP_USER@;$GDP_USER;g" \
		-e "s;@GDPLOGD_ARGS@;$GDPLOGD_ARGS;g" \

	echo "# End of generated" $1
) < $source_file > $target_dir/$source_root