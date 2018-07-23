#!/usr/bin/env sh

set -e

## These are our configuration parameters.
## FIXME: Even though we run `gdplogd2`, it reads the configuration
## parameters from `gdplogd`.. at least as of Jul 20, 2018

echo "swarm.gdp.zeroconf.enable=false" >> /tmp/gdplogd

echo "swarm.gdp.routers=$GDP_ROUTER" >> /tmp/gdplogd
echo "swarm.gdplogd.gdpname=$GDPLOGD_NAME" >> /tmp/gdplogd

echo "swarm.gdplogd.log.dir=/var/swarm/gdp/glogs" >> /tmp/gdplogd
mkdir -p /var/swarm/gdp/glogs

echo "swarm.gdplogd.sqlite.pragma.synchronous=OFF" >> /tmp/gdplogd
echo "swarm.gdplogd.sqlite.pragma.journal_mode=OFF" >> /tmp/gdplogd

mkdir -p /etc/ep_adm_params
mv /tmp/gdplogd /etc/ep_adm_params

