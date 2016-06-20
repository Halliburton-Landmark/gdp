#!/bin/sh
# Script to run the gdp tests

if [ $# -ne 1 ]; then
    echo "$0: Usage: $0 logName"
fi
logName=$1

# Run the tests

failedTests=""
overallReturnValue=0

echo "#### ./t_fwd_append $logName `hostname`"
./t_fwd_append $logName `hostname`
returnValue=$?
if [ $returnValue != 0 ]; then
    failedTests="./t_fwd_append"
    overallReturnValue=$returnValue
fi


echo "#### Creating log x00, it is ok if this log already exists"
../apps/gcl-create -k none -s `hostname` x00
returnValue=$?
if [ $returnValue -eq 73 ]; then
    echo "#### $0: Ignore the warning above, the x00 log already existed."
fi

echo "#### ./t_multimultiread"
./t_multimultiread -D '*=20'
returnValue=$?
if [ $returnValue != 0 ]; then
    failedTests="$failedTests ./t_multimultiread"
    overallReturnValue=$returnValue
fi

if [ $overallReturnValue != 0 ]; then
    echo "$0: Failed test(s): $failedTests"
    exit $overallReturnValue
fi

# setupAndRun.sh will stop the daemons
