#!/usr/bin/env sh

#
#  Script to fetch the GDP source code.
#

set -e

# throttle debconf errors
export DEBIAN_FRONTEND=noninteractive

args=`getopt b:Dr:v: $*`
test $? != 0 && echo "Usage $0 [-b branch] [-r repo] [-v ver]" >&2 && exit 64
set -- $args
debug=false
while true
do
	$debug && echo "Parsing $*"
	case "$1" in
	  -b)
		BRANCH=$2
		shift
		;;
	  -D)
		debug=true
		;;
	  -r)
		REPO=$2
		shift
		;;
	  -v)
		VER=$2
		shift
		;;
	  --)
		shift
		break;;
	esac
	shift
done

: ${VER:=latest}
test "$BRANCH" = "default" && BRANCH="r$VER"
test "$BRANCH" = "rlatest" && BRANCH=master
test "$REPO" = "default" && REPO="git://repo.eecs.berkeley.edu/projects/swarmlab/gdp.git"

if $debug; then
	echo VER=$VER BRANCH=$BRANCH REPO=$REPO
	echo args=$*
	exit 0
fi


# ideally this would leverage adm/gdp-setup.sh to avoid duplication
PACKAGES=`sed -e 's/*.*//' << 'EOF'
	apt-utils
	git
	libavahi-client-dev
	libevent-dev
	libevent-pthreads-2.0.5
	libjansson-dev
	libmariadb-client-lgpl-dev
	libsqlite3-dev
	libssl-dev
	libsystemd-dev
	protobuf-c-compiler
	sudo
	uuid-dev
EOF
`

echo "Compiling and installing gdp-dev-c from" `pwd`
apt-get update
apt-get install -y $PACKAGES

git clone --depth=1 -b ${BRANCH} ${REPO}
cd gdp && make


#
#  Do setup for GDP clients.
#	Assumes several environment variables are set:
#	  GDP_ROUTER		The IP address of the GDP router
#	  GDP_HONGD_SERVER	The IP address of the Human-Oriented Name
#				to GDPname server
#	  GDP_CREATION_SERVICE	The GDPname of the creation service
#

set -e

PARAMS=/etc/ep_adm_params
mkdir -p $PARAMS
cat > $PARAMS/gdp <<- EOF
	swarm.gdp.routers=$GDP_ROUTER
	swarm.gdp.hongdb.host=$GDP_HONGD_SERVER
EOF
if ! test -z "$GDP_CREATION_SERVICE"; then
	echo "swarm.gdp.create.service=$GDP_CREATION_SERVICE" >> $PARAMS/gdp
fi
