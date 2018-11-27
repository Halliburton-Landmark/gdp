#!/usr/bin/env sh

#
#  Initialize a container for running gdplogd.
#

set -e

if [ ! -z "$GDP_ROUTER" ]; then
	echo "swarm.gdp.routers=${GDP_ROUTER:=gdp-01.eecs.berkeley.edu; gdp-02.eecs.berkeley.edu}" \
		>> /etc/ep_adm_params/gdp
fi

# come up with unique values for this instance
if [ -z "$GDPLOGD_NAME" ]; then
	GDPLOGD_NAME=`hostname --fqdn | \
		tr '.' '\n' | \
		tac | \
		tr '\n' '.' | \
		sed -e 's/\.$//' -e 's/$/.gdplogd/'`
fi
cat >> /etc/ep_adm_params/gdplogd <<- EOF
	swarm.gdplogd.gdpname=$GDPLOGD_NAME
EOF

exec /usr/sbin/gdplogd-wrapper.sh
