#!/bin/sh

#
#  Common code for setup scripts
#

# yes, we really need the adm directory (for customize.sh).
# can override search by setting GDP_SRC_ROOT.
if [ -z "${GDP_SRC_ROOT-}" ]
then
	gdp=`pwd`
	while [ ! -d $gdp/gdp/adm ]
	do
		gdp=`echo $gdp | sed -e 's,/[^/]*$,,'`
		if [ -z "$gdp" ]
		then
			echo "[FATAL] Need gdp/adm directory somewhere in directory tree"
			exit 1
		fi
	done
	GDP_SRC_ROOT=$gdp/gdp
fi
. $GDP_SRC_ROOT/adm/common-support.sh

# if we use apt, try to set up the mosquitto repository
if { [ "$OS" = "ubuntu" -o "$OS" = "debian" ]; } &&
     ! ls /etc/apt/sources.list.d/mosquitto* > /dev/null 2>&1
then
	sudo apt-get update
	echo ""
	info "Setting up mosquitto repository"
	if [ "$OS" = "ubuntu" ]
	then
		sudo apt-get install -y \
			software-properties-common \
			python-software-properties
		sudo apt-add-repository -y ppa:mosquitto-dev/mosquitto-ppa
	elif [ "$OS" = "debian" ]
	then
		sudo apt-get install -y wget
		wget http://repo.mosquitto.org/debian/mosquitto-repo.gpg.key
		sudo apt-key add mosquitto-repo.gpg.key
		if [ "$OSVER" = "080000" ]
		then
			dver="jessie"
		else
			fatal "unknown debian version $OSVER"
		fi
		cd /etc/apt/sources.list.d
		sudo wget http://repo.mosquitto.org/debian/mosquitto-$dver.list
		cd $root
	else
		fatal "Unknown linux distribution $OS"
	fi
fi
