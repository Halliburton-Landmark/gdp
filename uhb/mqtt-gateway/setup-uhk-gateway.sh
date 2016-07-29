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

node_setup=true
if [ "x$1" = "x-n" ]
then
	node_setup=false
	shift
fi

nodevers=6

# heuristic to see if we are running on a beaglebone
beaglebone=false
test `uname -m` = "arm7l" && beaglebone=true

instdir=$1
if [ ! -z "$instdir" ]
then
	# don't need to do anything yet
	:
elif [ `whoami` = "debian" ]
then
	cd ~debian
	instdir=`pwd`/gateway/software
elif $beaglebone
then
	fatal "$0 must be run as debian on beaglebone"
else
	warn "You may need to adjust paths in startup scripts"
	instdir=/usr/lib/uhk
fi

root=`pwd`
info "Installing Urban Heartbeat Kit from $root into $instdir"

if $beaglebone
then
	# beaglebone, not much disk space
	gitdepth="--depth 1"
else
	gitdepth=""
fi

if ! grep -q "^mosquitto:" /etc/passwd
then
	info "Creating user mosquitto"
	adduser --system mosquitto
fi

echo ""
info "Determining OS version"

case $OS-$OSVER in
	ubuntu-1404*)
		pkgadd="libmosquitto0-dev mosquitto-clients"
		;;

	debian-*|ubuntu-1604*)
		pkgadd="libmosquitto-dev mosquitto-clients"
		;;

	*)
		warn "Unknown OS or Version $OS-$OSVER; guessing"
		pkgadd="libmosquitto-dev mosquitto-clients"
		;;
esac

echo ""
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
	mosquitto \
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

if $node_setup
then
	echo ""
	info "Set up node.js"
	info ">>> NOTE WELL: this may give several warnings about xpc-connection."
	info ">>> These should be ignored."
	curl -sL https://deb.nodesource.com/setup_$nodevers.x | sudo -E bash -
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
	umask 022
	if [ `pwd` != "$instdir" ]
	then
		if [ ! -d "$instdir" ]
		then
			info "Creating $instdir"
			sudo mkdir -p $instdir && sudo chown `whoami` $instdir
		fi
		cp -rp [a-z]* $instdir
		cd $instdir
		mkdir node_modules
	fi
	for i in $pkgs
	do
		echo ""
		info "Initializing for package $i"
		npm install $i --prefix $instdir
	done

	echo ""
	info "Clearing NPM cache"
	npm cache clean
fi

# install system startup scripts
if [ "$INITSYS" != "systemd" ]
then
	fatal "Cannot initialize system startup scripts: only systemd supported"
fi

echo ""
info "Installing system startup scripts"
info "  ... mosquitto.service"
sudo cp $root/mosquitto.service /etc/systemd/system
cd $root/gateway/systemd
for i in *.service
do
	info "  ... $i"
	sed "s;/home/debian/gateway/software/;$instdir/;" $i |
		sudo dd of=/etc/systemd/system/$i
done

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

enable	mosquitto
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
