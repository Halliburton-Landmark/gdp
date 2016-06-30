<!-- Use "pandoc -sS -o README-CAAPI.html README-CAAPI.md" to process this to HTML -->

OTHER INTERFACES TO THE GLOBAL DATAPLANE
========================================

There are some other interfaces to the GDP, generally referred to
as CAAPIs (Common Access APIs).  For the most part these are lightly
documented.

The REST Interface
------------------

The REST interface has not been maintained in some time.  This guide
is incomplete and possibly simply incorrect.

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

Python Key-Value Store
----------------------

Key-Value lookups can be done in Python using
`lang/python/apps/KVstore.py`.  This is a library package for
incorporation into larger Python programs.

<!-- vim: set ai sw=4 sts=4 ts=4 : -->
