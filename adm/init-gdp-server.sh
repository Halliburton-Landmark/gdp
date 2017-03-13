#!/bin/sh

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
GDP_SRC_ROOT=`pwd`
. adm/common-support.sh

## compile utilities, possibly not as root
(cd util && make)

: ${GDPLOGD_LOG:=$GDP_LOG_DIR/gdplogd.log}

if [ ! -x $GDP_ROOT/sbin/gdplogd -o ! -x $GDP_ROOT/sbin/gdp-rest ]
then
	warn "It appears GDP server code (gdplogd and gdp-rest) are not yet"
	warn "installed in $GDP_ROOT/sbin.  These should be installed by"
	warn "\"sudo make install\""
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

mkdir_gdp $GDP_ROOT
cd $GDP_ROOT
umask 0022

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
mkdir_gdp $GDP_LOG_DIR
mkdir_gdp $GDP_VAR
mkdir_gdp $GDP_KEYS_DIR 0750
mkdir_gdp $GDPLOGD_DATADIR 0750

mkfile_gdp $GDPLOGD_LOG
mkfile_gdp $GDP_REST_LOG

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
	chown ${GDP_USER}:${GDP_GROUP} $EP_PARAMS/gdp
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
	chown ${GDP_USER}:${GDP_GROUP} $EP_PARAMS/gdplogd
	cat $EP_PARAMS/gdplogd
else
	warn "$EP_PARAMS/gdplogd already exists; check consistency" 1>&2
fi

info "Installing utility programs"
cd $GDP_SRC_ROOT
(cd util && make install)

info "Installing gdplogd wrapper script"
install -o ${GDP_USER} adm/gdplogd-wrapper.sh $GDP_ROOT/sbin

info "Installing gdp-rest wrapper script"
install -o ${GDP_USER} adm/gdp-rest-wrapper.sh $GDP_ROOT/sbin

if [ -d /etc/rsyslog.d ]
then
	info "Installing rsyslog configuration"
	sh adm/customize.sh adm/60-gdp.conf.template /etc/rsyslog.d
	chown ${GDP_USER}:${GDP_GROUP} /etc/rsyslog.d/60-gdp.conf
fi

if [ -d /etc/logrotate.d ]
then
	info "Installing logrotate configuration"
	cp adm/gdp-logrotate.conf /etc/logrotate.d/gdp
fi

if [ "$INITSYS" = "systemd" ]
then
	adm/customize.sh adm/gdplogd.service.template /etc/systemd/system
	adm/customize.sh adm/gdp-rest.service.template /etc/systemd/system
	systemctl daemon-reload
	systemctl enable gdplogd
	systemctl enable gdp-rest
else
	warn "No system initialization configured"
fi
