<!-- Use "pandoc -sS -o README.html README.md" to process this to HTML -->

# URBAN HEARTBEAT KIT MQTT-GDP GATEWAY

This directory contains applications and scripts to support the
Urban Heartbeat project from TerraSwarm.

This is included in the default git tree, but it isn't a make target
from the root directory because it requires some other libraries that
aren't all that common (notably mosquitto).

This directory contains both common tools (which can be used for
multiple things) and startup scripts for very specific purposes.
Generically, the mqtt-gdp-gateway program will take one or more
arbitrary MQTT topic patterns and log them into corresponding GDP
logs.  The startup scripts are specifically for the Urban Heartbeat
project.  Specifically, the startup scripts will set up daemons to
log the MQTT feeds from the U. Michigan devices (BLEE, PowerBlade,
and Blink for now) into one log per device.  You have to configure
the names of the MQTT brokers and the names of the devices of
interest.


## Required Packages

The mqtt-gdp-gateway application requires Mosquitto, an MQTT
client library.  On Debian and Ubuntu you'll need to install
`libmosquitto0-dev`.  On FreeBSD the package is named `mosquitto`.
On MacOS mosquitto is supported by brew, but not bsdports.
However it is easy to download and compile the library from source.

Except for MacOS, the setup-mqtt-gateway.sh script will install the
prerequisite packages.  It will also download and build the third
party code (notably the U. Michigan code).


## What's in This Directory?

### mqtt-gdp-gateway.c

The mqtt-gdp-gateway application that reads one or more MQTT topics
(specified by a pattern) and copies all messages it receives to a GDP
log.

For the moment mqtt-gdp-gateway assumes that all MQTT messages are
formatted as JSON, and (assuming this is true) writes records that
are formatted as JSON.  The output format is "wrapped" with a JSON
object to include metadata.

MQTT topics are specified using a standard topic pattern.  Quoting
from the mosquitto mqtt(7) man page:

---

Topics are treated as a hierarchy, using a slash (/) as a separator.
This allows sensible arrangement of common themes to be created,
much in the same way as a filesystem. For example, multiple
computers may all publish their hard drive temperature information
on the following topic, with their own computer and hard drive name
being replaced as appropriate:

    sensors/COMPUTER_NAME/temperature/HARDDRIVE_NAME

Clients can receive messages by creating subscriptions. A subscription
may be to an explicit topic, in which case only messages to that topic
will be received, or it may include wildcards. Two wildcards are
available, `+` or `#`.

`+` can be used as a wildcard for a single level of hierarchy. It could
be used with the topic above to get information on all computers and
hard drives as follows:

    sensors/+/temperature/+

As another example, for a topic of "a/b/c/d", the following example
subscriptions will match:

* a/b/c/d
* +/b/c/d
* a/+/c/d
* a/+/+/d
* +/+/+/+

The following subscriptions will not match:

* a/b/c
* b/+/c/d
* +/+/+

`#` can be used as a wildcard for all remaining levels of
hierarchy. This means that it must be the final character in a
subscription. With a topic of "a/b/c/d", the following example
subscriptions will match:

* a/b/c/d
* \#
* a/#
* a/b/#
* a/b/c/#
* +/b/c/#

Zero length topic levels are valid, which can lead to some slightly
non-obvious behaviour. For example, a topic of "a//topic" would
correctly match against a subscription of "a/+/topic". Likewise,
zero length topic levels can exist at both the beginning and the
end of a topic string, so "/a/topic" would match against a
subscription of "+/a/topic", "#" or "/#", and a topic "a/topic/"
would match against a subscription of "a/topic/+" or "a/topic/#".

---

See the mqtt-gdp-gateway(1) man page for more details.

### setup-uhb-gateway.sh

Fetch the U. Michigan from the `lab11` repository, install
prerequisite packages, compile everything necessary, and install
the system startup scripts necessary to run that code on reboot.
Note that the U. Michigan packages _must_ run on a properly
configured BeagleBone Black from the `/home/debian` directory.
This _only_ installs the U. Michigan code.

### install-mqtt-gdp-gateway.sh

This script creates the `gdp` user and directories necessary to
run the MQTT-GDP gateway.  It then compiles the `mqtt-gdp-gateway`
code and installs both it and the system startup scripts as
described below.  Note that the GDP code *must* be installed
before this script is run.

This may run on a BeagleBone, but it isn't necessary.  A typical
scenario will be to run the `mqtt-gdp-gateway` on an Ubuntu machine
which will pull from the BeagleBone.

### mqtt-gdp-gateway.conf, mqtt-gdp-gateways.conf

The Upstart configuration files.  The former starts a single instance
of mqtt-gdp-gateway, i.e., one connected to a single MQTT broker.
The latter will startup several mqtt-gdp-gateway instances running
on multiple brokers, as defined by the `MQTT_SERVERS` variable in
`/etc/default/mqtt-gdp-gateway`.  That file should also set the
variable `MQTT_LOG_ROOT` to have the root name of any logs that will
be created.  For example, we use edu.berkeley.eecs.swarmlab.device,
which will have the actual device name appended.

### start-mqtt-gdp-gateway.sh

A helper script used to start up the MQTT-GDP gateway program.  It
is normally invoked by Upstart, but it can also be invoked by hand.
It takes the fully qualified domain name of the machine running the
MQTT broker as its only argument.  The domain part of that name is
stripped off and the remaining host name is used to find a
configuration in `$GDP_ETC/mqtt-gateway.$host` (`$GDP_ETC`
defaults to `/etc`.)  The first line of that file is the base
name for the GDP log names that will be created; for example,
it might be `edu.berkeley.eecs.swarmlab`, which will result in
logs named `edu.berkeley.eecs.swarmlab.device._id_`.  The
remaining lines of that file are the names of devices of interest.
Those names will be used both for building the MQTT topic name
and the GDP log name.

Logs will be created as necessary (it may be helpful to set the
`swarm.gcl-create.server` administrative parameter).  They will
be named `$basename.device.$id`, where `$basename` is from the
first line of the `mqtt-gateway` file and `$id` is from subsequent
lines in the same file.

It then starts `mqtt-gdp-gateway` with the necessary parameters
built from the above information.

For example, if the file `/etc/gdp/mqtt-gateway.foo` contains:

	edu.berkeley.eecs.swarmlab
	dev23
	dev37

then `start-mqtt-gdp-gateway.sh foo.example.org` will create the logs

	edu.berkeley.eecs.swarmlab.device.dev23
	edu.berkeley.eecs.swarmlab.device.dev37

and then start reading the MQTT broker at `foo.example.org` for
the topics

	device/+/dev23
	device/+/dev37

Those topics will be copied into the corresponding logs.


## Bugs/Warnings

* The start-mqtt-gdp-gateway.sh script only discovers configuration
  changes on startup, not on the fly.  To add or delete  devices the
  script must be restarted.
* The Upstart scripts assume that they are running on the same
  machine as `gdplogd`; in particular, they start and stop based
  on an Upstart job named `gdplogd` running on the same machine.
  Although this is the common case, there is no reason these
  scripts cannot run on a GDP client-only system.
* **Note:** The Debian systems running on the BeagleBone use
  systemd as their startup system; hence, the U. Michigan code
  uses systemd.  Conversely, Ubuntu 14.04 uses Upstart; hence,
  the MQTT-GDP code uses Upstart.  Systemd support is in progress.
