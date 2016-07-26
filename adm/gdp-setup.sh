#!/bin/sh

#
#  Set up GDP environment for compilation
#
#	This is overkill if you're not compiling.
#

cd `dirname $0`/..
root=`pwd`
. $root/adm/common-support.sh

info "Setting up packages for GDP compilation."
info "This is overkill if you are only installing binaries."

info "Installing packages needed by GDP for $OS"
case "$OS" in
    "ubuntu" | "debian")
	sudo apt-get update
	sudo apt-get clean
	package libdb-dev
	package libevent-dev
	package libevent-pthreads
	package libssl-dev
	package lighttpd
	package libjansson-dev
	package libavahi-common-dev
	package libavahi-client-dev
	package avahi-daemon
	package pandoc
	if ! ls /etc/apt/sources.list.d/mosquitto* > /dev/null 2>&1
	then
		package software-properties-common
		info "Setting up mosquitto repository"
		sudo apt-add-repository ppa:mosquitto-dev/mosquitto-ppa
	fi
	package libmosquitto-dev
	package mosquitto-clients
	;;

    "darwin")
	package libevent
	package openssl
	package lighttpd
	package jansson
	package pandoc
	if [ "$pkgmgr" = "brew" ]
	then
		package mosquitto
		warn "Homebrew doesn't support Avahi: install by hand"
	else
		package avahi
		warn "Macports doesn't support Mosquitto: install by hand"
	fi
	;;

    "freebsd")
	package libevent2
	package openssl
	package lighttpd
	package jansson
	package avahi
	package mosquitto
	package hs-pandoc
	;;

    "gentoo" | "redhat")
	package libevent-devel
	package openssl-devel
	package lighttpd
	package jansson-devel
	package avahi-devel
	package mosquitto
	warn "Yum doesn't support Pandoc: install by hand"
	;;

    "centos")
	package epel-release
	package libevent-devel
	package openssl-devel
	package lighttpd
	package jansson-devel
	package avahi-devel
	package mosquitto
	package pandoc
	;;

    *)
	fatal "oops, we don't support $OS"
	;;
esac

# vim: set ai sw=8 sts=8 ts=8 :
