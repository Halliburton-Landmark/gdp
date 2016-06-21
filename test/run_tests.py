#!/usr/bin/env python

# ----- BEGIN LICENSE BLOCK -----
#	GDP: Global Data Plane
#	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
#
#	Copyright (c) 2016, Regents of the University of California.
#	All rights reserved.
#
#	Permission is hereby granted, without written agreement and without
#	license or royalty fees, to use, copy, modify, and distribute this
#	software and its documentation for any purpose, provided that the above
#	copyright notice and the following two paragraphs appear in all copies
#	of this software.
#
#	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
#	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
#	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
#	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
#	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
#	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
#	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
#	OR MODIFICATIONS.
# ----- END LICENSE BLOCK -----

"""
Invoke the executables in this directory using Python 
so that we can get the output in JUnit compatible xml.

To start up the gdp_router, gdplogd and then run the tests:

./setupAndRun.sh ./_internalRunPythonTests.sh run_tests.py 

To create JUnit-compatible output:

./setupAndRun.sh ./_internalRunPythonTests.sh run_tests.py --junitxml=../reports/junit/gdpTest.xml

If the daemons are already running, then to create a log and run the tests, use:

export logName=gdp.test.newLog.$RANDOM
../apps/gcl-create -k none -s `hostname` $logName
py.test --logName=$logName run_tests.py


"""

import socket
import subprocess

# Parsing the logName command line argument is set up in conftest.py.

def test_t_fwd_append(logName):
    subprocess.call(["./t_fwd_append", logName, socket.gethostname()])

def test_t_multimultiread(logName):
    # Create the x00 log
    subprocess.call(["../apps/gcl-create", "-k", "none", "-s", socket.gethostname(), "x00"]);
    subprocess.call(["./t_multimultiread"])
