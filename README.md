<!-- Use "pandoc -sS -o README.html README.md" to process this to HTML -->

# GLOBAL DATAPLANE INTERFACES

This directory contains a very basic prototype of the Global Dataplane
(GDP) with two bindings.  The first is for C, and the second is a
HTTP-based RESTful interface.  The first is used to implement the
second.  All of these are likely to change in the future.

NOTE: these instructions assume you are starting with the source
distribution.  This is not appropriate if you are installing the
Debian package.  You can get the GDP source distribution using one of:

		git clone https://repo.eecs.berkeley.edu/git/projects/swarmlab/gdp.git

or

		git clone repoman@repo.eecs.berkeley.edu:projects/swarmlab/gdp.git

For the moment that repository is not open; contact Eric to get
access.


## Getting Started

### Quick start: setting up only a client

These are a "quick start" guide to setup a GDP client and be able to
write applications. Details are all included below. These instructions
do not tell you how to setup your own log daemon (or router). For that,
see below. This should work on all supported platforms. If you really
care only about writing application on Debian based systems and you
are okay with slightly older code, you can probably use the packaged
versions (see elsewhere).

   1.	Install the prerequisite packages described below.  For the base
	system all you'll need are `libevent2-dev` and `libssl-dev`, but the
	others are needed for full functionality.  The script
	`adm/gdp-setup.sh` should install everything you need.

   2.	Compile the code as described below.

   3.	Adjust any administrative parameters (see below).  In particular,
	you may want to update `swarm.gdp.routers` if you are using an
	existing router (which you probably are, because you wouldn't be
	reading this section). It defaults to `localhost`, which you would
	like to change to the router you are using. If you want, you can
	also setup the name of the log daemons explicitly.

   4.	Create your first GCL using `apps/gcl-create`.  Use -s
    followed by the name of the log daemon on which to create the
	GCL (as displayed in the previous step) and the name of the new
	log.  If no log name is specified, an internal name is used.

   5.	Run `apps/gdp-writer` giving it the name of the log you just
	created to add data to the log.  It will ask for lines of input
	that are appended ot the log.  There is no need to give the
	name of the log daemon; gdp_router will find it for you.

   6.	Check success by running `apps/gdp-reader` giving it the
	same name as you used in the previous step.

### Elaborate setup: setting up client and your own log-server

These instructions are assuming you are installing your own infrastructure.
For example, if you are using a previously existing router, you can
skip the steps for installing, configuring, and starting the router.
If you are using a Debian package and existing routers and log daemons
you can [skip this section entirely][Using the GDP].

   1.	Install the prerequisite packages described below.  For the base
	system all you'll need are `libevent2-dev` and `libssl-dev`, but the
	others are needed for full functionality.  The script
	`adm/gdp-setup.sh` should install everything you need.

   2.	(Optional) Get the code for a GDP router.  There are two
        versions, as described below.  This step is required only if
	you intend to run a GDP router. For the moment, it is highly
	recommended that you do NOT run your own router instance.

   3.	Compile the code as described below.

   4.	Create the directory `/var/swarm/gdp/gcls`.  This is where the data
	files are kept.  It should be owned by whatever user will be
	running `gdplogd`.  For security reasons, this should NOT be root!!
	(Although you will probably have to be root in order to create
	this directory in the first place.) You can change the name
	of this directory at runtime by setting the administrative
	parameter "`swarm.gdplogd.gcl.dir`", as described toward the
	end of this document.

   5.	Adjust any administrative parameters (see below).  In particular,
	you may want to update `swarm.gdp.routers` if you are using an
	existing router, otherwise it defaults to your local instance of
	the router. In the case you are running your own log daemon, you
	may want to update `swarm.gdplogd.gdpname` so that the log daemon
	has the same name every time it starts up (otherwise it will choose
	a random name).  In any case, it will print its own name when it
	starts up.

   6.	Start the GDP routing daemon (see router README files).

   7.	Start the GDP log daemon (see above).  Make note of its "GDP
	routing name" for use in the next step.

   8.	Create your first GCL using `apps/gcl-create`.  Use -s
    followed by the name of the log daemon on which to create the
	GCL (as displayed in the previous step) and the name of the new
	log.  If no log name is specified, an internal name is used.

   9.	Run `apps/gdp-writer` giving it the name of the log you just
	created to add data to the log.  It will ask for lines of input
	that are appended ot the log.  There is no need to give the
	name of the log daemon; gdp_router will find it for you.

  10.	Check success by running `apps/gdp-reader` giving it the
	same name as you used in the previous step.


