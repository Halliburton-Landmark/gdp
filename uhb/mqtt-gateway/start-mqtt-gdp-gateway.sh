#!/bin/sh
{ test -r /usr/local/etc/gdp.conf.sh && . /usr/local/etc/gdp.conf.sh; } ||
	{ test -r /etc/gdp.conf.sh && . /etc/gdp.conf.sh; }

#
#  Helper script for starting up an mqtt-gdp-gateway instance
#	This is normally started by systemd as an instance.
#
#    Usage: $0 mqtt-broker-host
#	mqtt-broker-host should be the full DNS name of the host
#	running the MQTT broker.  The hostname part of that is used
#	to look up a configuration file in $GDP_ETC that lists the
#	sensors of interest connected to that broker.  Logs for all
#	the sensors must be created in advance.
#

# configure defaults
: ${GDP_ROOT:=/usr}
if [ "$GDP_ROOT" = "/usr" ]
then
	: ${GDP_ETC:=/etc/gdp}
else
	: ${GDP_ETC:=$GDP_ROOT/etc}
fi
: ${GDP_VAR:=/var/swarm/gdp}
: ${GDP_KEYS_DIR:=$GDP_ETC/keys}
: ${GDP_LOG_DIR:=/var/log/gdp}
: ${GDP_USER:=gdp}
: ${MQTT_GATEWAY_ARGS:=-D*=2}
: ${MQTT_GATEWAY_LOG:=$GDP_LOG_DIR/mqtt-gdp-gateway.log}
: ${LLOGGER:=llogger}

# if we are running as root, start over as gdp
test `whoami` = "root" && exec sudo -u $GDP_USER $0 "$@"

EX_USAGE=64
EX_CONFIG=78

# make sure log file exists so we can append to it
test -f $MQTT_GATEWAY_LOG || cp /dev/null $MQTT_GATEWAY_LOG

{
	echo `date +"%F %T %z"` Running $0 "$@" as `whoami`
	if [ `whoami` != $GDP_USER ]
	then
		echo "[WARN] Should be running as $GDP_USER"
	fi
	if [ $# -ne 1 ]
	then
		echo "[ERROR] Usage: $0 mqtt-broker-host"
		echo "[ERROR] Usage: $0 mqtt-broker-host" 1>&2
		exit $EX_USAGE
	fi

	mqtt_host=$1

	shorthost=`echo $mqtt_host | sed 's/\..*//'`
	if [ ! -e $GDP_ETC/mqtt-gateway.$shorthost.conf ]
	then
		echo "[FATAL] No configuration $GDP_ETC/mqtt-gateway.$shorthost.conf"
		exit $EX_CONFIG
	fi

	args=""
	gw_prog="$GDP_ROOT/bin/mqtt-gdp-gateway"
	devices=`sed -e 's/#.*//' $GDP_ETC/mqtt-gateway.$shorthost.conf`
	set -- $devices
	gcl_root=$1
	shift

	echo "[INFO] Running $0 with GDP_ROOT=$GDP_ROOT"
	echo "[INFO] Using MQTT server at $mqtt_host"
	echo "[INFO] Using log names $gcl_root.*"

	for i
	do
		if $GDP_ROOT/bin/log-exists $gcl_root.device.$i
		then
			args="$args device/+/$i $gcl_root.device.$i"
		else
			echo "[ERROR] Log $gcl_root.device.$i does not exist!"
			echo "[ERROR] This may be because a log server" \
				"is down or inaccessible"
			echo "[ERROR] If you are sure that is not the case," \
				"create a new log using:"
			echo "        $GDP_ROOT/bin/log-create -q -K$GDP_KEYS_DIR" \
				" -e none $gcl_root.device.$i"
		fi
	done

	if [ ! -z "$args" ]
	then
		args="-s -M $mqtt_host -d -K$GDP_KEYS_DIR $MQTT_GATEWAY_ARGS $args"
		echo "[INFO] Running $gw_prog $args"
		exec $gw_prog $args
	else
		echo "[INFO] $0: no valid logs"
	fi
} 2>&1 | ${LLOGGER} $MQTT_GATEWAY_LOG
