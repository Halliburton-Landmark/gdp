#!/usr/bin/env python
# ----- BEGIN LICENSE BLOCK -----
#	GDP: Global Data Plane
#	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
#
#	Copyright (c) 2015, Regents of the University of California.
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
An example program that logs some system information periodically.
This is just an example program that can be used as a starting point.

** Requires python-psutil to be installed **
"""

import sys
sys.path.append("../")
import gdp
import psutil
import time
import os
import json

SLEEP_INTERVAL = 60

def main(name_str, keyfile, hostname):

    skey = gdp.EP_CRYPTO_KEY(filename=keyfile,                                  
                                keyform=gdp.EP_CRYPTO_KEYFORM_PEM,              
                                flags=gdp.EP_CRYPTO_F_SECRET)                   

    # Create a GDP_NAME object from a python string provided as argument
    gcl_name = gdp.GDP_NAME(name_str)

    # There's a GCL with the given name, so let's open it
    gcl_handle = gdp.GDP_GCL(gcl_name, gdp.GDP_MODE_AO,
                                open_info={'skey':skey})

    while True:

        # Collect information that we are going to log
        d = {}
        d['host'] = hostname
        l = os.getloadavg()
        d['load1'], d['load5'], d['load15'] = l[0], l[1], l[2]
        d['freeram'] = psutil.phymem_usage().free
        d['nprocs'] = len(psutil.get_pid_list())

        # convert that to a nice string
        string_to_write = json.dumps(d)

        # Create a minimalist datum dictionary
        datum = {"data": string_to_write}
        gcl_handle.append(datum)           # Write this datum to the GCL
        time.sleep(SLEEP_INTERVAL)


if __name__ == "__main__":

    if len(sys.argv) < 4:
        print "Usage: %s <hostname> <gcl_name> <signing-keyfile>" % sys.argv[0]
        sys.exit(1)

    # Change this to point to a gdp_router
    gdp.gdp_init()
    main(sys.argv[2], sys.argv[3], sys.argv[1])
