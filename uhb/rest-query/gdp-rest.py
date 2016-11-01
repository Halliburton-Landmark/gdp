#!/usr/bin/env python

from twisted.internet import reactor
from twisted.web.resource import Resource
from twisted.web.server import Site

import argparse
import json
from GDPcache import GDPcache
import gdp


class GDPResource(Resource):

    isLeaf = True

    def __init__(self, logname):

        Resource.__init__(self)

        self.logname = logname
        self.gdpcache = GDPcache(logname)


    def render_GET(self, request):
        print "Received GET request:", request.uri, request.args

        try:
            start = float(request.args['start'][0])
            end = float(request.args['end'][0])
        except (KeyError, ValueError) as e:
            request.setResponseCode(500)
            return "Did you pass a valid start and end time?\n"

        try:
            data = self.gdpcache.getRange(start, end)

            __filtered = []
            for d in data:
                tmp = { 'recno': d['recno'],
                        'ts': (d["ts"]["tv_sec"] +
                                (d["ts"]["tv_nsec"]*1.0)/(10**9)),
                        'data': d['data']
                      }
                __filtered.append(tmp)

            # ideally, we should do some error handling for encoding/
            #   decoding related execptions.
            return json.dumps(__filtered)
        except gdp.MISC.EP_STAT_Exception as e:
            return str(e)



if __name__ == "__main__":

    # Set up argument parsing code
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port", type=int, default=8080,
                            help="TCP port to serve requests on, default=8080")
    parser.add_argument("logname", help="Name of log")

    # Get the actual arguments
    args = parser.parse_args()

    # Create a 'site' object to serve requests on 
    site = Site(GDPResource(args.logname))
    # Setup the reactor with the port
    reactor.listenTCP(args.port, site)
    print "Starting REST interface on port %d, fetching data from log %s" % \
                (args.port, args.logname)
    reactor.run()
