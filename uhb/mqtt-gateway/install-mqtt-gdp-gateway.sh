#!/bin/sh

#
#  Install mqtt-gdp-gateway code and system configuration scripts.
#
#	Only tested on certain Debian-derived systems (including
#		Ubuntu).
#	Sorry, will not work on MacOS, FreeBSD, or RedHat.
#	Uses heuristics to try to figure out system initialization
#		mechanism.  If it guesses wrong, you can set
#		the envariable INITSYS to "upstart" or "systemd"
#		before running this script.
#

# need to run from the mqtt-gdp-gateway source directory
cd `dirname $0`
. ./setup-common.sh


# Installation program
: ${INSTALL:=install}

if [ `uname -s` != Linux ]
then
	fatal "Only works on (some) Linux systems"
	exit 1
fi

info "Installing with GDP_ROOT=$GDP_ROOT, GDP_ETC=$GDP_ETC, INITSYS=$INITSYS"

# create GDP user if necessary
if ! grep -q "^${GDP_USER}:" /etc/passwd
then
	info "Creating new gdp user"
	sudo adduser --system --group ${GDP_USER}
fi

# make system directories if needed
mkdir_gdp $GDP_ROOT
for d in bin sbin etc lib
do
	mkdir_gdp $GDP_ROOT/$d
done
mkdir_gdp $GDP_ETC
mkdir_gdp $GDP_LOG_DIR
if [ ! -f $GDP_LOG_DIR/mqtt-gdp-gateway.log ]
then
	sudo cp /dev/null $GDP_LOG_DIR/mqtt-gdp-gateway.log
	sudo chown ${GDP_USER}:${GDP_GROUP} $GDP_LOG_DIR/mqtt-gdp-gateway.log
fi

info "Installing mqtt-gdp-gateway program and documentation"
make mqtt-gdp-gateway
sudo $INSTALL -o $GDP_USER -g $GDP_GROUP mqtt-gdp-gateway $GDP_ROOT/bin
manroot=${GDP_ROOT}/share/man
test -d $manroot || manroot=$GDP_ROOT/man
sudo $INSTALL -o $GDP_USER -g $GDP_GROUP mqtt-gdp-gateway.1 $manroot/man1

info "Installing mqtt-gdp-gateway startup scripts"
sudo $INSTALL -o $GDP_USER -g $GDP_GROUP start-mqtt-gdp-gateway.sh $GDP_ROOT/sbin

info "Installing MQTT gateway configuration"
sudo cp -iv mqtt-gateway.*.conf $GDP_ETC
sudo chown ${GDP_USER}:${GDP_GROUP} $GDP_ETC/mqtt-gateway.*.conf

if [ "$INITSYS" = "upstart" ]
then
	info "Installing Upstart system startup configuration"
	sudo sh $GDP_SRC_ROOT/adm/customize.sh mqtt-gdp-gateway.conf /etc/init
	sudo sh $GDP_SRC_ROOT/adm/customize.sh mqtt-gdp-gateways.conf /etc/init
	sudo initctl check-config --system mqtt-gdp-gateway
	sudo initctl check-config --system mqtt-gdp-gateways
	if [ ! -e /etc/default/mqtt-gdp-gateway ]
	then
		MQTT_SERVERS=`sed 's/#.*//' <<- 'EOF'
			localhost
EOF
	`
		sudo dd of=/etc/default/mqtt-gdp-gateway <<- EOF
		# list of MQTT servers to monitor (use # to comment out lines)
		MQTT_SERVERS=$MQTT_SERVERS

		# root of GDP log name; the device name will be appended
		MQTT_LOG_ROOT="edu.berkeley.eecs.swarmlab.device"
EOF
		warn "Edit /etc/default/mqtt-gdp-gateway to define"
		warn "    MQTT_SERVERS and MQTT_LOG_ROOT"
	fi
elif [ "$INITSYS" = "systemd" ]
then
	info "Installing systemd startup configuration"
	sudo sh $GDP_SRC_ROOT/adm/customize.sh mqtt-gdp-gateway.service /etc/systemd/system
	sudo sh $GDP_SRC_ROOT/adm/customize.sh mqtt-gdp-gateway@.service /etc/systemd/system
	sudo systemctl daemon-reload
	sudo systemctl enable mqtt-gdp-gateway
	warn "To enable logging from MQTT brokers, use:"
	warn "    systemctl enable mqtt-gdp-gateway@host.dom.ain"
	warn "for all MQTT brokers"
else
	warn "Unknown init system; no system startup installed"
fi
