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

package() {
    info "Checking package $1..."
    case "$OS" in
	"ubuntu" | "debian")
	    if dpkg --get-selections | grep --quiet $1; then
		info "$1 is already installed. skipping."
	    else
		sudo apt-get install -y $@
	    fi
	    ;;
	"centos" | "redhat")
	    if rpm -qa | grep --quiet $1; then
		info "$1 is already installed. skipping."
	    else
		sudo yum install -y $@
	    fi
	    ;;
	"darwin")
	    if [ "$pkgmgr" = "brew" ]; then
		if brew list | grep --quiet $1; then
		    info "$1 is already installed. skipping."
		else
		    brew install --build-bottle $@ || brew upgrade $@
		fi
	    else
		if port -q installed $1 | grep -q "."; then
		    info "$1 is already installed. skipping."
		else
		    sudo port install $1
		fi
	    fi
	    ;;
	"freebsd")
	    export PATH="/sbin:/usr/sbin:$PATH"
	    if sudo pkg info -q $1; then
		info "$1 is already installed. skipping."
	    else
		sudo pkg install $@
	    fi
	    ;;
	"gentoo")
	    if equery list $1 >& /dev/null; then
		info "$1 is already installed. skipping."
	    else
		sudo emerge $1
	    fi
	    ;;
	*)
	    fatal "unrecognized OS $OS"
	    ;;
    esac
}

platform OS
info "Installing packages needed by GDP for $OS"
case "$OS" in
    "ubuntu" | "debian")
	sudo apt-get update
	sudo apt-get clean
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
	pkgmgr=none
	if type brew > /dev/null 2>&1 && [ ! -z "`brew list`" ]; then
	    pkgmgr=brew
	    sudo brew update
	fi
	if type port > /dev/null 2>&1 && port installed | grep -q .; then
	    if [ "$pkgmgr" = "none" ]; then
		pkgmgr=port
		sudo port selfupdate
	    else
		info "You seem to have both macports and homebrew installed."
		fatal "You will have to deactivate one (or modify this script)"
	    fi
	fi
	if [ "$pkgmgr" = "none" ]; then
	    fatal "you must install macports or homebrew"
	fi
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
