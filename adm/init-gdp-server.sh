#!/bin/sh
{ test -r /usr/local/etc/gdp.conf.sh && . /usr/local/etc/gdp.conf.sh; } ||
	{ test -r /etc/gdp.conf.sh && . /etc/gdp.conf.sh; }

#
#  Initialize GDP server hosts
#
#	This script should not be needed for ordinary clients.
#
#	This should be portable to all environments.  In particular,
#	it is not dependent on a particular version of Linux (or
#	for that matter, Linux at all).
#
#	It does **not** install system binaries.  Compiling and
#	installing should be done using "make install".
#
#	XXX  This assumes the Berkeley-based routers.  If you have
#	XXX  your own router you'll need to modify $EP_PARAMS/gdp
#	XXX  after this completes.
#
#	XXX  It also assumes you are in the eecs.berkeley.edu domain.
#	XXX  If you aren't you'll need to modify $EP_PARAMS/gdplogd
#	XXX  after this completes.
#
#	This script does not install a router or router
#	startup scripts.  The router is a separate package
#	and must be installed separately.
#

# we assume this is in the adm directory
cd `dirname $0`/..

# configure defaults
: ${GDP_ROOT:=/usr}
if [ "$GDP_ROOT" = "/usr" ]
then
	: ${GDP_ETC:=/etc/gdp}
	: ${EP_PARAMS:=/etc/ep_adm_params}
else
	: ${GDP_ETC:=$GDP_ROOT/etc}
fi
: ${GDP_LOG_DIR:=/var/log/gdp}
: ${GDP_USER:=gdp}
: ${GDP_GROUP:=$GDP_USER}
: ${GDP_VAR:=/var/swarm/gdp}
: ${GDP_KEYS_DIR:=$GDP_ETC/keys}
: ${GDPLOGD_DATADIR:=$GDP_VAR/gcls}

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
	info "Creating directory $1"
	mkdir -p $1
	chmod ${2:-0755} $1
	chown_gdp_opt $1
}

chown_gdp_opt() {
	#test "$GDP_ROOT" != "~${GDP_USER}" && return
	info "Giving $1 to user ${GDP_USER}:${GDP_GROUP}"
	chown ${GDP_USER}:${GDP_GROUP} $1
}

if [ -f adm/common-support.sh ]; then
	. adm/common-support.sh
else
	info() {
		echo "[INFO] $1" 1>&2
	}

	warn() {
		echo "[WARN] $1" 1>&2
	}
fi

#################### END OF FUNCTIONS ####################

if [ ! -x $GDP_ROOT/sbin/gdplogd ]
then
	warn "It appears GDP server code is not yet installed in $GDP_ROOT"
	info "Press <return> to continue, ^C to abort"
	read nothing
fi

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
	mkdir_gdp_opt bin
	mkdir_gdp_opt sbin
	mkdir_gdp_opt lib
	mkdir_gdp_opt log
	mkdir_gdp_opt etc
fi

# convert /etc/gdp/ep_adm_params => /etc/ep_adm_params
if [ `basename $GDP_ETC` = "gdp" ]
then
	EP_PARAMS=`dirname $GDP_ETC`/ep_adm_params
elif [ "$GDP_ROOT" = "~${GDP_USER}" ]
then
	EP_PARAMS=$GDP_ROOT/.ep_adm_params
else
	EP_PARAMS=$GDP_ETC/ep_adm_params
fi

mkdir_gdp_opt $GDP_ETC
mkdir_gdp $EP_PARAMS
mkdir_gdp $GDP_LOG_DIR
mkdir_gdp $GDP_VAR
mkdir_gdp $GDP_KEYS_DIR 0750
mkdir_gdp $GDPLOGD_DATADIR 0750

## set up default runtime administrative parameters
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
