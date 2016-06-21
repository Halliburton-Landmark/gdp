#!/bin/sh
# Check for gdplogd and gdp_router first, then create directories,
# then run the script named as an argument, then kill the daemons.

# To use this script, create a script that runs the local tests that takes
# a logName as an argument.

# Process arguments

if [ $# -lt 1 ]; then
    echo "$0: Usage: $0 runScript [args]"
fi

runScript=$1
if [ ! -x $runScript ]; then
    echo "$0: $runScript is not present or not executable"
fi

# Exit if we use a previously unset variable.
set -u

# See if the gdplogd directory is present.
grandparentDirectory=`dirname $0`/../..
sourceDirectory=`cd $grandparentDirectory; pwd`

gdplogd=$sourceDirectory/gdp/gdplogd/gdplogd

if [ ! -x "$gdplogd" ]; then
    echo "$0: $gdplogd was either not found or is not executable.  Exiting."
    exit 2
fi

######
# If necessary, check out the gdp_router directory.

gdpRouterSource=$sourceDirectory/gdp_router

if [ ! -d "$gdpRouterSource" ]; then
   echo "#### $0: Checking out the gdp_router repo and create $gdpRouterSource."
   mkdir -p `dirname $gdpRouterSource`
   (cd `dirname "$gdpRouterSource"`; git clone https://repo.eecs.berkeley.edu/git/projects/swarmlab/gdp_router.git)
else
    echo "#### $0: Running git pull in $gdpRouterSource"
    (cd "$gdpRouterSource"; git pull)
fi
    
if [ ! -d "${gdpRouterSource}" ]; then
    echo "$0: Could not create ${gdpRouterSource}. Exiting"
    exit 3
fi


######
# Create the ep_adm_params directory.
# When running, we set the EP_PARAM_PATH variable.

EP_ADM_PARAMS=$sourceDirectory/ep_adm_params

if [ ! -d "$EP_ADM_PARAMS" ]; then
   mkdir -p "$EP_ADM_PARAMS"
fi

export EP_ADM_PARAMS=`cd $EP_ADM_PARAMS; pwd`

echo "swarm.gdp.routers=localhost" > "$EP_ADM_PARAMS/gdp"

######
# The default directory is /var/swarm/gdp/gcls, which must be created
# by root, but owned by the user that runs the gdplogd process.
# Instead, we use a separate log directory

GCLS=$sourceDirectory/gcls

echo "#### $0: Removing $GCLS and then recreating it."
rm -rf $GCLS
mkdir -p "$GCLS"

echo "swarm.gdplogd.gcl.dir=$GCLS" > "$EP_ADM_PARAMS/gdplogd"

echo "#### $0: Set up $EP_ADM_PARAMS"
echo " To run with these settings from the command line, use:"
echo "    export EP_ADM_PARAMS=$EP_ADM_PARAMS"

echo "#### $0: Starting gdp_router"
# pkill -f matches against the full argument list.
pkill -u $USER -f python ./src/gdp_router.py
startingDirectory=`pwd`
echo "Command to start gdp_router (cd $gdpRouterSource; python ./src/gdp_router.py -l routerLog.txt) &"
cd "$gdpRouterSource"
python ./src/gdp_router.py -l routerLog.txt &
gdp_routerPid=$!

sleep 2

echo "#### $0: Starting gdplogd"
echo "Command to start gdplogd: $gdplogd -F -N `hostname` &"
cd "$startingDirectory"
pkill -u $USER gdplogd
$gdplogd -F -N `hostname` &
gdplogdPid=$!

trap "kill $gdplogdPid $gdp_routerPid; pkill -9 -u $USER $gdplogdPid" INT

sleep 2

cd "$startingDirectory"
logName=gdp.runTests.$RANDOM
echo "#### Creating log $logName"
echo "Command to create a log: $sourceDirectory/gdp/apps/gcl-create -k none -s `hostname` $logName"
$sourceDirectory/gdp/apps/gcl-create -k none -s `hostname` $logName
returnValue=$?
if [ $returnValue -eq 73 ]; then
    echo "$0: Error: Log $logName already existed?"
fi

# Run all the tests!
echo "$@ $logName"
$@ $logName
overallReturnValue=$?

echo "#### $0: Stopping gdplogd and gdp_router"
kill $gdplogdPid $gdp_routerPid
pkill -9 -u $USER $gdplogdPid

exit $overallReturnValue

