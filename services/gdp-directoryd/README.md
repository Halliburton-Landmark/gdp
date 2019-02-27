
WARNING: ./gdp-directoryd has been deprecated, please use ../gdp-ribd instead.

=========================================================================

# GDP Directory Daemon (Interim Version 2)

This service contains the routing information for the GDP.  ***[[Check
that this is accurate.]]***

***Note Well: this is NOT the Human-Oriented Name to GDPname directory
service.***  That service also uses MariaDB, so you may be able to skip
some of the early setup steps if it is already installed.

The gdp-directoryd interim version 2 binary depends upon the
co-located installation and configuration of a MariaDB Server and
several supporting packages including the OCGraph plugin.

## MariaDB Installation

For Ubuntu 16.04, install the following packages, and restart so plugin loads:

    $ apt install mariadb-server libmariadb2 libmariadb-client-lgpl-dev \
          libmariadb-client-lgpl-dev-compat mariadb-plugin-oqgraph
    $ sudo service mysql restart

## MariaDB Configuration

The database will be configured for localhost access ONLY, but a
production installation should also replace the default "testblackbox"
password (both within setup.sql and the directory daemon source code
later in these instructions so they match!), before proceeding to the
following commands:

    $ sudo mysql -v --show-warnings < setup.sql
    $ sudo mysql -v --show-warnings < blackbox.sql

## MariaDB Service Controls

The database underlying the directory service is now available for use
by the directory daemon. The MariaDB database service (named "mysql")
can be controlled as follows:

    $ sudo service mysql { stop | start | restart }

## Directory Daemon Installation

### Git and Build the GDP Source Tree

    $ mkdir <workspace-name>
    $ cd <workspace-name>
    $ git clone repoman@repo.eecs.berkeley.edu:projects/swarmlab/gdp.git gdp
    $ cd gdp
    $ make

### Directory Daemon Build and Install

Continuing from the prior section:
	
    $ cd services/gdp-directoryd

Replace the password -- specifically, edit the `#define IDENTIFIED_BY`
"<password>" line within gdp-directoryd.c -- to match the password
change, if any, made to the setup.sql file when following the MariaDB
Configuration instructions above.

If gdp-directoryd is already installed and in service on this host,
then use "make reinstall" to replace the binary and restart the
service.

Otherwise, for new installations, run the following to build, install,
and start the GDP Directory Daemon as a service:

    $ make install

The GDP Directory Daemon should now be ready to service requests
on port 9001 of the installation host.

## Directory Daemon Debugging

To debug this daemon, uncomment "#define TESTING" (which will change
the listen port to 9002 and raise the default debug level), rebuild,
then run the program interactively from a suitable command shell.

