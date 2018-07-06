#!/bin/sh

#
#  Set up system for binary installs.
#
#	Create gdp user, needed directories, etc.
#	Does not set up for compilation; for that use adm/gdp-setup.sh.
#

# we assume this is in the adm directory
cd `dirname $0`/..
TMP=/tmp
. adm/common-support.sh
. adm/gdp-version.sh
: ${GDP_VER=$GDP_VERSION_MAJOR}
: ${GDPLOGD_LOG:=$GDP_LOG_DIR/gdplogd.log}
: ${GDPLOGD_BIN:=$GDP_ROOT/sbin/gdplogd$GDP_VER}
export GDP_VER

## be sure we're running as root
test `whoami` = "root" || exec sudo $0 "$@"

info "Preparing install into GDP_ROOT=$GDP_ROOT"

## create "gdp" group
if ! grep -q "^${GDP_GROUP}:" /etc/group
then
	info "Creating group $GDP_GROUP"
	addgroup --system $GDP_GROUP
fi

## create "gdp" user
if ! grep -q "^${GDP_USER}:" /etc/passwd
then
	info "Creating user $GDP_USER"
	adduser --system --ingroup $GDP_GROUP $GDP_USER
fi

umask 0022

# make the root directory
mkdir_gdp $GDP_ROOT
cd $GDP_ROOT

## create system directories
if [ "$GDP_ROOT" != "/usr" ]
then
	mkdir_gdp bin
	mkdir_gdp sbin
	mkdir_gdp lib
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

mkdir_gdp $GDP_ETC
mkdir_gdp $EP_PARAMS
mkdir_gdp $GDP_KEYS_DIR 0750

## set up default runtime administrative parameters
hostname=`hostname`

# determine default router set --- customized for Berkeley servers!!!
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

info "Creating $EP_PARAMS/gdp"
{
	echo "swarm.gdp.routers=$routers"
	echo "#libep.time.accuracy=0.5"
	echo "#libep.thr.mutex.type=errorcheck"
	echo "libep.dbg.file=stdout"
} > $TMP/gdp.params
if [ ! -f $EP_PARAMS/gdp ]
then
	cp $TMP/gdp.params $EP_PARAMS/gdp
	chown ${GDP_USER}:${GDP_GROUP} $EP_PARAMS/gdp
	cat $EP_PARAMS/gdp
elif cmp -s $TMP/gdp.params $EP_PARAMS/gdp
then
	rm $TMP/gdp.params
else
	warn "$EP_PARAMS/gdp already exists; check consistency" 1>&2
	diff -u $TMP/gdp.params $EP_PARAMS/gdp
fi
