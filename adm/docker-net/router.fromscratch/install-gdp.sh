#!/usr/bin/env sh

set -e

PACKAGES="git libevent-dev libevent-pthreads-2.0-5 libsqlite3-dev libssl-dev uuid-dev libjansson-dev protobuf-c-compiler libavahi-client-dev libsystemd-dev" 

apt-get update
apt-get install -y $PACKAGES

git clone git://repo.eecs.berkeley.edu/projects/swarmlab/gdp.git
(cd gdp && make install-client)

rm -rf gdp
apt-get clean
rm -rf /var/lib/apt/lists/*
