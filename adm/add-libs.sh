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

try_lib() {
	lib=$1
	candidates=`ls \
		/lib/lib$lib.* \
		/usr/lib*/lib$lib.* \
		/usr/lib/*/lib$lib.* \
		/usr/local/lib*/lib$lib.* \
		2>/dev/null`
	if [ ! -z "$candidates" ]
	then
		echo "-l$lib"
	fi
}

try_lib db
try_lib execinfo
try_lib systemd
