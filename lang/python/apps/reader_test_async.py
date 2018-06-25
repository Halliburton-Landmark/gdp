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
A simple program to demonstrate asynchronous reads and associated
event handling.
"""

import sys
sys.path.append("../")
import gdp


def main(name_str, start, stop):

    # create a python object
    _name = gdp.GDP_NAME(name_str)

    # Assume that the GCL already exists
    gin_handle = gdp.GDP_GIN(_name, gdp.GDP_MODE_RO)

    # this is the actual subscribe call
    gin_handle.read_by_recno_async(start, stop-start+1)

    # timeout
    t = {'tv_sec':0, 'tv_nsec':500*(10**6), 'tv_accuracy':0.0}

    while True:

        # This could return a None, after the specified timeout
        event = gin_handle.get_next_event(t)
        if event is None or event["type"] == gdp.GDP_EVENT_DONE:
            break
        datum = event["datum"]
        assert event["gin"] == gin_handle       ## sanity check
        print datum["buf"].peek()

if __name__ == "__main__":

    if len(sys.argv) < 4:
        print "Usage: %s <name> <start-recno> <stop-recno>" % sys.argv[0]
        sys.exit(1)

    gdp.gdp_init()
    main(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]))
