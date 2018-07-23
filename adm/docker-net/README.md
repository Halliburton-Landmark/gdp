**Note: This is for V2. Eventually, the V2 distinction should be dropped.**

# Introduction

This directory contains a very basic setup for

- Creating gdp-router docker image from scratch
- Creating log-server docker image from scratch
- A basic set of docker commands to set up a local test environment.

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

# Image creation.

With the note about large images and slow image creation in mind, you
can run the following:

    make

You should now have two images: one called `router` and antoher called
`gdplogd`. You can check it via `docker image ls`.

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

# Using the infrastructure

Once these docker containers are up and running, you should point your
client applications to this infrastructure by means of appropriate
configuration. Typically, this means setting the router and default
log server parameters in your `~/.ep_adm_params/gdp` (or similar)
configuration file. For the setup above, you probably want the following:

    swarm.gdp.routers=HOSTNAME_OR_IP_ADDRESS_OF_DOCKER_HOST
    swarm.gdp.gdp-create.server=docker.gdplogd
