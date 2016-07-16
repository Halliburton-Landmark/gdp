#!/bin/sh
{ test -r /usr/local/etc/gdp.conf.sh && . /usr/local/etc/gdp.conf.sh; } ||
	{ test -r /etc/gdp.conf.sh && . /etc/gdp.conf.sh; }
: ${GDP_ROOT:=/usr}

#
#  Configuration for MQTT-GDP gateway
#
#	Right now this is just a shell script to start the
#	individual instances.
#
#	The $START script takes at least three arguments:
#		* The name of the host holding the MQTT broker
#		* The root name of the logs that will be
#		  created.  This will have ".device.<dev>"
#		  appended, where <dev> is given in the third
#		  through Nth arguments.
#		* The names of the devices from that broker
#		  (at least one).
#
#	It is up to the administrator to make sure there are no
#	duplicates.  This can occur if one device is in range of
#	two or more gateways.
#

START="sh $GDP_ROOT/sbin/start-mqtt-gdp-gateway.sh"

$START uhkbbb001.eecs.berkeley.edu &
$START uhkbbb002.eecs.berkeley.edu &
$START uhkbbb004.eecs.berkeley.edu &
wait
