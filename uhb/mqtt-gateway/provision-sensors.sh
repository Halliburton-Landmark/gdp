#!/bin/sh
{ test -r /usr/local/etc/gdp.conf.sh && . /usr/local/etc/gdp.conf.sh; } ||
	{ test -r /etc/gdp.conf.sh && . /etc/gdp.conf.sh; }

#
#  Create logs and adjust configuration files for new sensors.
#
#    Usage: $0 mqtt-gateway-config sensor ...
#	Verifies that each sensor is not already in the config file.
#	If not, creates the log file if it does not already exist
#	The configuration file must already exist, even if it has
#	no sensors listed.
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

EX_USAGE=64
EX_OSFILE=72
EX_CONFIG=78

# if we are running as root, start over as gdp
test `whoami` = "root" && exec sudo -u $GDP_USER $0 "$@"
if [ `whoami` != "$GDP_USER" ]
then
	echo "[FATAL] Must be run as $GDP_USER or root"
	exit $EX_USAGE
fi

if [ $# -lt 2 ]
then
	echo "[FATAL] Usage: $0 mqtt-gateway-config sensor ..."
	exit $EX_USAGE
fi

echo "[INFO] Running $0 with GDP_ROOT=$GDP_ROOT GDP_ETC=$GDP_ETC"

# get the short host name
shorthost=`basename "$1" ".conf" | sed -e 's/^mqtt-gateway.//' -e 's/\..*//'`
shift

# verify the configuration file
if [ ! -e $GDP_ETC/mqtt-gateway.$shorthost.conf ]
then
	echo "[FATAL] No configuration $GDP_ETC/mqtt-gateway.$shorthost.conf"
	exit $EX_CONFIG
fi

# save the names of the devices we want to provision
devices=$@

# get the existing configuration file and take the first line for the log name
configfile=$GDP_ETC/mqtt-gateway.$shorthost.conf
config=`sed -e 's/#.*//' $configfile`
set -- $config
gcl_root=$1
shift

# save the clean configuration (just existing devices, white space removed)
config="$@"

# now we can go back to devices in our arguments
set -- $devices

# if there is no keys directory, be sure to create it
if [ ! -d $GDP_KEYS_DIR ]
then
	if ! mkdir -p $GDP_KEYS_DIR
	then
		echo "[FATAL] Cannot create keys directory $GDP_KEYS_DIR"
		exit $EX_OSFILE
	fi
	chmod 755 $GDP_KEYS_DIR
fi

announce_keys_dir=false
for i
do
	echo "[INFO] Provisioning for sensor $i on gateway $shorthost"

	sensor=`echo $i | tr '[A-F]' '[a-f]' | sed -e 's/[^0-9a-f]//g'`
	if [ "$sensor" != "$i" ]
	then
		echo "[INFO] Modified $i to $sensor"
	fi

	# check for duplicates --- if so, just pass
	addconfig=true
	if echo $config | grep -q $sensor
	then
		addconfig=false
		echo "[WARN] Sensor $sensor already provisioned on $shorthost"
	else
		# check to see if it already exists in another configuration
		dups=`grep -l "^[ 	]*$sensor" $GDP_ETC/mqtt-gateway.*.conf`
		if [ ! -z "$dups" ]
		then
			echo "[ERROR] Sensor $sensor already provisioned in file"
			echo "[ERROR]    $dups"
			echo "[ERROR] Please edit this configuration and try again."
			continue
		fi
	fi

	# see if the log already exists --- if not, create
	if ! $GDP_ROOT/bin/log-exists $gcl_root.device.$sensor
	then
		echo "[INFO] Creating log $gcl_root.device.$sensor"
		if $GDP_ROOT/bin/gcl-create -K$GDP_KEYS_DIR -e none \
			$gcl_root.device.$sensor
		then
			echo "[INFO] Created log $gcl_root.device.$sensor"
			announce_keys_dir=true
		else
			echo "[ERROR] Could not create log; skipping $sensor"
		fi
	else
		echo "[WARN] $gcl_root.device.$sensor already exists;" \
				"continuing anyway"
	fi

	if $addconfig
	then
		# append device name to configuration file
		echo "[INFO] Adding configuration for $sensor to $configfile"
		echo $sensor >> $configfile
	fi
done

if $announce_keys_dir
then
	echo "[INFO] New private key(s) are in $GDP_KEYS_DIR"
fi
