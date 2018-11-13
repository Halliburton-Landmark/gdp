#!/usr/bin/env sh

#
#  Initialize a container for running gdplogd.
#

set -e

: ${GDPLOGD_NAME:=`hostname`}

# come up with unique values for this instance
cat >> /etc/ep_adm_params/gdplogd <<- EOF
	swarm.gdplogd.gdpname=$GDPLOGD_NAME.gdplogd
EOF

exec /usr/sbin/gdplogd-wrapper.sh
