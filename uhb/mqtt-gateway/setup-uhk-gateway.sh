#!/bin/sh

#
#  Set up the U Michigan gateway code on a Swarm box (BBB or Ubuntu)
#
#	Right now this _must_ be run in ~debian (the home directory
#		for user "debian".  The start-up scripts assume the
#		code is there.  No, this is not best practice.
#	Does _not_ include the MQTT-GDP gateway code, which will
#		often run on another host.
#

. `dirname $0`/setup-common.sh

test `whoami` = "debian" || fatal "$0 must be run as debian"
cd ~debian
root=`pwd`

# heuristic to see if we are running on a beaglebone
beaglebone=false
test `uname -m` = "arm7l" && beaglebone=true

if $beaglebone
then
	# beaglebone, not much disk space
	gitdepth="--depth 1"
else
	gitdepth=""
fi

echo ""
info "Determining OS version"

case $OS-$OSVER in
	ubuntu-1[46]04*)
		pkgadd="libmosquitto0-dev mosquitto-clients"
		;;

	debian-*)
		pkgadd="libmosquitto-dev mosquitto-clients"
		;;

	*)
		warn "Unknown OS or Version $OS-$OSVER; guessing"
		pkgadd="libmosquitto-dev mosquitto-clients"
		;;
esac

echo 	""
info "Installing Debian packages"
test ! -d /var/lib/bluetooth &&
	mkdir /var/lib/bluetooth &&
	chmod 700 /var/lib/bluetooth
sudo apt-get install -y \
	avahi-daemon \
	bluetooth \
	bluez \
	curl \
	g++ \
	gcc \
	git \
	libavahi-compat-libdnssd-dev \
	libbluetooth-dev \
	libudev-dev \
	locales \
	make \
	psmisc \
	$pkgadd

info "Enabling bluetooth daemon"
sudo update-rc.d bluetooth defaults

# check out the git tree from UMich
echo ""
info "Checking out Gateway source tree from Michigan"
cd $root
rm -rf gateway
git clone $gitdepth https://github.com/lab11/gateway.git
cd gateway

# verify that we have checked things out
if [ ! -d software -o ! -d systemd ]
then
	fatal "$0 must be run from root of gateway git tree" 1>&2
fi

echo ""
info "Set up node.js"
info ">>> NOTE WELL: this may give several warnings about xpc-connection."
info ">>> These should be ignored."
curl -sL https://deb.nodesource.com/setup_5.x | sudo -E bash -
sudo apt-get install -y nodejs

# get the names of the packages that might run
cd systemd
pkgs=`ls -d *.service | sed 's/\.service//'`

# initialize remaining dependencies for each service
#	We also install the packages themselves even though they are
#	run out of the source tree; doing npm install in each source
#	directory causes duplicate dependencies, and our disks are
#	just too small.
cd $root/gateway/software
for i in $pkgs
do
	echo ""
	info "Initializing for package $i"
	npm install $i
done

echo ""
info "Clearing NPM cache"
npm cache clean

# install system startup scripts
if [ "$INITSYS" != "systemd" ]
then
	fatal "Cannot initialize system startup scripts: only systemd supported"
fi

cd $root/gateway/systemd
echo ""
info "Installing system startup scripts"
sudo cp *.service /etc/systemd/system

echo ""
info "Selectively enabling system startup scripts"

enable() {
	info "Enabling service $1"
	sudo systemctl enable $1
}

skip() {
	if [ -z "$2" ]
	then
		info "Skipping service $1"
	else
		info "Skipping service $1: $2"
	fi
}

enable	adv-gateway-ip
enable	ble-address-sniffer-mqtt
enable	ble-gateway-mqtt
skip	ble-nearby
skip	gateway-mqtt-emoncms	"not in use at Berkeley"
skip	gateway-mqtt-gatd	"not in use at Berkeley"
skip	gateway-mqtt-log	"not in use at Berkeley"
enable	gateway-mqtt-topics
enable	gateway-publish
enable	gateway-server
skip	gateway-ssdp
skip	gateway-watchdog-email
skip	ieee802154-monjolo-gateway
