#!/bin/sh

lib=$1
major=$2
minor=$3

{ test -r /usr/local/etc/gdp.conf.sh && . /usr/local/etc/gdp.conf.sh; } ||
	{ test -r /etc/gdp.conf.sh && . /etc/gdp.conf.sh; }
: ${GDP_ROOT:=/usr}
: ${GDP_USER:=gdp}
: ${GDP_GROUP:=gdp}
if [ "$GDP_ROOT" = "/usr" ]
then
	: ${GDP_ETC:=/etc/gdp}
else
	: ${GDP_ETC:=$GDP_ROOT/etc}
fi

# can override search for GDP source root node by setting GDP_SRC_ROOT.
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

info "Creating lib$lib links in `pwd`"
case "$OS" in
    "ubuntu" | "debian" | "freebsd" | "centos")
    	rm -f lib$lib.so.$major lib$lib.so
	ln -s lib$lib.so.$major.$minor lib$lib.so.$major
	ln -s lib$lib.so.$major lib$lib.so
	;;

"darwin")
	rm -f lib$lib.dylib lib$lib.$major.$minor.dylib
	mv lib$lib.so.$major.$minor lib$lib.$major.$minor.dylib
	ln -s lib$lib.$major.$minor.dylib lib$lib.dylib
	;;

"redhat")
	rm -f lib$lib-$major.$minor.so lib$lib.so.$major
	mv lib$lib.so.$major.$minor lib$lib-$major.$minor.so
	ln -s lib$lib-$major.$minor.so lib$lib.so.$major
	;;
esac
