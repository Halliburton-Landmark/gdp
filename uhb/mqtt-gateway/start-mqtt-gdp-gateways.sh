#!/bin/sh

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

: ${GDP_ROOT:=/usr/gdp}
START="sh $GDP_ROOT/adm/start-mqtt-gdp-gateway.sh"

$START uhkbbb001.eecs.berkeley.edu edu.berkeley.eecs.swarmlab \
	c098e5300003 \
	c098e570002b \
	c098e5700088 \
	c098e570008b \
	c098e570008e \
	c098e590000a \
	&

$START uhkbbb002.eecs.berkeley.edu edu.berkeley.eecs.swarmlab \
	c098e530000a \
	c098e590000b \
	c098e5900019 \
	c098e5900091 \
	&

$START uhkbbb004.eecs.berkeley.edu edu.berkeley.eecs.bwrc \
	c098e5300009 \
	c098e5300036 \
	c098e5300054 \
	c098e530005d \
	c098e570008f \
	c098e5700090 \
	c098e590001e \
	&
wait
