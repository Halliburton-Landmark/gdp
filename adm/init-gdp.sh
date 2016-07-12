#!/bin/sh
(test -r /usr/local/etc/gdp.conf.sh && . /usr/local/etc/gdp.conf.sh) ||
	(test -r /etc/gdp.conf.sh && . /etc/gdp.conf.sh)
: ${GDP_LOGDIR:=/var/log/gdp}
: ${GDP_ROOT:=/usr}
: ${GDP_USER:=gdp}
: ${GDPLOGD_DATADIR:=/var/swarm/gdp/gcls}

#
#  Initialize server hosts
#	This is specialized for Berkeley
#

#################### FUNCTIONS ####################

mkdir_gdp() {
	test -d $1 && return
	info "Creating $1 as ${GDP_USER}:${GDP_GROUP}"
	mkdir -p $1
	chmod ${2:-0755} $1
	chown ${GDP_USER}:${GDP_GROUP} $1
}

mkdir_gdp_opt() {
	test -d $1 && return
	info "Creating $1"
	mkdir -p $1
	chmod ${2:-0755} $1
	chown_gdp_opt $1
}

chown_gdp_opt() {
	test "$GDP_ROOT" != "~${GDP_USER}" && return
	info "Giving $1 to user ${GDP_USER}:${GDP_GROUP}"
	chown ${GDP_USER}:${GDP_GROUP} $1
}

if [ -d adm ]; then
	. adm/common-support.sh
else
	info() {
		echo "[I] $1" 1>&2
	}

	warn() {
		echo "[W] $1" 1>&2
	}
fi

#################### END OF FUNCTIONS ####################

## be sure we're running as root
test `whoami` = "root" || exec sudo $0 "$@"

info "GDP_ROOT=$GDP_ROOT"

## create "gdp" user
if ! grep -q "^${GDP_USER}:" /etc/passwd
then
	info "Creating user $GDP_USER"
	adduser --system --group $GDP_USER
fi

mkdir_gdp_opt $GDP_ROOT
cd $GDP_ROOT
umask 0022

## create system directories
if [ "$GDP_ROOT" != "/usr" ]
then
	ETC=$GDP_ROOT/etc
	mkdir_gdp_opt bin
	mkdir_gdp_opt log
	mkdir_gdp_opt etc
else
	ETC=/etc
fi
if [ "$GDP_ROOT" = "~${GDP_USER}" ]
then
	EP_PARAMS=$GDP_ROOT/.ep_adm_params
else
	EP_PARAMS=$ETC/ep_adm_params
fi

mkdir_gdp_opt $ETC
mkdir_gdp $EP_PARAMS
mkdir_gdp $GDP_LOGDIR
mkdir_gdp $GDPLOGD_DATADIR 0750

## set up default parameters
hostname=`hostname`

if [ ! -f $EP_PARAMS/gdp ]
then
	# determine default router set
	routers=`echo gdp-01 gdp-02 gdp-03 gdp-04 |
			tr ' ' '\n' |
			grep -v $hostname |
			shuf |
			tr '\n' ';' |
			sed -e 's/;/.eecs.berkeley.edu; /g' -e 's/; $//' `
	if echo $hostname | grep -q '^gdp-0'
	then
		routers="127.0.0.1; $routers"
	fi

	# create the parameters file
	info "Creating $EP_PARAMS/gdp"
	{
		echo "swarm.gdp.routers=$routers"
		echo "#libep.time.accuracy=0.5"
		echo "#libep.thr.mutex.type=errorcheck"
		echo "libep.dbg.file=stdout"
	} > $EP_PARAMS/gdp
	chown_gdp_opt $EP_PARAMS/gdp
	cat $EP_PARAMS/gdp
else
	warn "$EP_PARAMS/gdp already exists; check consistency" 1>&2
fi

if [ ! -f $EP_PARAMS/gdplogd ]
then
	info "Creating $EP_PARAMS/gdplogd"
	{
		echo "swarm.gdplogd.gdpname=edu.berkeley.eecs.$hostname.gdplogd"
		echo "swarm.gdplogd.gcl.dir=$GDPLOGD_DATADIR"
		echo "swarm.gdplogd.runasuser=gdp"
	} > $EP_PARAMS/gdplogd
	chown_gdp_opt $EP_PARAMS/gdplogd
	cat $EP_PARAMS/gdplogd
else
	warn "$EP_PARAMS/gdplogd already exists; check consistency" 1>&2
fi
