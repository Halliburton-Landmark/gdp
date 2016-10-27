#!/bin/sh

#
#  This script outputs additional flags for use when loading binaries.
#  For example, it might output "-ldb" if this system has the Berkeley
#  db library in a separate library (as opposed to being part of libc).
#
#  The only parameter is the name of the module being loaded.  In
#  particular, gdplogd needs libraries that normal apps do not.
#

module=${1-none}

candidates=`ls \
	/lib/libdb.* \
	/usr/lib*/libdb.* \
	/usr/lib/*/libdb.* \
	/usr/local/lib*/libdb.* \
	2>/dev/null`
if [ ! -z "$candidates" ]
then
	echo "-ldb"
fi

candidates=`ls \
	/usr/lib/libsystemd.* \
	/usr/lib/*/libsystemd.* \
	2>/dev/null`
if [ ! -z "$candidates" ]
then
	echo "-lsystemd"
fi
