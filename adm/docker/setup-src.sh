#!/usr/bin/env sh

#
#  Script to process the GDP source code into a docker container.
#

set -e

# throttle debconf errors
export DEBIAN_FRONTEND=noninteractive

args=`getopt D $*`
test $? != 0 && echo "Usage $0 [-D]" >&2 && exit 64
set -- $args
debug=false
while true
do
	$debug && echo "Parsing $*"
	case "$1" in
	  -D)
		debug=true
		;;
	  --)
		shift
		break;;
	esac
	shift
done

cd /src
ls -al gdp-src.tar
mkdir gdp
cd gdp
tar xf ../gdp-src.tar
#rm ../gdp-src.tar

# make sure adm/gdp-version.{sh,txt} are created
#cd gdp
#make gdp_version.h
make

#
#  Do setup for GDP clients.
#	Assumes several environment variables are set:
#	  GDP_ROUTER		The IP address of the GDP router
#	  GDP_HONGD_SERVER	The IP address of the Human-Oriented Name
#				to GDPname server
#	  GDP_CREATION_SERVICE	The GDPname of the creation service
#

PARAMS=/etc/gdp/params
mkdir -p $PARAMS
cat > $PARAMS/gdp <<- EOF
	swarm.gdp.routers=$GDP_ROUTER
	swarm.gdp.hongdb.host=$GDP_HONGD_SERVER
EOF
if ! test -z "$GDP_CREATION_SERVICE"; then
	echo "swarm.gdp.creation-service.name=$GDP_CREATION_SERVICE" >> $PARAMS/gdp
fi
