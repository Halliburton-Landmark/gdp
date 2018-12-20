#!/bin/sh
#
#  Wrapper for initializing a GDP-ized version of MySQL.
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

# configure defaults
: ${GDP_ROOT:=/usr}
: ${GDP_LOG_DIR:=/var/log/gdp}
: ${GDP_USER:=gdp}
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

# manifest constants (see <sysexits.h>)
EX_USAGE=64
EX_UNAVAILABLE=69
EX_OSFILE=72
EX_NOPERM=77
EX_CONFIG=78

dockerargs="--rm -d"
if [ ! -d "$GDP_MARIADB_DATADIR" ]; then
	echo "[FATAL] $GDP_MARIADB_DATADIR must exist and be a directory" 1>&2
	exit $EX_OSFILE
fi

if [ "$GDP_MARIADB_USER" = "UNKNOWN" ];then
	# use (numeric) ownership of data directory
	GDP_MARIADB_USER=`ls -ldn $GDP_MARIADB_DATADIR | awk '{ print $3 ":" $4 }'`
fi

if [ -d $GDP_MARIADB_DATADIR/mysql ]; then
	# this database is already initialized
	echo "[ERROR] MariaDB/MySQL has already been initialized." 1>&2
	echo "[INFO] Data directory is $GDP_MARIADB_DATADIR/mysql" 1>&2
	exit $EX_CONFIG
fi

echo "[INFO] Initializing database in $GDP_MARIADB_DATADIR" 1>&2
if [ -z "$GDP_MARIADB_ROOT_PASSWORD" ]; then
	echo "[FATAL] Must set GDP_MARIADB_ROOT_PASSWORD in environment to initialize" 1>&2
	exit $EX_USAGE
fi
export MYSQL_ROOT_PASSWORD=$GDP_MARIADB_ROOT_PASSWORD
dockerargs="$dockerargs -e MYSQL_ROOT_PASSWORD"

docker run \
	--name $GDP_MARIADB_DOCKERID \
	--user $GDP_MARIADB_USER \
	-p ${GDP_MARIADB_PORT}:3306 \
	-v ${GDP_MARIADB_DATADIR}:/var/lib/mysql \
	-e MYSQL_ROOT_PASSWORD \
	$dockerargs \
	mariadb:$GDP_MARIADB_VERSION
