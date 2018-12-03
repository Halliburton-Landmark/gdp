#!/usr/bin/env sh

#
#  Initialize a container for running gdplogd.
#	This is run when a container is starting up.
#

set -e

if [ -z "$GDP_ROUTER" ]; then
	# assume use of Berkeley servers if nothing specified
	# (be sure to include semicolon at the end of each line)
	GDP_ROUTER=`cat <<- EOF
		gdp-01.eecs.berkeley.edu;
		gdp-02.eecs.berkeley.edu;
EOF
`
	# eliminate newlines...
	GDP_ROUTER=`echo $GDP_ROUTER`
fi
echo "swarm.gdp.routers=${GDP_ROUTER}" >> /etc/ep_adm_params/gdp

# come up with unique values for this instance
if [ -z "$GDPLOGD_NAME" ]; then
	# pick a unique name --- note: changes on every instantiation, so
	# not all that useful, but the alternative is worse.
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
