#!/bin/sh
{ test -r /usr/local/etc/gdp.conf.sh && . /usr/local/etc/gdp.conf.sh } ||
	{ test -r /etc/gdp.conf.sh && . /etc/gdp.conf.sh }

#
#  Helper script for starting up an mqtt-gdp-gateway instance
#

# configure defaults
: ${GDP_ROOT:=/usr}
: ${GDP_VAR:=/var/swarm/gdp}
: ${GDP_KEYS:=$GDP_VAR/KEYS}
: ${GDP_LOG_DIR:=/var/log/gdp}
: ${GDP_USER:=gdp}
: ${MQTT_GATEWAY_ARGS:=-D*=2}
: ${MQTT_GATEWAY_LOG:=$GDP_LOG_DIR/mqtt-gateway.log}
if [ "$GDP_ROOT" = "/usr" ]
then
	: ${GDP_ETC:=/etc}
else
	: ${GDP_ETC:=$GDP_ROOT/etc}
fi

# if we are running as root, start over as gdp
test `whoami` = "root" && exec sudo -u $GDP_USER $0 "$@"

fatal() {
	echo "[FATAL] $1"
	exit 1;
}

info() {
	echo "[INFO] $1"
}

EX_USAGE=64
EX_CONFIG=78

{
	echo `date +"%F %T %z"` Running $0 "$@" as `whoami`
	if [ `whoami` != $GDP_USER ]
	then
		echo "Warning: should be running as $GDP_USER"
	fi
	if [ $# -ne 2 ]
	then
		echo "Usage: $0 mqtt-broker-host logname-root"
		echo "Usage: $0 mqtt-broker-host logname-root" 1>&2
		exit $EX_USAGE
	fi

	mqtt_host=$1
	gcl_root=$2

	hostname=`echo $mqtt_host | sed 's/\..*//'`
	if [ ! -e $GDP_ETC/mqtt-devices.$hostname ]
	then
		echo "Fatal: no configuration $GDP_ETC/mqtt-devices.$hostname"
		exit $EX_CONFIG
	fi

	#echo "Running $0 with GDP_ROOT=$GDP_ROOT"
	#echo "Using MQTT server at $mqtt_host"
	#echo "Using log names $gcl_root.*"

	args="-s -M $mqtt_host -d -K$GDP_KEYS $MQTT_GATEWAY_ARGS"
	gw_prog="$GDP_ROOT/bin/mqtt-gdp-gateway"
	devices=`cat $GDP_ETC/mqtt-devices.$hostname | sed -e 's/#.*//'`

	for i in $devices
	do
		if $GDP_ROOT/bin/gcl-create \
			-q -K$GDP_KEYS -e none $gcl_root.device.$i
		then
			echo "Created log $gcl_root.device.$i"
		fi
		args="$args device/+/$i $gcl_root.device.$i"
	done

	exec $gw_prog $args
} >> $MQTT_GATEWAY_LOG 2>&1
