#!/usr/bin/env sh

#
#  Set up gdplogd configuration and database
#	This is run when the image is built.
#

set -e

# database directory inside container
: ${GDP_DATA_ROOT:=/var/swarm/gdp}
mkdir -p $GDP_DATA_ROOT/glogs
chown gdp:gdp $GDP_DATA_ROOT $GDP_DATA_ROOT/glogs
chmod 750 $GDP_DATA_ROOT $GDP_DATA_ROOT/glogs

# system log files inside container
#	Arguably, these should be in the host filesystem tree, since
#		logs are useful across restarts; also, I suspect chatty
#		logs chew up memory.
SYSLOGDIR=/var/log/gdp
mkdir -p $SYSLOGDIR
chown gdp:gdp $SYSLOGDIR

# set up default parameters
mkdir -p /etc/ep_adm_params
cat > /etc/ep_adm_params/gdp <<- EOF
	swarm.gdp.data.root=$GDP_DATA_ROOT
EOF
cat > /etc/ep_adm_params/gdplogd <<- EOF
	swarm.gdp.zeroconf.enable=false
EOF
chown gdp:gdp /etc/ep_adm_params/*
