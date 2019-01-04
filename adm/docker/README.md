**Note: This is for V2. Eventually, the V2 distinction should be dropped.**

***NOTA BENE: This is not up to date.  It's still to be done.***

# Introduction

This directory contains a very basic setup for creating docker images
for the GDP, including:

- `gdp-dev-c`, an Ubuntu 16.04-based compile environment for GDP
  applications that are written in "C".  It has the necessary packages
  and the GDP software installed.  The expectation is that application
  developers will run this directly, although it can also be
  referenced in a `FROM` command to containerize user appplications.
- `gdp-dev-python`, the equivalent for applications written in Python.
  *[[Not yet available.]]*
- `gdplogd`, for running the GDP log server.  This is described
  in more detail below.
- `gdp-router`, for running the GDP routing layer.  *[[Not currently
  implemented here --- it's actually in the gdp_router repo.]]*

There are also two images that are intended for use only from with
`FROM` commands:

- `gdp-src-base`, containing the GDP source and libraries necessary
  for compilation.  This is based on ubuntu-16.04 and hence has a
  full compiler environment.  It is very similar to `gdp-dev-c` with
  the addition of the GDP source code.
- `gdp-run-base`, containing the libraries necessary to run GDP
  applications, but not including a full Ubuntu distribution, notably
  excluding development tools.  For example, see the `Dockerfile`
  entry for `gdplogd` to see how to use this.
  _[[Right now this uses `gdp-dev-c` as the base, but that should be
  changed to use alpine.]]_


# Building Docker Images

Running `make` should be sufficient to build the standard set of
images.  Note that it builds the image but does not push them to
a registry.  *We should create a GDP registry for this.*
A `make` variable `VER` is used as the tag of all of these images;
this is normally the version version of the GDP release.
For example, when building version 1.2.3 of the GDP, you **should**
use:

	make VER=1.2.3

This version number should match the version number defined in
`gdp/Makefile` (variables `GDP_VERSION_MAJOR`, `GDP_VERION_MINOR`,
and `GDP_VERSION_PATCH`) and have a `git` tag `r$VER` in the
repository.  If these are not the same then you'll also have to
specify a specific `BRANCH`.  If `VER` is not specified it defaults
to `latest` and `BRANCH` defaults to `master`.  If `VER` is specified,
then the tag `latest` is **not** included in the image, which may be
confusing in the Docker world..
*[[We should probably fix this.]]*
*[[Ideally we would extract `VER` directly from the Makefile.]]*

The source code used to build the instances is *not* based on what
you have in the current directory: it pulls the code from the
repository.  The git branch pulled depends on `VER` or `BRANCH`
as described above.  Note that the following two commands produce
equivalent results:

	make VER=1.2.3
	make VER=1.2.3 BRANCH=r1.2.3

## Arguments

There are several arguments that can be passed in when building
the Docker Image.  To pass in arguments, use:

	make DOCKERFLAGS="--build-arg VARIABLE=VALUE"

These are mostly values that will be stored in the parameter
files in the resulting image.  They can all be overridden when
starting up a container (see below for instructions).  They
are inherited from the runtime administrative parameter files
on the host system on which the image is run.

* `GDP_ROUTER` — the default GDP router.  Inherited from the
  `swarm.gdp.routers` administrative parameter.
* `GDP_CREATION_SERVICE` — The GDPname of the default log creation
  service.  Inherited from `swarm.gdp.creation-service.name` if set;
  otherwise, there is no default for now.
* `GDP_HONGD_SERVER` — The IP hostname of the Human-Oriented Name
  to GDPname Directory server (actually the MariaDB server) where
  mappings may be accessed.  At the moment there is no default,
  but there probably should be until we come up with a scalable,
  federated solution.  In the meantime, there can be only one of
  these in any GDP cluster.  Inherited from `swarm.gdp.hongdb.host`.


# Running Docker Instances

## Running `gdp-dev-c` and `gdp-dev-python`

***To Be Written.***

## Running `gdplogd`

`gdplogd` has some special requirements.  In particular, it has to
have access to an external volume on which to store persistent data,
and must have a unique name.  The script `spawn-gdplogd.sh` tries
to set this up for you.

The philosophy behind `spawn-gdplogd.sh` is that the setup should
be essentially the same as it would be if `gdplogd` were not run in
a container.  For example, it examines the usual parameters files:

```
	/etc/ep_adm_params/gdp
        /etc/ep_adm_params/gdplogd
        /usr/local/etc/ep_adm_params/gdp
        /usr/local/etc/ep_adm_params/gdplogd
        $HOME/.ep_adm_params/gdp
        $HOME/.ep_adm_params/gdplogd
```

for parameters.  In particular, it uses the following parameters:

* `swarm.gdplogd.log.dir` is the Unix pathname of the directory
  holding persistent log data.
  Defaults to `/var/swarm/gdp/glogs`.
* `swarm.gdp.routers` sets the default set of routers.  If set,
  this will override the `GDP_ROUTER` parameter set when the image
  was built.
* `swarm.gdp.hongdb.host` overrides `GDP_HONGD_SERVER`.  It has
  no default.
* `swarm.gdplogd.gdpname` overrides `GDPLOGD_NAME`.  Defaults
  to the reversed name of the host on which the container is
  being run with `.gdplogd` appended.




# Everything from here down is old and may be inaccurate

Especially note that:

- There is no guaranteed for the safety of the data. This is experimental
  software, and more importantly, experimental configuration/setup
  procedure. You are on your own if you experience data loss.
- The docker commands are not error prone. In other words, don't just
  naively execute these commands without knowing what they do.
- These images pull from the repository and compile the code from scratch,
  which is a very inefficient way of creating docker images. It takes too
  long to build the images and the images are huge. But this is what it is
  for the moment, and needs to be fixed.
- The router image creates its single, standalone network. Ideally, one
  should be able to connect to other GDP networks. This needs to be fixed
  as well.
- For the moment, the router image is based on the `eric/net4` branch
  from the `gdp_router_click` repository. The `master` branch is what we
  would like in the long run, but that requires quite a bit of setup and
  is a little less mature.
- The log-server image stores data in a docker volume stored on the host
  and managed by docker. Make sure you are not naively running multiple
  containers attached to the same volume.

# Management via Docker

Here's one way to start your own environment. Make sure you understand what
you are doing, and don't just naively run things.

First, let's create a bridged network for our images. This enables isolation
and name resolution by referring to the container name.

    docker network create gdpnet

Let's run a router instance, which we call `gdprouter`. We expose the port
`8009` to outside.

    docker run -d --network=gdpnet --name gdprouter -p 8009:8009 router

Now, let's create a volume to host persistent data for our log-server. We
call the volume `logs`, and we will attach this volume to our running
container next.

    docker volume create logs

The following starts the log-server container. We pass the router that this
log server should connect to via the environment variable `GDP_ROUTER`,
which is simply the name of the container `gdprouter` we created earlier.
Since both contianers join the same network `gdpnet`, hostname resolution
works nicely. The name of the log daemon can be overridden by setting
`GDPLOGD_NAME` to appropriate value as well.


    docker run -d --network=gdpnet \
        -e GDP_ROUTER=gdprouter:8009 \
        -e GDPLOGD_NAME=docker.gdplogd \
        --name=logserver \
        --mount source=logs,destination=/var/swarm/gdp/glogs \
        gdplogd -D *=10

To stop the containers, we need two steps: (1) stop the container and
(2) remove the container so that the name becomes available for future
and that we accidentally do not end up in a situation where multiple
containers are all connected to the same volume. Here's how to stop
the containers we started above:

    docker stop logserver
    docker container rm logserver

Similarly for the router container

    docker stop gdprouter
    docker container rm gdprouter
