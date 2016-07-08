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

adm=adm

. $adm/common-support.sh

info "Installing gdplogd wrapper"
umask 022
sudo cp $adm/gdplogd-wrapper.sh $GDP_ROOT/sbin/gdplogd-wrapper.sh

info "Installing rsyslog configuration"
umask 0222
sudo sh $adm/customize.sh $adm/60-gdp.conf.template /etc/rsyslog.d
info "... done"

info "Installing gdplogd systemd startup script"
sudo sh $adm/customize.sh $adm/gdplogd.service.template /etc/systemd/system
info "... done; reloading systemd"
sudo systemctl daemon-reload
sudo systemctl enable gdplogd