### Required Packages

Note that all of these packages can be automatically installed
using the `adm/gdp-setup.sh` script.  It should not be necessary
to install them by hand.

libevent2 --- Event handling library available from
	[`http://libevent.org`](http://libevent.org).
	Most systems (Linux, MacOS, and
	FreeBSD at a minumum) have this available as a package.
	On Debian-derived linux systems (Debian, Ubuntu, etc.)
	you have to install the package libevent-dev; on
	some other systems (e.g., RHEL 6, which only has
	libevent1) you'll have to download directly from
	[`http://libevent.org`](http://libevent.org) and install.

libssl-dev --- for the cryptographic support.

lighttpd --- Web server for RESTful interface.
	Note: I used [macports]([http://macports.org]) to install
	lighttpd on the Mac, which is probably easier than compiling
	it by hand; in particular, it does the Mac-specific
	configuration for you.

scgilib --- C binding for Simple Common Gateway Interface: to connect
	lighttpd with GDP.  It is included (with some local patches)
	with this distribution.  For the original version, see
	[`http://www.xamuel.com/scgilib/`](http://www.xamuel.com/scgilib).

Jansson --- JSON library from
	[`http://www.digip.org/jansson/`](http://www.digip.org/jansson/).
	On Linux it can be installed as the `libjansson-dev` package.
	On MacOS and FreeBSD it is the `jansson` port or pkg.

Avahi --- for Zeroconf discovery.

To install these, use the following commands:

*   On the Mac:
	`sudo port install libevent openssl lighttpd jansson avahi`
*   On FreeBSD:
	`sudo pkg install libevent2 openssl lighttpd jansson avahi`
*   On Ubuntu/Linux:
	`sudo apt-get install libevent-dev libssl-dev lighttpd libjansson-dev libavahi-common-dev libavahi-client-dev`
*   On Gentoo/Linux:
	`sudo emerge openssl libevent lighttpd jansson`

Or, use the helper script `adm/gdp-setup.sh`, which should work
on all these platforms.

### Compiling the GDP Code

First, install the prerequisite packages as shown above.

Compiling should just be a matter of typing "make" in the root.

Note: gcc on linux has a bug that causes it to complain about
non-constant expressions in an initializer when the `-std=c99`
flag is given.  Those same expressions are constant in Clang
and even in gcc without the `-std=c99` flag.  As a result of
this problem, we don't use the `-std=c99` flag by default, but
this means that not all features of C99 are available.
If you want full C99, use `STD=-std=c99` on the make command
line.

Further note: At least some versions of gcc give warnings
about ignored return values even when the function call has
been explicitly voided.  We know about this and do not
consider it to be a bug in the GDP code.  If these warnings
bother you we recommend installing clang and using that
compiler.  (Hint: it gives much better error messages and
catches things that gcc does not.)

As of this writing, GDP works on MacOS 10.9 (using clang),
Ubuntu 14.04 (using gcc or clang), and Freebsd 10.0 (using
clang).  I believe at least one person has it running on
MacOS 10.7 using gcc.

NOTA BENE: JavaScript support in `lang/js` currently has no one
to support it, so it may require extra work to get it going.
It must be built separately using:

		make all_JavaScript

To clean (including removing a rather extensive set of Node.js
modules which are installed by `all_JavaScript`):

		make clean_JavaScript

### Create Log Directory

If you are using the log servers at Berkeley you can skip
this step.

Create a user (or choose an existing user) to own the on-disk
files.  These instructions will assume a new user named
gdp, group gdp; on Debian/Ubuntu:

		sudo adduser --system --group gdp

You'll have to select a location to store the log data.  For
obvious reasons this should be on a filesystem that has a
reasonable amount of free space.  The default is
/var/swarm/gdp/gcls.  You'll probably have to be root to
create this directory:

		sudo mkdir -p /var/swarm/gdp/gcls
		sudo chown -R gdp:gdp /var/swarm

The directory should be mode 700 or 750, owned by gdp:gdp:

		sudo chmod 750 /var/swarm/gdp/gcls

Mode 750 is just to allow users in the gdp group to be able
to peek into the directory.  This should be limited to
people who are maintaining the GDP.

### Adjust Administrative Parameters

If you want to change parameters such as socket numbers or the
GCL directory you can do so without recompiling.  Configuration
files are simple "name=value" pairs, one per line.  There is
a built-in search path "`.ep_adm_params:~/.ep_adm_params:\
/usr/local/etc/ep_adm_params:/etc/ep_adm_params`"
that can be overridden the `EP_PARAM_PATH` environment variable.
(Note: if a program is running setuid then only the two
system paths are searched, and `EP_PARAM_PATH` is ignored.)
Those directories are searched for files named "`gdplogd`" (for
gdplogd only), "`gdp`" (for all programs that use the GDP), or
"`defaults`" (for all programs).

There are many parameters you change, as described near the
end of this README.  In most cases, the ones you are likely
to find interesting (and the files in which they should
probably live) are:

`swarm.gdp.routers`			(file: `gdp`)

> A semicolon-separated list of host names or IP
> addresses to search to find a running routing node.
> This defaults to 127.0.0.1.  If you have a local
> routing node you should name it first, followed
> by "gdp-01.eecs.berkeley.edu; gdp-02.eecs.berkeley.edu"
> (these are run by us for your convenience).  This
> parameter is only consulted if Zeroconf fails.

`swarm.gdplogd.gcl.dir`		(file: `gdplogd`)

> This is the name of the directory you created in
> the previous step.  It only applies on nodes that
> are running a log daemon.

`swarm.gdplogd.gdpname`		(file: `gdplogd`)

> This is a user-friendly name for the name of the
> log daemon.  You'll need this for creating GCLs,
> as described below.  If you don't specify this,
> the name is chosen randomly each time gdplogd
> starts up.

#### Example

In file `/usr/local/etc/ep_adm_params/gdp`:

		swarm.gdp.routers=mygdp.example.com; gdp-01.eecs.berkeley.edu; \
			gdp-02.eecs.berkeley.edu

[This example is line wrapped to fit; when you create
the file it must be on one line.]
This tells application programs where to look for routers if Zeroconf
fails.
In file `/usr/local/etc/ep_adm_params/gdplogd`:

		swarm.gdplogd.gcl.dir=/home/gdp/data/gcls
		swarm.gdplogd.gdpname=com.example.mygdp.gdplogd

This tells `gdplogd` where to store GCL log data (only needed if not
using the default) and what name to use on startup (only needed for
the `gcl-create` command).

### Installing and Starting the GDP Routing Daemon

If you are using the routing daemons at Berkeley you can skip
this step. It is highly recommended that you do not run your
own gdp\_router at the moment.

There are two versions of the router.  Version 1 is easier to
set up and run, but doesn't work through firewalls and doesn't
scale as well as Version 2.

#### Version 1

> Note:
> This version is deprecated; however, it can still be useful
> when debugging the GDP.  In a test environment with only
> one router node it requires no configuration.  If you are not
> debugging the GDP itself, please move on to Version 2.

Version 1 of the router is in it's own separate repository at:
`https://repo.eecs.berkeley.edu/git/projects/swarmlab/gdp_router.git`.
You can get this the same way as the GDP base code, but use
"`gdp_router`" instead of "`gdp`".

Version 1 of the routing daemon uses a simple
"global knowledge" algorithm where every routing node knows
every name known by the system.  Names include running servers
and applications as well as logs.  Because of this, every
routing server **must** be talking with every other routing
server.

To run your own, you'll have to make sure you know the address
of all other routing nodes in the GDP.  This is a temporary
situation, but for now it is critical that all routing nodes
know about all other routing nodes.  To do this you'll have to
get the current list of nodes (`gdp-routers.list`), add your
information to that list, and make sure that list gets updated
on all the nodes.  For now we'll maintain that master list at
Berkeley.

Once you have the list of routers you'll need to start the
Python program "`gdp-router.py`".  It takes several parameters
which are described in detail in `gdp_router/README.md`.  The most
important flag is "`-r`", which takes the list of known
routers as the argument.  For example:

    gdp-router.py -r gdp-routers.list

#### Version 2

Version 2 of the router is in it's own separate repository at:

`repoman@repo.eecs.berkeley.edu:projects/swarmlab/gdp_router_click.git`

You can get this the same way as the GDP base code, but use
"`gdp_router_click`" instead of "`gdp`".

Version 2 has two types of nodes.  Primary nodes work much like
version 1, that is, with global knowledge of all the other
primary nodes.  This implies that Primary nodes cannot be behind
a firewall.  Secondary nodes need to talk to a Primary node,
and can be inside a firewall.

See the instructions in that directory for details.

### Start the GDP Log Daemon

If you are using the log servers at Berkeley you can skip
this step.

The program gdplogd implements physical on-disk logs.  It must
be started after the routing layer.  Located in gdplogd/gdplogd,
it takes these parameters:

* `-D` _debug-spec_

> Turn on debugging.  See "Setting Debug Flags" below
> for more information.  Implies `-F`.

* `-F`

> Run in foreground.  At the moment, gdpd always runs
> in foreground, but the intent is that it will default
> to background mode without this flag.

* `-G` _gdp-addr_

> Use _gdp-addr_ as the address of the routing layer
> daemon.  Defaults to `localhost:8007`.  Can also be set
> with the `swarm.gdp.ip_addr` administrative parameter.
> See "Changing Parameters" below for more information.

* `-N` _routing-name_

> Sets the GDP routing name, overriding the
> `swarm.gdplogd.gdpname` configuration value.

* `-n` _workers_

> Start up _workers_ worker threads.  Defaults to a
> minimum of one thread which can expand up to twice
> the number of cores available.  Can also be set (with
> finer control) using the `libep.thr.pool.min_workers`
> and `libep.thr.pool.max_workers` parameters.

## Using the GDP

Once you have the GDP infrastructure set up, you can start using
the GDP to get work done.

### Create a GCL

For the time being,
to create your own logs you'll need the GDP name (_not_ the
DNS name) of the log server that will store the log.  This
can be set as an administrative parameter, and the internal
form is printed out as the log server starts up.  We'll run
three log servers at Berkeley you can use:

		edu.berkeley.eecs.gdp-01.gdplogd
		edu.berkeley.eecs.gdp-02.gdplogd
		edu.berkeley.eecs.gdp-03.gdplogd

You'll also need to select a name for your log.  We
recommend using a name that is unlikely to clash with other
logs, either similar to the gdplogd example above or using
_institution_`/`_project_`/`_name_.  For example,

		edu.berkeley.eecs.swarmlab.sensor23
		berkeley/swarmlab/sensor/23

would both be reasonable choices.  To actually create the log on
a given server, use

>	`gcl-create` _server_ _logname_

For example:

		gcl-create edu.berkeley.eecs.gdp-01.gdplogd \
			berkeley/swarmlab/sensor/23

The good news is that this the only time you'll need to know
the name of the log server.

### Running GDP-based Programs

There are two sample programs included with the GDP: `gdp-reader`
and `gdp-writer`.  There are also some demo programs you can
copy and adapt for your own use.  The descriptions given
below do not include all of the command line flags; these are
just the ones that are most useful.

Note that both the routing and the log daemons must be running
before a GDP program can start up.  In many cases, GDP programs
will try to reconnect if a component (gdplogd, gdp\_router) goes
down, but they must all be up and running before they will start
in the first place.

The gdp-writer program reads records from the standard input
and writes to the target log.  It is invoked as:

		gdp-writer [-D dbgspec] [-G router-addr] gcl-name

The `-D` flag turns on debugging output and is described below.
The `-G` flag overrides the swarm.gdp.routers parameter.  The
_gcl-name_ is the name of the GCL to be appended to.  Lines are
read from the input and written to the log, where each input
line creates one log record.  See the gdp-writer(1) man page
for more details.

The gdp-reader program reads records from the log and writes
them to the standard output.  It is invoked as:

		gdp-reader [-D dbgspec] [-f firstrec] [-G router-addr] \
			[-m] [-M] [-n nrecs] [-s] [-v] gcl-name

The `-D` and `-G` flags are the same as gdp-writer.  The `-f` and
`-n` flags specify the first record number (starting from 1) and
the maximum number of records to read.  There are three ways
gdp-reader can operate.  By default, it reads the records
one at a time in separate read requests.  The `-m` flag changes
this to "multiread" mode, where the records are delivered in
a more efficient way --- essentially, the multiread command
returns success and then the data records are sent after the
command acknowledgement.  The `-s` flag turns on subscription
mode, which is like -m except that the log daemon will wait
until new records are added and deliver them spontaneously to
subscribers.  The `-v` flag prints signature information associated
with the record.  For example:

		gdp-reader -f 1 -s eric/sensor45

will return all the data already recorded in the log and then
wait until more data is written; the new data will be
immediately printed.  If `-m` were used instead of `-s` then
all the existing data would be returned, and then it would
exit.  If `-s` were used without `-f`, no existing data would
be printed, only new data.  See the gdp-reader(1) man page for
more details.

There are man pages available in the source tree for
gdp-reader, gdp-writer, and most of the other
Berkeley-supplied programs.

### Running the GDP RESTful Interface

(In these instructions, _gcl-name_ is a URI-base\-64-encoded
string of length 43 characters.  A _recno_ is a positive
non-zero integer.  Note that for the moment the RESTful interface
is not being maintained due to lack of use.)

1.	Do the "Getting Started" steps described above.

2.	The instructions for SCGI configuration for lighttpd are totally
	wrong.  The configuration file you actually need is:

			server.modules += ( "mod_scgi" )

			scgi.server = (
				"/gdp/v1/" =>
					( "gdp" =>
						( "host"  => "127.0.0.1",
						  "port" => 8001,
						  "check-local" => "disable",
						)
					)
				)

	(Normally in `/usr/local/etc/lighttpd/conf.c/scgi.conf`) This will
	tell lighttpd to connect to an SCGI server on the local machine,
	port 8001.  You'll also need to make sure the line
	"`include conf.d/scgi.conf`" in `/usr/local/etc/lighttpd/modules.conf`
	is not commented out.  The rest of the lighttpd setup should be
	off the shelf.  I've set up instance of lighttpd to listen on
	port 8080 instead of the default port 80, and the rest of these
	instructions will reflect that.

3.	Start up the GDP RESTful interface server in `apps/gdp-rest`.
	It will run in foreground and spit out some debugging
	information.  For even more, use `-D\*=20` on the command
	line.  This sets all debug flags to level 20.  The backslash
	is just to keep the Unix shell from trying to glob the
	asterisk.

4.	Start the lighttpd server, for example using:

			lighttpd -f /usr/local/etc/lighttpd/lighttpd.conf -D

	This assumes that your configuration is in
	`/usr/local/etc/lighttpd`.  The `-D` says to run in foreground
	and you can skip it if you want.  You may want to turn on
	some debugging inside the daemon to help you understand the
	interactions.  See ...`/etc/lighttpd/conf.d/debug.conf`.

5.	The actual URIs and methods used by the REST interface are
	described in `doc/gdp-rest-interface.html`.  See that file for
	details.

You can do GETs from inside a browser such as Firefox or
Chrome, but not POSTs.  To use other methods you'll have to
use Chrome.  Install the "postman" extension to enable
sending of arbitrary methods such as POST and PUT.

### Other GDP Interfaces

The GDP supports the concept of CAAPIs (Common Access APIs) that
can present different programming models.  For example,
`lang/python/apps/KVstore.py` presents a log as a key-value store
using a Python interface.  Other CAAPIs are planned.

## Writing Code

The native programming interface for the GDP is in C.  The API is
documented in `doc/gdp-programmatic-api.html`.  To compile,
you'll need to include `-I`_path-to-include-files_; if the GDP
is installed in the standard system directories you can skip this.
To link, you'll need `-L`_path-to-libraries_ (not needed if the
GDP is installed in the standard system directories) and
`-lgdp -lep -lcrypto -levent -levent_pthreads -lavahi-client -lavahi-common`
when linking.

There is also a binding for Python in `lang/python` which is well
tested, a binding for Java in `lang/java` which is lightly tested,
and a binding for JavaScript in `lang/js` which is incomplete.
Documentation for the Python bindings is in `lang/python/README`.

## For GDP Developers Only

The rest of the information in this file is for people who are
developing the GDP code itself.  If you are a GDP user rather than
a GDP developer, please stop reading here.

### Full List of Administrative Parameters

* `swarm.gdp.routers` --- the address(es) of the gdp routing layer
	(used by clients).  Multiple addresses can be included,
	separated by semicolons.  If no port is included, it
	defaults to 8007.  This will eventually be replaced by
	service discovery.  Defaults to 127.0.0.1:8007.

* `swarm.gdp.event.loopdelay` --- if the event loop exits for some
	reason, this is the number of microseconds to delay
	before restarting the loop.  Defaults to 100000 (100msec).

* `swarm.gdp.event.looptimeout` --- the timeout for the event loop;
	this is mostly just to make sure things don't "hang up"
	forever.  Defaults to 30 (seconds).

* `swarm.gdp.connect.timeout` --- how long to wait for a connection
	to the GDP routing layer before giving up and trying
	another entry point (in milliseconds).  Defaults to
	10000 (ten seconds).

* `swarm.gdp.reconnect.delay` --- the number of milliseconds to wait
	before attempting to reconnect if the routing layer is
	disconnected.  Defaults to 100 milliseconds.

* `swarm.gdp.invoke.timeout` --- the number of milliseconds to wait
	for a response before timing out a GDP request.  Defaults
	to 10000 (ten seconds).

* `swarm.gdp.invoke.retries` --- the number of times to re-send a
	request when no response is received.  Defaults to 3
	(meaning that the request may be sent up to 4 times
	total).

* `swarm.gdp.invoke.retrydelay` --- the number of milliseconds
	between retry attempts.  Defaults to 5000 (five seconds).

* `swarm.gdp.crypto.key.path` --- path name to search for secret
	keys when writing a log.  Defaults to
	`.:KEYS:~/.swarm/gdp/keys:/usr/local/etc/swarm/gdp/keys:/etc/swarm/gdp/keys`.

* `swarm.gdp.crypto.key.dir` --- the directory in which to store the
	secret keys when creating a GCL.  Defaults to `KEYS`.
	Can be overridden by gcl-create `-K` flag.

* `swarm.gdp.crypto.hash.alg` --- the default hashing algorithm.
	Defaults to `sha256`.  Can be overridden by gcl-create
	`-h` _alg_ flag.

* `swarm.gdp.crypto.sign.alg` --- the default signing algorithm.
	Defaults to `ec`.  Can be overridden by gcl-create
	`-k` flag.

* `swarm.gdp.crypto.keyenc.alg` --- the default secret key encryption
	algorithm.  Defaults to "aes192".  A value of "none"
	turns off encryption.  Can be overridden by gcl-create
	`-e` flag.

* `swarm.gdp.crypto.ec.curve` --- the default curve to use when
	creating an EC key.  Defaults to sect283r1 (subject
	to change).  Can be overridden by gcl-create `-c` flag.

* `swarm.gdp.crypto.dsa.keylen` --- the default size of an RSA
	signature key in bits.  Defaults to 2048.  Can be
	overridden by gcl-create `-b` flag.

* `swarm.gdp.crypto.rsa.keylen` --- the default size of an RSA
	signature key in bits.  Defaults to 2048.  Can be
	overridden by gcl-create `-b` flag.

* `swarm.gdp.crypto.rsa.keyexp` --- the exponent for an RSA signature
	key.  Defaults to 3.

* `swarm.gdp.syslog.facility` --- the name of the log facility to use
	(see syslog(3) for details).  The `gdp` in the name
	can be replaced by an individual program name to set a
	value that applies only to that program, e.g.,
	`swarm.gdplogd.log.facility`.  If both are set, the more
	specific name wins.  Defaults to `local4`.

* `swarm.gdp.zeroconf.domain` --- the domain searched in zeroconf
	queries.  Defaults to `local`.

* `swarm.gdp.zeroconf.enable` --- enable zeroconf lookup when programs
	start up.  Defaults to `true`.

* `swarm.gdp.zeroconf.proto` --- the protocol used in zeroconf queries.
	Defaults to `_gdp._tcp`.

* `swarm.gdplogd.gcl.dir` --- the directory in which log data will
	be stored.  Defaults to `/var/swarm/gdp/gcls`.

* `swarm.gdplogd.reclaim.interval` --- how often to wake up to
	reclaim unused resources.  Defaults to 15 (seconds).

* `swarm.gdplogd.reclaim.age` --- how long a GCL is permitted to
	sit idle before its resources are reclaimed.  Defaults
	to 300 (seconds, i.e., five minutes).

* `swarm.gdplogd.gdpname` --- the name to use as the source address
	for protocol initiating from this program.  If not set,
	a random name is made up when the program is started.
	Generally speaking, you will want to set this parameter;
	I recommend reverse-dns addresses, e.g.,

		swarm.gdplogd.gdpname=edu.berkeley.eecs.gdp-01.gdplogd.

* `swarm.gdplogd.runasuser` --- the name of the UNIX account to
	switch to if gdplogd is started as root.  Generally
	it is better if gdplogd is *not* run as root; this
	parameter avoids mistakes.  If the named account does
	not exist it defaults to 1:1 (user daemon on most
	systems).

* `swarm.gdplogd.crypto.strictness` --- how strictly signatures
	are enforced.  This is a sequence of comma-separated
	words where only the first letter is significant.
	Values are `verify` (signature must verify if it
	exists), `required` (signature must be present if the
	GCL has a public key), and/or `pubkeyreq` (public
	key is required).  For now, defaults to `verify`.

* `swarm.gdplogd.subscr.timeout` --- how long a subscription will
	be kept active without being refreshed (essentially,
	the length of a "lease" on the subscription).  Defaults
	to 600 (seconds).


* `swarm.rest.kv.gclname` --- the name of the GCL to use for the
	key-value store.  Defaults to "swarm.rest.kv.gcl".

* `swarm.rest.prefix` --- the REST prefix (e.g., `/gdp/v1/`).

* `swarm.rest.scgi.port` --- the port number for the SCGI server to
	listen on.  If you change this you'll also have to
	change the lighttpd configuration.  Defaults to 8001.

* `swarm.rest.scgi.pollinterval` --- how often to poll for SCGI
	connections (in microseconds).  Defaults to 100000
	(100 msec).


* `libep.crypto.dev` --- whether or not to try to use `/dev/crypto`
	for hardware acceleration.  Defaults to true.

* `libep.time.accuracy` --- the value filled in for the "accuracy"
	field in time structures (defaults to zero).

* `libep.thr.pool.min_workers` --- the default minimum number of
	worker threads in the thread pool.  If not specified
	the default is 1.  It can be overridden by the calling
	application.

* `libep.thr.pool.max_workers` --- the default maximum number of
	worker threads in the thread pool.  If not specified
	the default is two times the number of available
	cores.  It can be overridden by the calling application.

### Setting Debug Flags

You can turn on debugging output using a command line flag,
conventionally "`-D`_pattern_`=`_level_".  The _pattern_ specifies
which flags should be set and _level_ specifies how much
should be printed; zero indicates no output, and more output
is added as the values increase.

By convention _level_ is no greater than 127, and values 100
and above may modify the base behavior of the program (i.e.,
do more than just printing information).

Each debug flag has a hierarchical name with (by convention)
"." as the separator, for example, "`gdp.proto`" to indicate
the protocol processing of the GDP.  The "what(1)" program on
a binary will show you which debug flags are available
including a short description.  (The `what` program isn't available
on Linux; it can be simulated using `strings | grep '@(#)'`).

To enable debugging for all patterns, use "`*=`_level_", for
example "`*=18`".

### Directory Structure

* ep --- A library of C utility functions.  This is a stripped
	down version of a library I wrote several years ago.
	If you look at the code you'll see vestiges of some
	of the stripped out functions.  I plan on cleaning
	this version up and releasing it again.

* gdp --- A library for GDP manipulation.  This is the library
	that applications must link to access the GDP.

* gdplogd --- The GDP log daemon.  This implements physical
	(on disk) logs for the GDP.  The implementation is
	still fairly simplistic.  It depends on a routing
	layer (currently gdp_router, in a separate repository).

* scgilib --- An updated version of the SCGI code from
	[`http://www.xamuel.com/scgilib/`](http://www.xameul.com/scgilib/).
	SCGI permits a web server to access outside programs by opening
	a socket in a manner much more efficient than basic
	CGI fork/exec.  This is only used for the REST interface.

* apps --- Application programs, including tests.

* doc --- Some documentation, woefully incomplete.

* examples --- Some example programs, intended be usable as
	tutorials.

* lang --- sub-directories with language-specific application
	programs and supporting code.

* lang/java --- Java-specific apps and libraries.

* lang/js --- JavaScript-specific apps and libraries.  Also contains
	the Node.js/JS GDP RESTful interface code.  See associated
	README files for details.

* lang/python --- Python-specific apps and libraries.  See the
	associated README file for details.

<!-- vim: set ai sw=4 sts=4 ts=4 : -->
