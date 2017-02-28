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
A simple program to demonstrate subscriptions
"""
from __future__ import print_function

import sys
sys.path.append("../")
import gdp


def main(name_str):

    # create a python object
    gcl_name = gdp.GDP_NAME(name_str)

    # Assume that the GCL already exists
    gcl_handle = gdp.GDP_GCL(gcl_name, gdp.GDP_MODE_RO)

    # this is the actual subscribe call
    gcl_handle.subscribe(0, 0, None)

    while True:

        # This blocks, until there is a new event
        event = gdp.GDP_GCL.get_next_event(None)
        datum = event["datum"]
        handle = event["gcl_handle"]
        print(datum)

if __name__ == "__main__":

    if len(sys.argv) < 2:
        print("Usage: %s <gcl-name>" % sys.argv[0])
        sys.exit(1)

    # Change this to point to a gdp_router
    gdp.gdp_init()
    main(sys.argv[1])
