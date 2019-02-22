# GDP Routing Information Base (RIB) Daemon

The GDP v2 Network design uses a two-layer routing and forwarding
(switching) framework, similar to modern SDN architectures. The GDP
Routing Information Base (RIB) Daemon is the routing layer for a GDP
Trust Domain.

The GDP RIB Daemon receives network adjacency advertisements and
withdrawals from GDP Routers, maintains an adjacency table and a
dynamic graph viewport on that adjacency table in MariaDB with support
from the OQGraph Engine Plugin, responds to forwarding layer requests,
and expires adjacencies which have not been readvertised recently nor
withdrawn. The GDP forwarding layer can ask the GDP RIB Daemon to
either find (anycast) or mfind (multicast) the shortest weighted path
(anycast) or paths (multicast) from the current location (request
origin) to a destination and expect a response containing the derived
next hop (anycast) or hops (sorted by locality for multicast) or no
route.

***Note: A "SINGLE_ROUTER true" router configuration will never
   attempt to connect to a gdp-ribd, so gdp-ribd installation is not
   required for that special case.***

# GDP RIB Daemon Installation

The following steps were tested on Ubuntu 16.04.

## Step 1: Set up the GDP Repository

Clone the gdp repository, invoke the gdp-setup script, then build from the workspace root:

```
    $ git clone repoman@repo.eecs.berkeley.edu:projects/swarmlab/gdp.git gdp
    $ cd gdp
	$ ./adm/gdp-setup.sh
    $ make
    $ cd services/gdp-ribd
```
## Step 2: Site Specific Password Update [ Optional ]

The GDP RIB Daemon's 'gdp_rib_user' database account is by default
restricted in the database configuration to '127.0.0.1' access, since
the account is only meant to be accessed by a co-resident
gdp-ribd. For some deployments, the '127.0.0.1' access restriction may
be sufficient security. In other deployments, the default account
password can be replaced with a site specific password, by editing
./gdp-ribd.sql (search for "IDENTIFIED BY") and ./gdp-ribd.c (search
for "#define IDENTIFIED_BY") to replace the default 'gdp_rib_pass'
password string with a site specific secret before continuing to the
next step.

## Step 3A: For Docker Installs Only

```
	$ make docker
	$ docker run --network=host gdp-ribd:latest
```

## Step 3B: For Host Installs Only

### MariaDB Installation

Install the following apt packages, and restart mysql to load the plugin:

    $ apt install mariadb-server libmariadb3 mariadb-plugin-oqgraph
    $ sudo service mysql restart

See MariaDB website for current database security configuration best practices.

### MariaDB Setup and Stored Procedure Installation

Load the mariadb gdp-ribd account settings, database, tables, and
stored procedures:

```
    $ sudo mysql -v --show-warnings < gdp-ribd.sql
```

***Note: gdp-ribd.sql will finish with some unit tests, then clean the tables.***

### For First Time Installation Only

***For a new installation***, run the following to build, install, and
   start the systemd managed directory service:

```
	$ sudo make install
```

### For Reinstallation Only

***For a reinstallation***, run the following to stop the systemd
   managed directory service, build and install the binary, then start
   the service:


```
	$ sudo make reinstall
```

The GDP Directory Daemon should now be operational on UDP port 9001.

