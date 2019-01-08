# Setting Up a Version 2 GDP Cluster From Source

This describes how to set up a private GDP version 2 cluster.
It assumes that you are:

 1.  Compiling from source code.
 2.  Running on Ubuntu 16.04.  It should work on Ubuntu 18.04 but
     we haven't tested that yet, and might work on MacOS 10.13 and
     FreeBSD 11.2.  It probably will not work on Ubuntu 14.04 since
     we require systemd.
 3.  _Not_ connecting into the existing Berkeley infrastructure.
     If you are, you don't need most of these steps.

***These instructions are largely untested.  Please let us know
how they can be improved.***

Note that many of the included scripts assume you are using the
Berkeley infrastructure and reference Berkeley-specific hosts.  You
will probably have to hand edit some scripts.  A `grep -i` for
`berkeley.edu` will probably find most of them.

The basic outline is:

 * Gather information.
 * Get the code from the Berkeley git repository.
 * Install system packages, compile the GDP library and application
   code, and install it.
 * Set up an instance of MariaDB 10.4 and the Human-Oriented Name
   to GDPname Directory (HONGD) database.  This step uses Docker,
   but you should be able to make HONGD work in a native install.
 * Set up one or more GDP routers.
 * Set up at least one gdplogd (log data server).
 * Set up the Log Creation Service.
 * Check configuration.

Note that these instructions assume they are all being run in one
shell session.  In particular, some variables are set in one step
and then used in later steps.  If you are not installing everything
on the same machine you may need to duplicate configuration steps.

In the future we will have many of these components packaged as
precompiled Docker containers, but for now you have to compile
yourself.

## Gathering Information

Before starting, you will need to make some decisions regarding
where GDP services will be installed and run.  Things to decide:

 * Which host(s) will run GDP routers.
 * Which host(s) will be running gdplogd instances.  These systems
   should have adequate disk space to store your data.
 * Which host will run the Log Creation Service.  This can be
   a virtual machine or be run in a Docker container.
 * Which host will run the MariaDB instance used to store the
   HONGD database.  This currently runs in a Docker container.

## Fetching the Source Code

    root=`pwd`
    git clone git://repo.eecs.berkeley.edu/projects/swarmlab/gdp.git
    git clone git://repo.eecs.berkeley.edu/projects/swarmlab/gdp-router.git

## Install Packages, Compile and Install GDP Libraries

    cd $root/gdp
    adm/gdp-setup.sh
    make
    sudo make install-client

## Set up MariaDB and HONGD

You need to be root or in the `docker` group before running
`gdp-mariadb-init.sh`.  You may need to be root to execute the
`mkdir` command, but it's better if you change the ownership to a
non-privileged user.  In this example we will assume that user
is `gdp:gdp` (i.e., the user name and group name are both `gdp`).

You will also need to come up with a password for the database
administrator (also known as `root`, but it is not the same as `root`
on the host system).

    rootpw=[create a new password]
    sudo mkdir -p /var/swarm/gdp/mysql-data
    sudo chown gdp:gdp /var/swarm/gdp /var/swarm/gdp/mysql-data
    cd $root/gdp/services/mariadb
    env GDP_MARIADB_ROOT_PASSWORD="$rootpw" services/mariadb/gdp-mariadb-init.sh

## Set Up GDP Router(s)

***Rick, please fill in***

    GDP_ROUTERS=[space-separated list of GDP router nodes]
    export GDP_ROUTERS
    cd $root/gdp-router
    make

## Set up GDP Log Server(s)

This should be run on each machine running `gdplogd`.  Be sure that
`GDP_ROUTERS` is set before this is run.

    adm/gdplogd-install.sh

## Set up the Log Creation Service

The `rootpw` is the root password for MariaDB which you created in the
prior step "Set up MariaDB and HONGD."

    sudo mkdir -p /etc/gdp
    (umask 037 && echo "$rootpw" | \
        sudo cp /dev/stdin /etc/gdp/creation_service_pw.txt)
    sudo chown gdp:gdp /etc/gdp /etc/gdp/creation_service_pw.txt
    cd $root/gdp/services/log-creation
    sudo mkdir -p /opt/log-creation2
    sudo cp *.py /opt/log-creation2
    sudo cp logCreationService.service \
        /etc/systemd/system/logCreationService2.service
    sudo systemctl daemon-reload

## Check Configuration

Check the runtime configuration files for rationality.

    vi /etc/ep_adm_params/*

## Language Bindings

These instructions only install the `C` language bindings.  If you want
other languages, look in `$root/lang` and follow the `README` files.

## MISSING ITEMS

What have I forgotten?