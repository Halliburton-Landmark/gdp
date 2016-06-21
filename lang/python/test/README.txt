To start up the gdp_router and gdplogd and then run the tests, use:

  ./run.sh

That script invokes the ../../../test/setupAndRun.sh script, which in turn
invokes _internalRunPythonTests.sh


If the daemons are already running, then to create a log and run the tests, use:

  export newLog=gdp.runPythonTests.newLog.$RANDOM
  ../../../apps/gcl-create -k none -s ealmac23.local $newLog
  py.test --logName=$newLog

