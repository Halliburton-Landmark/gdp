#!/bin/sh

lib=$1
major=$2
minor=$3

rm lib$lib.so.$major.$minor || true
cp ../$lib/lib$lib.so.$major.$minor .
. ../adm/common-support.sh

platform OS
case "$OS" in
    "ubuntu" | "debian" | "freebsd" | "centos")
    	rm lib$lib.so.$major lib$lib.so || true
	ln -s lib$lib.so.$major.$minor lib$lib.so.$major
	ln -s lib$lib.so.$major lib$lib.so
	;;

"darwin")
	rm lib$lib.dylib lib$lib.$major.$minor.dylib || true
	mv lib$lib.so.$major.$minor lib$lib.$major.$minor.dylib
	ln -s lib$lib.$major.$minor.dylib lib$lib.dylib
	;;

"redhat")
	rm lib$lib-$major.$minor.so lib$lib.so.$major || true
	mv lib$lib.so.$major.$minor lib$lib-$major.$minor.so
	ln -s lib$lib-$major.$minor.so lib$lib.so.$major
	;;
esac
