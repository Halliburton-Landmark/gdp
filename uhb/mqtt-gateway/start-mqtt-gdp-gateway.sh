#!/bin/sh

#
#  Helper script for starting up an mqtt-gdp-gateway instance
#
#	Normally this would exist in $GDP_ROOT/adm, where
#	$GDP_ROOT is either /usr/gdp or /usr/local/gdp.
#	The value of $GDP_ROOT is derived from $0.

# if we are running as root, start over as gdp
test `whoami` != "root" || exec sudo -u gdp $0 "$@"

cd `dirname $0`/..
root=`pwd`
: ${GDP_ROOT:=$root}
. adm/common-support.sh

if [ $# -lt 3 ]
then
	fatal "Usage: $0 mqtt-broker logname-root devname..."
fi

mqtt_host=$1
groot=$2
shift;shift

info "Running $0 in $root with GDP_ROOT=$GDP_ROOT"
info "Using MQTT server at $mqtt_host"
info "Using log names $groot.*"

args="-D *=2 -s -M $mqtt_host -d"
gw_prog="mqtt-gdp-gateway"

for i
do
	info "Running gcl-create -q -e none $groot.device.$i"
	if gcl-create -q -e none $groot.device.$i
	then
		info "Created log $groot.device.$i"
	fi
	args="$args device/+/$i $groot.device.$i"
done

info "running $gw_prog $args"
exec $gw_prog $args
