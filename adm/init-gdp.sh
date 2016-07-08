#!/bin/sh

#
#  Initialize server hosts
#	This is specialized for Berkeley
#

if [ `whoami` != 'gdp' ]
then
	echo "This script must run as user gdp"
	if ! grep -q '^gdp:' /etc/passwd
	then
		echo "I will create the user for you"
		sudo adduser --system --group gdp
	fi
	test `whoami` = "root" && exec $0 "$@"
	echo "Please try again"
	exit 1
fi

cd ~gdp
home=`pwd`
umask 0002

[ -d bin ] || mkdir bin
[ -d log ] || mkdir log
[ -d etc ] || mkdir etc
[ -d db ] || mkdir db
[ -d .ep_adm_params ] || mkdir .ep_adm_params

hostname=`hostname`
if echo $hostname | grep -q '^gdp-0'
then
	routers="127.0.0.1"
	for i in 01 02 03
	do
		if [ $hostname != "gdp-$i" ]
		then
			routers="$routers; gdp-$i.eecs.berkeley.edu"
		fi
	done
else
	routers="gdp-01.eecs.berkeley.edu; gdp-02.eecs.berkeley.edu; gdp-03.eecs.berkeley.edu"
fi

if [ ! -f .ep_adm_params/gdp ]
then (
	echo "swarm.gdp.routers=$routers"
	echo "libep.time.accuracy=0.5"
	echo "libep.thr.mutex.type=errorcheck"
	echo "libep.dbg.file=stdout"
	) > .ep_adm_params/gdp
else
	echo "Warning: .ep_adm_params/gdp already exists; check consistency" 2>&1
fi

if [ ! -f .ep_adm_params/gdplogd ]
then (
	echo "swarm.gdplogd.gdpname=edu.berkeley.eecs.$hostname.gdplogd"
	echo "swarm.gdplogd.gcl.dir=$home/db/gcl"
	echo "swarm.gdplogd.runasuser=gdp"
	) > .ep_adm_params/gdplogd
else
	echo "Warning: .ep_adm_params/gdplogd already exists; check consistency" 2>&1
fi

umask 0007
[ -d db/gcl ] || mkdir db/gcl
