#!/bin/sh
#
#  Wrapper for initializing a Docker-ized version of MariaDB.
#  It should be run once to initialize the database.
#
#	This runs on the host, not in the container.
#

# allow environment to give us a different configuration; local overrides
: ${GDP_VER=}
{ test -r /etc/gdp.conf.sh && . /etc/gdp.conf.sh; }
{ test -r /usr/local/etc/gdp.conf.sh && . /usr/local/etc/gdp.conf.sh; }
{ test -r /etc/gdp${GDP_VER}.conf.sh && . /etc/gdp${GDP_VER}.conf.sh; }
{ test -r /usr/local/etc/gdp${GDP_VER}.conf.sh && . /usr/local/etc/gdp${GDP_VER}.conf.sh; }
cd `dirname $0`
. ${GDP_ADM:=../../adm}/common-support.sh

# configure defaults:
# the docker name for this container
: ${GDP_MARIADB_DOCKERID:=gdp-mariadb}
# the (host visible) port
: ${GDP_MARIADB_PORT:=3306}
# the data directory on the host system
: ${GDP_MARIADB_DATADIR:=/var/swarm/gdp/mysql-data}
# the user who owns the data directory
: ${GDP_MARIADB_USER:=UNKNOWN}
# the version of MariaDB to use
: ${GDP_MARIADB_VERSION:="10.4"}

dockerargs=""
if [ ! -d "$GDP_MARIADB_DATADIR" ]; then
	fatal "$GDP_MARIADB_DATADIR must exist and be a directory" $EX_OSFILE
fi

if [ "$GDP_MARIADB_USER" = "UNKNOWN" ];then
	# use (numeric) ownership of data directory
	GDP_MARIADB_USER=`ls -ldn $GDP_MARIADB_DATADIR | awk '{ print $3 ":" $4 }'`
fi

if [ -d $GDP_MARIADB_DATADIR/mysql ]; then
	# this database is already initialized
	error "MariaDB/MySQL has already been initialized."
	info "Data directory is $GDP_MARIADB_DATADIR/mysql"
	exit $EX_OK
fi

info "Initializing database in $GDP_MARIADB_DATADIR"
if [ -z "$GDP_MARIADB_ROOT_PASSWORD" ]; then
	fatal "Must set GDP_MARIADB_ROOT_PASSWORD in environment to initialize" $EX_USAGE
fi
export MYSQL_ROOT_PASSWORD=$GDP_MARIADB_ROOT_PASSWORD

docker run \
	--name $GDP_MARIADB_DOCKERID \
	--user $GDP_MARIADB_USER \
	--rm \
	-dit \
	-p ${GDP_MARIADB_PORT}:3306 \
	-v ${GDP_MARIADB_DATADIR}:/var/lib/mysql \
	-e MYSQL_ROOT_PASSWORD \
	$dockerargs \
	mariadb:$GDP_MARIADB_VERSION

sleep 5
docker stop $GDP_MARIADB_DOCKERID

sudo $GDP_ADM/customize.sh gdp-mariadb.service /etc/systemd/system
sudo systemctl daemon-reload
