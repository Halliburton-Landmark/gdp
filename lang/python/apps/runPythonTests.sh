#!/bin/sh
# Run the Python tests.
# Usage: runPythonTests.sh logName
#
# An alternative way is to run gdp/test/setupAndRun.sh, which will start the daemons:
#   ../../../test/setupAndRun.sh ./runPythonTests.sh

if [ $# -ne 1 ]; then
    echo "$0: Usage: $0 logName"
fi
logName=$1

# Run the tests

failedTests=""
overallReturnValue=0

function runTest () {
    echo "#### $@"
    $@
    returnValue=$?
    if [ $returnValue != 0 ]; then
        failedTests="$failedTests $1"
        overallReturnValue=$returnValue
    fi
}

runTest ./rw_test.py $logName


if [ $overallReturnValue != 0 ]; then
    exit $overallReturnValue
fi

# gdp/test/setupAndRun.sh will stop the daemons
 
