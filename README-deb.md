<!-- Use "pandoc -sS -o README-deb.html README-deb.md" to process this to HTML -->

INSTALLING THE GLOBAL DATAPLANE FROM .DEB PACKAGES
==================================================

We maintain pre-compiled binary packages for ubuntu/debain-based systems to
make software distribution easier. The reason for this particular platform
is guided by *swarmbox* testbed that we support. Any software packaging for
other platforms is certainly welcome.

**This README is only relevant if you are installing from the Debian
packages.  Those packages are *not* included with the source distribution.
If you are installing from source, please read `README-compiling.md`.**

Package structure:
=================

* `gdp-client`: Client side C library (`.so` file), header files and various
  compiled utility functions.
* `python-gdp`: Language bindings in Python, requires gdp-client.
* `gdp-server`: GDP log-server.
* `gdp-router-*`: A packaged version of the GDP router, see elsewhere.

In order to write/run C applications, you will need `gdp-client`. If you want
to write Python applications, you will need `python-gdp`. You probably do not
need the server side components unless you are planning on being part of the
infrastructure.

Installation
============

These are standard debian package installation instructions that are included
here for convenience. 

For installing a debian package

    sudo dpkg -i <debian-file>

In case it fails because of missing dependencies, you can fix it by

    sudo apt-get -f install

<!-- vim: set ai sw=4 sts=4 ts=4 : -->
