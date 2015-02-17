#!/usr/bin/env python

"""
A simple prgoram that reads keyboard input from stdin and logs it
in a log
"""

import sys
sys.path.append("../")
import gdp


def main(name_str):

    # Create a GDP_NAME object from a python string provided as argument
    gcl_name = gdp.GDP_NAME(name_str)

    # There's a GCL with the given name, so let's open it
    gcl_handle = gdp.GDP_GCL(gcl_name, gdp.GDP_MODE_AO)

    while True:

        line = sys.stdin.readline().strip()  # read from stdin
        # Create a minimalist datum dictionary
        datum = {"data": line}
        gcl_handle.append(datum)           # Write this datum to the GCL


if __name__ == "__main__":

    if len(sys.argv) < 2:
        print "Usage: %s <gcl_name>" % sys.argv[0]
        sys.exit(1)

    # Change this to point to a gdp_router
    gdp.gdp_init("127.0.0.1", 8007)
    main(sys.argv[1])
