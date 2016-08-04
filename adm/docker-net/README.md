Creates a private test network for testing GDP-routers and log servers
without disrupting the actual production network.
Currently, it supports a very simple topology of `N` routers (all connected
to each other) and `M` log servers connected to each router (total `MxN` log 
servers).

Hopefully this will be of use for testing and any future upgrades
of the GDP protocol.

Files:
======

- Dockerfile.baseimage : A base image with pre-requisite packages
- Dockerfile.gdp-router: A recipe for generating gdp-router docker image
- Dockerfile.gdplogd   : A recipe for generating gdplogd docker image
- Makefile             : Wrapper script to start up everything

How to use:
===========

To get started, adjust parameters `NUM_ROUTERS` and `NUM_LOGDS_PER_ROUTER` in
makefile, run `make`. To shut down the circus, use `make clean`. If you want 
to get rid of the base docker image (because it's too old with outdated
packages, or you changed some dependency), use `make clean-all`.

You can also pass additional commandline arguments by adjusting appropriate
`*CMDLINE_ARGS` parameter in the Makefile. But before modifying it, first check
what are the current parameters being passed. The way arguments works is that 
docker executes the executable specified in Dockerfile under ENTRYPOINT with 
the specified arguments. Any addition to the actual `docker run ...` command 
after the image name are passed to the executable on container startup. Treat
`*CMDLINE_ARGS` as an easy way to specify additional parameters, but make sure
you don't cause conflicts with what is already being passed.

Some basic docker commands:

    docker images
>   shows the images available locally. You should be able to see 'gdp-router'
    and 'gdp-logd' after executing `make images`

    docker ps
>   shows currently running docker containers

    docker ps -a
>   shows all docker containers

    docker logs <container-name>
>   Displays the console output of a specified container.
    Pass argument `-f` for equivalent behavior as `tail -f`
    container-names are found using `docker ps`, they are of the form 
    `gdp-router_172.30.0.*` or `gdp-logd_172.30.*.*.` 

    docker kill <container-name>
>   Kill a running container (does not remove the container files from disk)

    docker rm <container-name>
>   Remove a container, frees up the name too.


Notes:
=====

- As of version 1.7, docker lacks the ability to specify the IP address of a 
  container by the user. We need to know the IP addresses of all the router
  instances in advance, thus functionality of pre-determined IP addresses is a 
  necessity.
  To get around this, we use `pipework` - a 3rd party shell script that creates
  it's own bridge. Launching a docker container becomes a two step process: 
  - Launch a docker container from a given image
  - Use pipework to attach another virual network interface in the container
    just launched, which is connected to a separate virual bridge.

- The convention used for IP address allocation is: 
  - Host has IP address 172.30.0.255
  - GDP routers have IP addresses: 172.30.0.x (x=> 1-254)
  - GDP log servers attached to 172.30.0.x have IP addresses of the form
    172.30.y.x (y=> 1-15). This can be increased, if needed.

- GDP routers include a random delay before starting, this is to make sure that
  we can start multiple instances of GDP routers with the same command line
  arguments without running into race conditions. GDP log servers are executed
  using a wrapper script that adds a delay of 10 seconds to accommodate for the
  delay in GDP router startup. See dockerfile.

