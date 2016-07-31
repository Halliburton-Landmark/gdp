#!/bin/sh

#
#  If we are running git, get the current commit info
#

cd `dirname $0`/..

if type git > /dev/null 2>&1 && test -d .git
then
	# this will return the null string if we're not under git control
	rev=`git rev-parse HEAD 2> /dev/null`
	mods=`git status -s | grep -v '??'`
	test -z "$mods" || mods="++"
else
	# git isn't installed
	rev=""
	mods=""
fi
dtime=`date +'%Y-%m-%d %H:%M'`

# add a leading space
test -z "$rev" || rev=" $rev"

echo "($dtime)$rev$mods"
