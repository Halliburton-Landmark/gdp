#!/bin/sh

lib=$1
major=$2
minor=$3

rm -f lib$lib.so.$major.$minor
cp ../$lib/lib$lib.so.$major.$minor .
if [ -f ../adm/common-support.sh ]; then 
    . ../adm/common-support.sh
else
    # If we run this script in lang/js/gdpjs, then we need to look further up the tree.
    . ../../../adm/common-support.sh
fi

platform OS
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
