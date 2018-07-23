#!/usr/bin/env sh

set -e

NUM_PROCS=`cat /proc/cpuinfo  | grep "^processor" | wc -l`
NUM_JOBS=$((NUM_PROCS*2))

apt-get update
apt-get install -y libdb5.3-dev

# this needs overhaul -- not a big fan of the process at the moment
git clone git://repo.eecs.berkeley.edu/projects/swarmlab/gdp_router_click.git
(cd gdp_router_click && \
    git checkout eric/net4 && \
    sed -i 's/-lgdp/-lgdp -luuid/g' Makefile && \
    make -j $NUM_JOBS gdp-router-click)
