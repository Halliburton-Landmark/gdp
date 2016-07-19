#!/bin/sh

#
#  Set up MQTT-GDP gateway requirements
#
#	This does not include the Michigan code, which will often run
#		on another host.
#	This only runs on Debian-based systems.  It should really be
#		more portable.
#

cd `dirname $0`
uhkroot=`pwd`

. $uhkroot/setup-common.sh

echo 	""
info "Installing Debian packages needed for MQTT-GDP gateway"
sudo apt-get install -y \
	libmosquitto-dev \
	locales \
	make \
	mosquitto-clients \
	psmisc \

echo ""
info "Compiling and installing UHK code (assumes GDP base code already installed)"
make clean all
sudo make install

# start mosquitto
# foreach server: create list of devices
