#!/usr/bin/env sh

#
#  Set up gdplogd configuration and database
#

set -e

# database directory inside container
: ${GDP_LOG_ROOT:=/var/swarm/gdp/glogs}
mkdir -p $GDP_LOG_ROOT
chown gdp:gdp $GDP_LOG_ROOT
chmod 750 $GDP_LOG_ROOT

# system log files inside container
SYSLOGDIR=/var/log/gdp
mkdir -p $SYSLOGDIR
chown gdp:gdp $SYSLOGDIR

mkdir -p /etc/ep_adm_params
cat > /etc/ep_adm_params/gdplogd <<- EOF
	swarm.gdp.zeroconf.enable=false
	swarm.gdp.routers=$GDP_ROUTER
	swarm.gdplogd.log.dir=$GDP_LOG_ROOT
EOF
chown gdp:gdp /etc/ep_adm_params/gdplogd
