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
TMP=/tmp
. adm/common-support.sh

## compile code and utilities, possibly not as root
make
(cd util && make)

. adm/gdp-version.sh
: ${GDP_VER=$GDP_VERSION_MAJOR}
: ${GDPLOGD_LOG:=$GDP_LOG_DIR/gdplogd.log}
: ${GDPLOGD_BIN:=$GDP_ROOT/sbin/gdplogd$GDP_VER}
: ${GDP_REST_INSTALL:=false}
export GDP_VER

if [ -z "$GDP_VER" -a ! -x $GDPLOGD_BIN ]
then
	warn "It appears the GDP log server (gdplogd) is not yet"
	warn "installed in $GDP_ROOT/sbin.  It should be installed by"
	warn "\"sudo make install\""
	info "Press <return> to continue, ^C to abort"
	read nothing
fi

## be sure we're running as root
test `whoami` = "root" || exec sudo GDP_REST_INSTALL=$GDP_REST_INSTALL $0 "$@"

info "GDP_ROOT=$GDP_ROOT"
if [ ! -z "$GDP_VER" ]
then
	warn "Installing $GDPLOGD_BIN but not associated documentation."
	cp gdplogd/gdplogd $GDPLOGD_BIN
fi

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
elif cmp $TMP/gdp.params $EP_PARAMS/gdp
then
	rm $TMP/gdp.params
else
	warn "$EP_PARAMS/gdp already exists; check consistency" 1>&2
	diff -u $TMP/gdp.params $EP_PARAMS/gdp
fi

info "Creating $EP_PARAMS/gdplogd"
{
	echo "swarm.gdplogd.gdpname=edu.berkeley.eecs.$hostname.gdplogd"
	echo "swarm.gdplogd.runasuser=gdp"
} > $TMP/gdplogd.params
if [ ! -f $EP_PARAMS/gdplogd ]
then
	cp $TMP/gdplogd.params $EP_PARAMS/gdplogd
	chown ${GDP_USER}:${GDP_GROUP} $EP_PARAMS/gdplogd
	cat $EP_PARAMS/gdplogd
elif cmp $TMP/gdplogd.params $EP_PARAMS/gdplogd
then
	rm $TMP/gdplogd.params
else
	warn "$EP_PARAMS/gdplogd already exists; check consistency" 1>&2
	diff -u $TMP/gdplogd.params $EP_PARAMS/gdplogd
fi

info "Installing utility programs"
cd $GDP_SRC_ROOT
(cd util && make install)

info "Installing gdplogd wrapper script"
install -o ${GDP_USER} adm/gdplogd-wrapper.sh $GDP_ROOT/sbin

if $GDP_REST_INSTALL
then
	info "Installing gdp-rest wrapper script"
	install -o ${GDP_USER} adm/gdp-rest-wrapper.sh $GDP_ROOT/sbin
fi

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
	info "Installing and enabling systemd service files"
	info "gdplogd.service ..."
	adm/customize.sh adm/gdplogd.service.template $TMP
	cp $TMP/gdplogd.service /etc/systemd/system/gdplogd$GDP_VER.service
	rm $TMP/gdplogd.service
	if $GDP_REST_INSTALL
	then
		info "gdp-rest.service ..."
		adm/customize.sh adm/gdp-rest.service.template /etc/systemd/system
	fi
	systemctl daemon-reload
	systemctl enable gdplogd$GDP_VER
	if $GDP_REST_INSTALL
	then
		systemctl enable gdp-rest
		warn "Startup scripts for gdp-rest are installed, but you will"
		warn "need to configure a web server to use the SCGI interface."
		warn "See README-CAAPI.md for advice."
	fi
else
	warn "No system initialization configured"
fi
