#!/bin/sh
(test -r /usr/local/etc/gdp.conf.sh && . /usr/local/etc/gdp.conf.sh) ||
	(test -r /etc/gdp.conf.sh && . /etc/gdp.conf.sh)
: ${GDP_ROOT:=/usr}

#
#  Install system configuration scripts.
#
#	XXX	This installs systemd startup, but does not check if
#		systemd is actually in use.
#	XXX	Similarly, it assumes rsyslog rather than one of the
#		multiple other options.
#
#	This only installs startup files for gdplogd; in particular,
#	it does not include any router startup files.  Appropriate
#	instructions should be found with the router you are using.
#

# we assume this script is installed in the adm directory
cd `dirname $0`/..

. adm/common-support.sh

info "Installing gdplogd wrapper and actual binary"
umask 022
sudo cp adm/gdplogd-wrapper.sh $GDP_ROOT/sbin/gdplogd-wrapper.sh
sudo cp gdplogd/gdplogd $GDP_ROOT/sbin/gdplogd

info "Installing rsyslog configuration"
umask 0222
sudo sh adm/customize.sh adm/60-gdp.conf.template /etc/rsyslog.d
info "... done"

info "Installing gdplogd systemd startup script"
sudo sh adm/customize.sh adm/gdplogd.service.template /etc/systemd/system
info "... done; reloading systemd"
sudo systemctl daemon-reload
sudo systemctl enable gdplogd
